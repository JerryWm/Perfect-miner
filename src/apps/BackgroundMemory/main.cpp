#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>

#include <share/cpp/PerfomanceTime.cpp>
#include <share/cpp/BindCpu.cpp>
#include <share/cpp/WinApi.cpp>
#include <share/cpp/Common.cpp>
#include <share/cpp/LargePageSharedMemory.cpp>


int main() {
	
	if ( !WinApi::init() ) {
		return 0;
	}

	if ( !Common::addPrivilege(GetCurrentProcess(), SE_LOCK_MEMORY_NAME) ) {
		return 0;
	}
	
	LargePageMemoryBackground::SharedMemoryMng smm;
	
	if ( !LargePageMemoryBackground::sharedMemoryServer(YESCRYPT_R16_GB_SHARED_MEMORY_NAME, smm) ) {
		return 0;
	}

	return 0;
	
}