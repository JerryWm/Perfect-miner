#include <stdint.h>

#include <share/c/worker_api_header.c>

#include "yescrypt/yescrypt.c"

extern "C" {
	DLL_EXPORT WORKER_API void hash(const void *blob, void *hash, void *memory);
}

DLL_EXPORT WORKER_API void hash(const void *blob, void *hash, void *memory) {
	yescrypt_hash((const char*)blob, (char*)hash, memory);
}