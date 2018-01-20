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
#include <share/c/job_context.c>
#include <share/c/constants.c>



void *yescryptAllocateMemory(uint32_t &numaNodeNumber, bool &isLargePage) {
	PROCESSOR_NUMBER procNumber;
	USHORT _numaNodeNumber;
	
	WinApi::GetCurrentProcessorNumberEx(&procNumber);
	if ( !WinApi::GetNumaProcessorNodeEx(&procNumber, &_numaNodeNumber) ) {
		_numaNodeNumber = (USHORT)NUMA_NO_PREFERRED_NODE;
	}
	numaNodeNumber = _numaNodeNumber;
	
	
	
	isLargePage = true;
	
	void *mem = LargePageMemoryBackground::sharedMemoryClient(YESCRYPT_R16_GB_SHARED_MEMORY_NAME, numaNodeNumber);
	if ( mem ) { return mem; }
	
	mem = Common::allocMemoryNuma(YESCRYPT_R16_GB_SHARED_MEMORY_SIZE, numaNodeNumber, true);
	if ( mem ) { return mem; }
	
	
	
	isLargePage = false;
	
	mem = Common::allocMemoryNuma(YESCRYPT_R16_GB_SHARED_MEMORY_SIZE, numaNodeNumber, false);
	if ( mem ) { return mem; }
	
	return _mm_malloc(YESCRYPT_R16_GB_SHARED_MEMORY_SIZE, 4096);
}
#define ZERO_MEMORY(e)	\
	{	\
		uint8_t *s = (uint8_t *)&(e);	\
		size_t size = sizeof(e);	\
		while(size--) {	\
			*s = 0; s++;	\
		}	\
	}

typedef struct {
	uint32_t init_id;
	uint32_t worker_id;
	hash_function_t hash_fun;
	void *memory;
	
	global_workers_context_t *gb_workers_ctx;
} local_worker_context_t;

class Worker {
	private:
		local_worker_context_t *lwc;
		worker_responce_context_t *wrc;
		hash_function_t hash_fun;
		global_workers_context_t *gbWorkersCtx;
		
		uint64_t jobSeqRead = 0;
	public:
		Worker(local_worker_context_t *lwc, worker_responce_context_t *wrc) {
			this->lwc = lwc;
			this->wrc = wrc;
			this->hash_fun = lwc->hash_fun;
			this->gbWorkersCtx = lwc->gb_workers_ctx;
		}
		
		void loop() {
			job_context_t *job = NULL;
			share_context_t share;
			job_context_t job_be;

			while(1) {
				if ( getJob(&job) ) {
					share.job_id = job->job_id;
					memToBe32(&job_be.blob[0], &job->blob[0], sizeof(job->blob));
					
					//Common::textFileAppend("wlogmem.txt", Common::Dev::dumpMemoryToString((uint8_t*)&job_be.blob[0], sizeof(job_be.blob)));
					//Common::textFileAppend("wlogmem-le.txt", Common::Dev::dumpMemoryToString((uint8_t*)&job->blob[0], sizeof(job->blob)));
					continue;
				}
				
				if ( !job ) {
					Sleep(1);
					continue;
				}
				
				share.nonce = __sync_fetch_and_add(&job->nonce, 1);
				be32enc(&job_be.nonce, share.nonce);
				
				hash(&job_be.blob[0], &share.hash[0]);
				
				if ( testShare(&share.hash[0], &job->target[0]) ) {
					submitShare(&share);
				}
			}
		}
		
		void hash(uint32_t *job_blob, uint64_t *share_blob) {
			uint64_t counterStart = PerfomanceTime::i().getCounter();
				
				hash_fun((void*)job_blob, (void*)share_blob, lwc->memory);
				
			uint64_t counterEnd = PerfomanceTime::i().getCounter();
			uint32_t counterDelta = counterEnd - counterStart;
			if ( counterDelta < wrc->min_delta_counter_hash ) {
				wrc->min_delta_counter_hash = counterDelta;
			}

			wrc->hash_count++;
		}
		
		bool testShare(uint64_t *share, uint64_t *target) {
			for(int i = 3; i >= 0; i--) {
				if ( share[i] < target[i] ) { return true; }
				if ( share[i] > target[i] ) { return false; }
			}
			
			return false;
		}

		
		static inline void be32enc(void *pp, uint32_t x) {
			uint8_t * p = (uint8_t *)pp;

			p[3] = x & 0xff;
			p[2] = (x >> 8) & 0xff;
			p[1] = (x >> 16) & 0xff;
			p[0] = (x >> 24) & 0xff;
		}		
		void memToBe32(uint32_t *dst, uint32_t *src, int size) {
			for(; size; size -=4) {
				be32enc((void *)dst, *src);
				dst++; src++;
			}
		}
		
		bool getJob(job_context_t **job) {
			uint64_t jobSeqTmp = __sync_fetch_and_add(&gbWorkersCtx->jobSeq, 0);
			if ( jobSeqRead == jobSeqTmp ) {
				return false;
			}
			//Common::textFileAppend("newjob.log.txt", "Append\n");
			jobSeqRead = jobSeqTmp;
			
			*job = &gbWorkersCtx->jobs[ WORKERSCTX_ENT_GET_INDEX(jobSeqRead) ].job;
			
			return true;
		}
		void submitShare(share_context_t *share) {
			uint64_t shareSeqWrite = __sync_fetch_and_add(&gbWorkersCtx->shareSeqWrite, 1);
			
			gbWorkersCtx->shares[ WORKERSCTX_ENT_GET_INDEX(shareSeqWrite) ].share = *share;
			
			gbWorkersCtx->shares[ WORKERSCTX_ENT_GET_INDEX(shareSeqWrite) ].is_ready = true;
			
			wrc->share_count++;
		}
};


