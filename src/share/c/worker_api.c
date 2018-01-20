#pragma once

#include "worker_api_header.c"

typedef void (__cdecl *hash_function_t) (const void *, void *, void *);
	
int loadWorker(const char *workerPath, hash_function_t *hashFun) {
	HMODULE hModule = LoadLibraryA(workerPath);

	if ( !hModule ) {
		return 0;
	}

	*hashFun = (hash_function_t)GetProcAddress(hModule , "hash");

	if ( !*hashFun ) {
		return 0;
	}
	
	return 1;
}