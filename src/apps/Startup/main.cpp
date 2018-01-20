#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <mm_malloc.h>

#include <share/cpp/PerfomanceTime.cpp>
#include <share/cpp/BindCpu.cpp>
#include <share/cpp/WinApi.cpp>
#include <share/cpp/Common.cpp>
#include <share/cpp/LargePageSharedMemory.cpp>

#include <share/c/worker_api.c>
#include <share/c/workers_context.c>
#include <share/c/constants.c>

#include "UdpLocalClient.cpp"

#include "commands.c"

//#include "yescrypt/yescrypt.c"

#define WRAPPER_PATH_X86	"./wrapper.bin"
#define WRAPPER_PATH_X64	"./wrapper.bin"

#define ZERO_MEMORY(e)	\
	{	\
		uint8_t *s = (uint8_t *)&(e);	\
		size_t size = sizeof(e);	\
		while(size--) {	\
			*s = 0; s++;	\
		}	\
	}
#define COPY_MEMORY(dst, src) \
{	\
	uint8_t *udst = (uint8_t *)&(dst);	\
	uint8_t *usrc = (uint8_t *)&(src);	\
	for(int i = 0; i < sizeof(src); i++) {	\
		*udst = *usrc;	\
		udst++; usrc++;	\
	}	\
}
#define INIT_RAND_MEMORY(e)	\
	initRandMemory((char*)&(e), sizeof(e));
#define DUMP_MEMORY(e) \
	dump_memory((uint8_t*)&(e), sizeof(e));

void logFileError(std::string error) {
	//Common::textFileAppend(".startup-error-list", error + "\r\n");
}
	
std::string dirname(const std::string& fname) {
     size_t pos = fname.find_last_of("\\/");
     return (std::string::npos == pos) ? "./" : fname.substr(0, pos + 1);
}
std::string nameOfPath(const std::string& fname) {
     size_t pos = fname.find_last_of("\\/");
     return (std::string::npos == pos) ? fname : fname.substr(pos + 1);
}
	
	class NewProcessSingleWorker {
		private:
			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			
			bool alive = false;
		
			void exit() {
				if ( this->alive ) {
					TerminateProcess(this->pi.hProcess, NO_ERROR);
					CloseHandle(this->pi.hProcess);
					CloseHandle(this->pi.hThread);				
				}
				
				this->alive = false;
			}
		public:
			HANDLE hPipe;

			NewProcessSingleWorker() {
			}
			
			void create(const std::string worker_path, const std::string memory_id, uint64_t id) {
				this->alive = false;
				
				this->hPipe = INVALID_HANDLE_VALUE;
								
				ZeroMemory(&this->si,sizeof(STARTUPINFO));
				this->si.cb = sizeof(STARTUPINFO);
				this->si.dwFlags = 0;
			 
				ZeroMemory(&this->pi,sizeof(PROCESS_INFORMATION));

				std::string _worker_path = "app_wrapper " + std::to_string(GetCurrentProcessId()) + " " + memory_id + " " + std::to_string(id);
				
				char *pworker_path = (char*)malloc(worker_path.length() + 1);
				memcpy(pworker_path, worker_path.c_str(), worker_path.length() + 1);
				
				char *pworker_cmd = (char*)malloc(_worker_path.length() + 1);
				memcpy(pworker_cmd, _worker_path.c_str(), _worker_path.length() + 1);
				
				//	CREATE_NEW_CONSOLE	|	CREATE_NO_WINDOW
				if ( !CreateProcess(pworker_path, pworker_cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &this->si, &this->pi) ) {
					free(pworker_path);
					free(pworker_cmd);
					throw "Can not create process";
				}
				
				this->alive = true;
				
				free(pworker_path);	
				free(pworker_cmd);			
			}
			~NewProcessSingleWorker() {
				this->exit();
			}

			HANDLE getProcess() {
				return this->pi.hProcess;
			}
			
			bool isAlive() {
				if ( !this->alive ) {
					return false;
				}
				
				if ( !Common::isProcessAlive(this->pi.hProcess) ) {
					this->exit();
					return false;
				}
				
				return true;
			}
			
			void tryAlive() {
				if ( !this->isAlive() ) {
					throw "Process is closed";
				}
			}
	};