WINAPI void threadEntyPoint(local_worker_context_t *lwc) {
	global_workers_context_t *gbWorkersCtx = lwc->gb_workers_ctx;
	
	SetThreadPriority(GetCurrentThread(), thrPrioToWinApi(gbWorkersCtx->workers_init[lwc->init_id].thread_priority));
	
	lwc->worker_id = __sync_fetch_and_add(&gbWorkersCtx->thread_seq, 1);

	Cpu::bind_processor_t bp;
	Cpu::getProcessorBindForSeq(lwc->worker_id, &bp);
	Cpu::setThreadProcessorAffinity(GetCurrentThread(), &bp);
	Sleep(20);
	
	uint32_t numaNodeNumber;
	bool isLargePage;
	lwc->memory = yescryptAllocateMemory(numaNodeNumber, isLargePage);
	if ( !lwc->memory ) {
		Common::terminateCurrentProcess();
	}
	
	worker_responce_context_t *wrc = &gbWorkersCtx->workers_responce[lwc->worker_id];
	
	wrc->process_priority        = procPrioOfWinApi(GetPriorityClass(GetCurrentProcess()));
	wrc->thread_priority         = thrPrioOfWinApi(GetThreadPriority(GetCurrentThread()));
	wrc->processor_package_index = bp.processorPackageIndex;
	wrc->processor_core_index    = bp.processorCoreIndex;
	wrc->processor_logical_index = bp.processorLogicalIndex;
	wrc->numa_node_index         = numaNodeNumber;
	wrc->large_page              = isLargePage;
	wrc->min_delta_counter_hash  = 0xFFFFFFFF;
	Sleep(1);
	wrc->is_ready = 1;
	
	Worker worker(lwc, wrc);
	worker.loop();
}

bool yescrypt_r16_selftest(hash_function_t hashFun) {
	uint32_t blob[20] = {0x2acc48c2,0x21d521e9,0x430debb2,0x0a5d296c,0x8446fb80,0xc40b2fab,0xe5c7dccb,0x52976502,0xdeaa8a56,0x13da3f37,0x09431de9,0x648bfc6d,0x8fa33a11,0x0da5b07d,0x5c65bdcf,0xa0d42cb9,0xa08eac9c,0xbb552edc,0xa3d59342,0xcae838af};
	uint8_t hash_target[32] = {0xDB, 0x79, 0xB4, 0x8F, 0x31, 0xAD, 0x87, 0xBB, 0xB2, 0x17, 0x12, 0x7C, 0xE5, 0xB4, 0xB9, 0x0C, 0x6C, 0x05, 0x36, 0xA2, 0x07, 0x74, 0x91, 0x88, 0xBC, 0x8E, 0xAB, 0x86, 0x4E, 0xDA, 0xD1, 0xEE};
	uint64_t hash[4];

	void *mem = _mm_malloc(10*1024*1024, 4096);
	if ( !mem ) {
		return false;
	}
	hashFun((const void*)&blob, (void*)&hash, mem);
	_mm_free(mem);
	
	return !memcmp((void*)&hash, (void*)hash_target, sizeof(hash));
}

void app(global_workers_context_t *gbWorkersCtx, uint64_t id, HANDLE hParentProcess) {
	hash_function_t hashFun;
	
	if ( !loadWorker(&gbWorkersCtx->worker_path[0], &hashFun) ) {
		Common::terminateCurrentProcess();
	}
	
	if ( !yescrypt_r16_selftest(hashFun) ) {
		Common::terminateCurrentProcess();
	}
	
	SetPriorityClass(GetCurrentProcess(), procPrioToWinApi(gbWorkersCtx->wrappers_init[id].process_priority));
	
	int index = gbWorkersCtx->wrappers_init[id].index;
	int len = gbWorkersCtx->wrappers_init[id].len;
	for(int i = index; i < index + len; i++) {
		local_worker_context_t *lwc = (local_worker_context_t*)malloc(sizeof(local_worker_context_t));
		lwc->init_id = i;
		lwc->hash_fun = hashFun;
		lwc->gb_workers_ctx = gbWorkersCtx;
		
		
		DWORD idThread;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&threadEntyPoint, (LPVOID)lwc, 0, &idThread);
	}
	
	
	
	while(1) {
		if ( !Common::isProcessAlive(hParentProcess) ) {
			Common::terminateCurrentProcess();
		}
		
		Sleep(100);
	}
}

int main(int argc, char *argv[]) {
	if ( !WinApi::init() ) {
		Common::terminateCurrentProcess();
	}
	
	if ( argc < 4 ) {
		Common::terminateCurrentProcess();
	}
	
	uint16_t processId;
	try { processId = std::stoi(argv[1]); } catch(const std::invalid_argument& ia) { Common::terminateCurrentProcess(); }
	HANDLE hParentProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId); if ( hParentProcess == INVALID_HANDLE_VALUE ) { Common::terminateCurrentProcess(); }
	
	global_workers_context_t *gbWorkersCtx = (global_workers_context_t*)Common::openSharedMemory(argv[2]);
	if ( !gbWorkersCtx ) {
		Common::terminateCurrentProcess();
	}
	
	uint16_t id;
	try { id = std::stoi(argv[3]); } catch(const std::invalid_argument& ia) { Common::terminateCurrentProcess(); }
	
	app(gbWorkersCtx, id, hParentProcess);
	
	return 0;
}
