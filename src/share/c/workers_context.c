#pragma once

#include "job_context.c"

#define WORKERSCTX_MAX_WORKERS		(4096)
#define WORKERSCTX_ENT_LIST_MAX		(4096)
#define WORKERSCTX_ENT_GET_INDEX(i)	\
	((i) % WORKERSCTX_ENT_LIST_MAX)

typedef struct {
	uint32_t process_priority;
	uint32_t thread_priority;
	
	uint32_t processor_package_index;
	uint32_t processor_core_index;
	uint32_t processor_logical_index;
	uint32_t numa_node_index;
	
	uint32_t large_page;
	
	uint32_t min_delta_counter_hash;
	
	uint64_t hash_count;
	uint64_t share_count;
	
	volatile uint8_t is_ready;
} worker_responce_context_t;

typedef struct {
	uint32_t thread_priority;
} worker_init_context_t;

typedef struct {
	job_context_t job;
} job_worker_t;

typedef struct {
	share_context_t share;
	volatile uint8_t is_ready;
} share_worker_t;

typedef struct {

	volatile uint32_t thread_count;
	volatile uint32_t thread_open_count;
	volatile uint32_t thread_seq;
	
	volatile uint32_t worker_count;
	
	worker_init_context_t workers_init[WORKERSCTX_MAX_WORKERS];
	worker_responce_context_t workers_responce[WORKERSCTX_MAX_WORKERS];
	
	char worker_path[4096];
	
	uint32_t test_perfomance;
	
	struct {
		uint32_t process_priority;
		uint32_t index;
		uint32_t len;
	} wrappers_init[256];
	
	
	volatile uint64_t jobSeq;
	job_worker_t   jobs[WORKERSCTX_ENT_LIST_MAX];
	
	volatile uint64_t shareSeqRead;
	volatile uint64_t shareSeqWrite;
	share_worker_t shares[WORKERSCTX_ENT_LIST_MAX];
	
} global_workers_context_t;