class WorkerMng {
	public:
		bool runned = false;
		hash_function_t hashFun;
		std::string wrapperPath;
		
		uint32_t maxCpuCount;
		
		volatile uint32_t threadSeq = 0;
		
		sv_startup_info_t startup;
		uint32_t worker_count;
		
		std::string gbWorkersContextMemoryName;
		global_workers_context_t *gbWorkersContext;
		
		NewProcessSingleWorker process1;
		NewProcessSingleWorker process2;
		bool process2_enable = false;
	public:
		void start(sv_startup_info_t &startup, const std::string wrapperPath) {
			this->startup = startup;
			this->wrapperPath = wrapperPath;
		
			this->maxCpuCount = Cpu::getProcessorLogicalCount();
			
			this->startThreads();
		}
		
		bool startThreads() {

			this->gbWorkersContext = (global_workers_context_t*)Common::createSharedMemory(this->gbWorkersContextMemoryName, sizeof(*this->gbWorkersContext));
			if ( !this->gbWorkersContext ) {
				throw "No memory for global_workers_context";
			}
			
			ZERO_MEMORY(*this->gbWorkersContext)
			
			if ( this->startup.thread_count > Cpu::getProcessorLogicalCount() ) { this->startup.thread_count = Cpu::getProcessorLogicalCount(); }
			
			int p1len = this->startup.thread_count;
			int p2len = 0;

			for(int i = 0; i < this->startup.thread_count; i++) {
				gbWorkersContext->workers_init[i].thread_priority = this->startup.thread_priority;
			}
			
			if ( this->startup.thread_count == Cpu::getProcessorLogicalCount() ) {
				p1len--;
				p2len++;
				gbWorkersContext->workers_init[p1len].thread_priority  = THR_PRIO_NORMAL;
			}
	
			gbWorkersContext->wrappers_init[0].process_priority = this->startup.process_priority;
			gbWorkersContext->wrappers_init[0].index = 0;
			gbWorkersContext->wrappers_init[0].len   = p1len;
			
			gbWorkersContext->wrappers_init[1].process_priority = PROC_PRIO_NORMAL;
			gbWorkersContext->wrappers_init[1].index = p1len;
			gbWorkersContext->wrappers_init[1].len   = p2len;
			
			std::string worker_file_name = nameOfPath((char*)&this->startup.worker_path);
			memcpy(&gbWorkersContext->worker_path, worker_file_name.c_str(), worker_file_name.length());
			
			gbWorkersContext->worker_count = this->startup.thread_count;
			this->worker_count = this->startup.thread_count;
			
			this->process1.create(this->wrapperPath, this->gbWorkersContextMemoryName, 0);
			if ( p2len ) {
				this->process2_enable = true;
				this->process2.create(this->wrapperPath, this->gbWorkersContextMemoryName, 1);
			}
		}
		
		bool tryAlive() {
			this->process1.tryAlive();
			if ( this->process2_enable ) {
				this->process2.tryAlive();
			}
		}
		
		bool checkComplected() {
			for(int i = 0; i < this->startup.thread_count; i++) {
				if ( !this->gbWorkersContext->workers_responce[i].is_ready ) {
					return false;
				}
			}
			
			return true;
		}
		
