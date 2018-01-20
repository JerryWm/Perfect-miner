#include <stdint.h>
#include <stdlib.h>
#include <map>

#include <share/cpp/PerfomanceTime.cpp>
#include <share/cpp/BindCpu.cpp>
#include <share/cpp/WinApi.cpp>
#include <share/cpp/Common.cpp>

#define YESCRYPT_R16_GB_SHARED_MEMORY_NAME	("\\\\.\\pipe\\largePageMemoryBackgroundYescryptR16_h892osdlbmpqls")
#define YESCRYPT_R16_GB_SHARED_MEMORY_SIZE	(1024*1024*9)

namespace LargePageMemoryBackground {

	class SharedMemoryNumaItemWrapper {
		typedef struct {
			std::string name;
			uint64_t size;
			bool free;
		} numa_node_memory_t;

		typedef std::map<uint32_t, numa_node_memory_t> numa_node_t;

		private:
			numa_node_t list;
		
			uint32_t numaIndex;
			uint64_t allocSize;
		public:
			void setNuma(uint32_t numaIndex) {
				this->numaIndex = numaIndex;
			}
			void setSize(uint64_t allocSize) {
				this->allocSize = allocSize;
			}
		
			void reserve(uint32_t count) {
				while(this->list.size() < count) {
					if ( !this->addNodeNumaMemory() ) {
						return;
					}
				}
			}

			std::string alloc(bool addNew = true) {
				for(auto &val : this->list) {
					if ( val.second.free ) {
						//printf("Ret val.second.name: %s \n", val.second.name.c_str());
						val.second.free = false;
						return val.second.name;
					}
				}
				
				if ( addNew && this->addNodeNumaMemory() ) {
					return this->alloc(false);
				}
				
				return "";
			}
			
			bool free(std::string name) {
				for(auto &val : this->list) {
					if ( val.second.name == name ) {
						val.second.free = true;
						return true;
					}
				}
				
				return false;
			}
			
		private:
			bool addNodeNumaMemory() {
				std::string name;
				if ( Common::createSharedMemoryNuma(name, this->allocSize, this->numaIndex, true) ) {
					numa_node_memory_t nnm;
					nnm.name = name;
					nnm.size = this->allocSize;
					nnm.free = true;
					
					int sz = this->list.size();
					this->list[sz] = nnm;
					
					return true;
				}
				
				return false;
			}
	};

	class SharedMemoryNuma {
		private:
			uint64_t allocSize = 4096;
			
			std::map<uint32_t, SharedMemoryNumaItemWrapper> nodes;
		public:
			SharedMemoryNuma(uint64_t allocSize) {
				this->allocSize = allocSize;
				
				ULONG numaHighestNodeNumber;
				bool supportNuma = WinApi::GetNumaHighestNodeNumber(&numaHighestNodeNumber) && numaHighestNodeNumber;
				
				if ( !supportNuma ) {
					numaHighestNodeNumber = 0;
				}
				
				int numaNodeCount = numaHighestNodeNumber + 1;
				for(int i = 0; i < numaNodeCount; i++) {
					this->nodes[i].setNuma(i);
					this->nodes[i].setSize(this->allocSize);
				}
				
				int memCountForNumaNode = std::round(Cpu::getProcessorLogicalCount() / numaNodeCount) + 4;
				for(int j = 1; j <= memCountForNumaNode; j++) {
					for(auto &node : this->nodes) {
						node.second.reserve(j);
					}
				}
			}
			
			std::string alloc(uint32_t numaNodeIndex = 0) {
				if ( numaNodeIndex < this->nodes.size() ) {
					return this->nodes[numaNodeIndex].alloc();
				}
				
				return "";				
			}

			void free(std::string name) {
				for(auto &node : this->nodes) {
					node.second.free(name);
				}
			}

	};

	class SharedMemoryMng {
		typedef struct {
			HANDLE hProcess;
			std::string name;
		} item_t;
		
		private:
			std::map<HANDLE, std::string> list;

		public:
			SharedMemoryNuma smn;
			
			SharedMemoryMng(): smn(YESCRYPT_R16_GB_SHARED_MEMORY_SIZE) {
			}
		
			std::string getMemory(bool isNoCache, DWORD idProcess, DWORD idNumaNode = 0) {
				this->free();
				
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, idProcess);
				if ( hProcess == INVALID_HANDLE_VALUE ) {
					return "";
				}
	
				std::string name = smn.alloc(idNumaNode);
				if ( !name.length() ) {
					return "";
				}
				
				list[hProcess] = name;
				
				return name;
			}
		
			void free() {
				std::map<HANDLE, HANDLE> del_list;
				
				for(auto val : list) {
					if ( !Common::isProcessAlive(val.first) ) {
						CloseHandle(val.first);
						smn.free(val.second);
						
						del_list[val.first] = val.first;
					}
				}
				
				for(auto val : del_list) {
					list.erase(val.first);
				}
			}
	};
	
	
	typedef struct {
		uint64_t isNoCache;
		uint64_t idProcess;
		uint64_t idNumaNode;
	} query_shared_memory_t;

	bool sharedMemoryServer(const char *gbMemoryName, SharedMemoryMng &sharedMemoryMng) {
		HANDLE hNamedPipe = CreateNamedPipeA(
			gbMemoryName,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_WAIT,
			1,
			0, 0, 
			5000, 
			NULL
		);

		if ( hNamedPipe == INVALID_HANDLE_VALUE ) {
			return false;
		} else {
			while(1) {
				if ( ConnectNamedPipe(hNamedPipe, NULL) ) {
					query_shared_memory_t query;
					DWORD cbRead;
					DWORD cbWritten;

					if ( ReadFile(hNamedPipe, &query, sizeof(query), &cbRead, NULL) && cbRead == sizeof(query_shared_memory_t) ) {
						std::string name = sharedMemoryMng.getMemory(query.isNoCache, query.idProcess, query.idNumaNode);
						
						WriteFile(hNamedPipe, name.c_str(), name.length()+1, &cbWritten, NULL);
					}
					
					DisconnectNamedPipe(hNamedPipe);
				}
			}
		}
	}
	void *sharedMemoryClient(const char *gbMemoryName, uint32_t numaNodeIndex = 0, bool isNoCache = false) {
		query_shared_memory_t query;
		query.isNoCache = isNoCache;
		query.idProcess = GetCurrentProcessId();
		query.idNumaNode = numaNodeIndex;

		char recv_buf[4096];
		DWORD rBytes;
		
		void *ret = NULL;
		
		if ( CallNamedPipeA(
			gbMemoryName,
			(LPVOID)&query, sizeof(query),
			(LPVOID)&recv_buf, sizeof(recv_buf) - 1, &rBytes,
			5000
		) ) {
			recv_buf[rBytes] = '\0';
			ret = Common::openSharedMemory(&recv_buf[0]);
		}
	
		return ret;
	}
	
}