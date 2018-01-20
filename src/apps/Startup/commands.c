#pragma once

#define SV_CMD_STARTUP_INFO		(1)
#define SV_CMD_JOB 				(2)
#define SV_CMD_PING 			(3)

#define CL_CMD_SHARE			(81)
#define CL_CMD_INFO				(82)
#define CL_CMD_PONG 			(84)
#define CL_CMD_HELLO			(85)
#define CL_CMD_ERROR			(86)
#define CL_CMD_WORKER_INFO		(87)

#pragma pack(push, 1)
/*** CLIENT */
typedef struct {
	uint32_t id;
} cl_pong_t;

typedef struct {
	uint8_t text[500];
} cl_error_t;

typedef struct {
	uint32_t job_id;
	uint64_t hash[4];
	uint32_t nonce;
} cl_share_t;

typedef struct {
	uint64_t hash_count;
	uint64_t share_count;
	uint32_t min_delta_micro_sec_hash;
	uint64_t time_mili_sec;
} cl_info_t;

typedef struct {
	uint32_t process_priority;
	uint32_t thread_priority;
	
	uint32_t processor_package_index;
	uint32_t processor_core_index;
	uint32_t processor_logical_index;
	uint32_t numa_node_index;
	
	uint32_t large_page;
} cl_worker_info_t;


typedef struct {
	uint32_t cmd;
	union {
		cl_share_t share;
		cl_info_t info;
		cl_pong_t pong;
		cl_error_t error;
		cl_worker_info_t worker_info;
		char _pad[4096];
	};
} client_command_t;


/*** SERVER */
typedef struct {
	uint32_t thread_count;
	uint32_t process_priority;
	uint32_t thread_priority;
	uint32_t test_perfomance;

	uint32_t worker_x64;
	uint8_t  worker_path[500];
} sv_startup_info_t;

typedef struct {
	uint32_t job_id;
	uint64_t target[4];
	uint32_t blob[20];
} sv_job_t;

typedef struct {
	uint32_t id;
} sv_ping_t;

typedef struct {
	uint32_t cmd;
	union {
		sv_startup_info_t startup_info;
		sv_job_t job;
		sv_ping_t ping;
		char _pad[4096];
	};
} server_command_t;
#pragma pack(pop)
