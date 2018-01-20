#pragma once

typedef struct {
	uint32_t job_id;
	union {
		uint32_t blob[20];
		struct {
			uint32_t _pad[19];
			uint32_t nonce;
		};
	};
	uint64_t target[4];
} job_context_t;

typedef struct {
	uint32_t job_id;
	uint64_t hash[4];
	uint32_t nonce;
} share_context_t;