		void getStat(cl_info_t *info) {
			uint64_t hash_count = 0;
			uint64_t share_count = 0;
			uint64_t min_delta_counter_hash = 0xFFFFFFFFFFFFFFFF;
			
			for(int i = 0; i < startup.thread_count; i++) {
				worker_responce_context_t *wrc = &gbWorkersContext->workers_responce[i];
				
				if ( wrc->min_delta_counter_hash < min_delta_counter_hash ) {
					min_delta_counter_hash = wrc->min_delta_counter_hash;
				}
				
				hash_count += wrc->hash_count;
				share_count += wrc->share_count;
			}
			
			uint32_t min_delta_micro_sec_hash = 0xFFFFFFFF;
			if ( min_delta_counter_hash != 0xFFFFFFFFFFFFFFFF ) {
				min_delta_micro_sec_hash = std::round(PerfomanceTime::i().counterToMicroSecDouble(min_delta_counter_hash));
			}
			
			info->hash_count = hash_count;
			info->share_count = share_count;
			info->min_delta_micro_sec_hash = min_delta_micro_sec_hash;
			info->time_mili_sec = PerfomanceTime::i().getMiliSec();
		}
		
		
		bool getShare(share_context_t *ret_share) {
			if ( gbWorkersContext->shareSeqRead == gbWorkersContext->shareSeqWrite ) {
				return false;
			}
			
			share_worker_t *share = &gbWorkersContext->shares[ WORKERSCTX_ENT_GET_INDEX(gbWorkersContext->shareSeqRead) ];
			while(!share->is_ready) {
				Sleep(1);
			}
			
			*ret_share = share->share;
			
			share->is_ready = false;
			
			gbWorkersContext->shareSeqRead++;
			
			return true;
		}
		void setJob(job_context_t *job) {
			gbWorkersContext->jobs[ WORKERSCTX_ENT_GET_INDEX(gbWorkersContext->jobSeq + 1) ].job = *job;
			for(int i = 0; i < 1000; i++) {}
			__sync_fetch_and_add(&gbWorkersContext->jobSeq, 1);
		}
	
		
	
};

class Control {
	#define CL_INIT_CMD_DATA(_cmd)	\
		client_command_t cl_data;	\
		cl_data.cmd = _cmd;
		
	#define CL_SEND(name)	\
		this->send(cl_data, sizeof(cl_data. name));
	
	
	#define SV_TRY_CMD(name)	\
		if ( size != 4 + sizeof(sv_data. name) ) { throw "Error: try server command " #name "; Size !== sizeof(command)"; }
	
	private:
		UdpLocalClient ulc;
		WorkerMng workerMng;
		bool workers_ready = false;
	public:
		Control() {
			
		}
		
		bool start(uint16_t port) {
			if ( !ulc.connect(port) ) {
				throw "Error: udp local client connect";
			}
			
			this->cmd_hello();
			
			if ( !WinApi::init() ) {
				this->cmd_error(WinApi::error_text.c_str());
				throw "Error: WinApi::init";
			}
			
			this->loop();
		}
		
		void cmd_error(std::string text) {
			CL_INIT_CMD_DATA(CL_CMD_ERROR)
			
			int sz = (text.length() > 500) ? 500 : text.length();
			memcpy(&cl_data.error.text, text.c_str(), sz);
			cl_data.error.text[sz] = 0;
			this->send(cl_data, sz);
		}
		void cmd_hello() {
			CL_INIT_CMD_DATA(CL_CMD_HELLO)
			this->send(cl_data);
		}
		void cmd_pong(uint32_t id) {
			CL_INIT_CMD_DATA(CL_CMD_PONG)
			cl_data.pong.id = id;
			CL_SEND(pong)
		}
		
		void loop() {
			while(1) {
				this->netProcess();
				Sleep(1);
			}
		}
		void loopWork() {
			Common::TimeInterval ti_updateInfo(1000);
			
			while(1) {
				this->netProcess();
				
				workerMng.tryAlive();
				
				share_context_t share;
				if ( workerMng.getShare(&share) ) {
					CL_INIT_CMD_DATA(CL_CMD_SHARE)
					COPY_MEMORY(cl_data.share.job_id, share.job_id)
					COPY_MEMORY(cl_data.share.nonce , share.nonce)
					COPY_MEMORY(cl_data.share.hash  , share.hash)
					CL_SEND(share)
				}
				
				if ( ti_updateInfo.frame() ) {
					CL_INIT_CMD_DATA(CL_CMD_INFO)
					workerMng.getStat(&cl_data.info);
					CL_SEND(info)
				}
				
				Sleep(1);
			}
		}
		
		void netProcess() {
			uint32_t size;
			server_command_t sv_data;
			
			do {
					
				if ( !this->ulc.recv((uint8_t *)&sv_data, &size) ) {
					throw "Error: udp local client recv";
				}
					
				if ( size ) {
					if ( size < 4 ) { throw "Error: server send invalid data"; }
					
					this->parseServerCommand(sv_data, size);
				}
					
			} while(size);			
		}
		
		void parseServerCommand(server_command_t &sv_data, uint32_t size) {
			switch(sv_data.cmd) {
				case SV_CMD_PING:
					SV_TRY_CMD(ping)
					this->cmd_pong(sv_data.ping.id);
					return;
			}
			
			if ( !this->workers_ready ) {
				
				switch(sv_data.cmd) {					
					case SV_CMD_STARTUP_INFO:
						SV_TRY_CMD(startup_info)
						this->startupInfo(sv_data.startup_info);
						return;
				}
				
			} else {
				
				switch(sv_data.cmd) {
					case SV_CMD_JOB:
						SV_TRY_CMD(job)
						job_context_t job;
						COPY_MEMORY(job.job_id, sv_data.job.job_id);
						COPY_MEMORY(job.blob  , sv_data.job.blob);
						COPY_MEMORY(job.target, sv_data.job.target);
						this->workerMng.setJob(&job);
						return;
				}				
				
			}
			
			//printf("Unexpected command cmd: %i \n", sv_data.cmd);
			throw "Unexpected command";
		}
		
		void startupInfo(sv_startup_info_t &startup_info) {
			startup_info.worker_path[ sizeof(startup_info.worker_path) - 1 ] = 0;
			char *workerPath = (char *)&startup_info.worker_path[0];
			
			workerMng.start(startup_info, dirname(workerPath) + (startup_info.worker_x64 ? WRAPPER_PATH_X64 : WRAPPER_PATH_X86));
			
			Common::TimeInterval ti(2000);
			while(1) {
				workerMng.tryAlive();
				if ( workerMng.checkComplected() ) {
					break;
				}
				
				if ( ti.frame() ) {
					throw "Start workers error. timeout";
				}
				
				Sleep(10);
			}
		
			this->cl_update_workers_state();
			
			this->workers_ready = true;
			this->loopWork();
		}
		
		void cl_update_workers_state() {
			CL_INIT_CMD_DATA(CL_CMD_WORKER_INFO)
			for(int i = 0; i < workerMng.worker_count; i++) {
				worker_responce_context_t *wrc = &workerMng.gbWorkersContext->workers_responce[i];
				
				cl_data.worker_info.process_priority        = wrc->process_priority;
				cl_data.worker_info.thread_priority         = wrc->thread_priority;
				cl_data.worker_info.processor_package_index = wrc->processor_package_index;
				cl_data.worker_info.processor_core_index    = wrc->processor_core_index;
				cl_data.worker_info.processor_logical_index = wrc->processor_logical_index;
				cl_data.worker_info.numa_node_index         = wrc->numa_node_index;
				cl_data.worker_info.large_page              = wrc->large_page;
				
				CL_SEND(worker_info)
			}
		}
		void send(client_command_t &cl_data, uint32_t size = 0) {
			if ( !this->ulc.send((uint8_t*)&cl_data, size + 4) ) {
				throw "Error: udp local client send";
			}
		}
};

void app(uint16_t port) {
	try {
		Control cnt;
		cnt.start(port);
	} catch(const char *e) {
		logFileError(std::string(e) + std::string("\n"));
		Common::terminateCurrentProcess();
	}
}

int main(int argc, char *argv[]) {
	if ( !WinApi::init() ) {
		logFileError(std::string("WinApi functions not found: ") + WinApi::error_text);
		return 0;
	}

	if ( argc < 2 ) {
		Common::terminateCurrentProcess();
	}
	
	uint16_t port;
	try {
		port = std::stoi(argv[1]);
	} catch(const std::invalid_argument& ia) {
		Common::terminateCurrentProcess();
	}
	
	app(port);
	
	Common::terminateCurrentProcess();
	return 0;
}