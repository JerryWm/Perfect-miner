#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <map>

#include <windows.h>

#include "WinApi.cpp"

namespace Cpu {

class GLPIE {
	private:
		uint8_t *buffer = NULL;
		uint8_t *ptr = NULL;
		DWORD size = 0;
		
	public:
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX slpi;
		
		PROCESSOR_RELATIONSHIP	*Processor;
		NUMA_NODE_RELATIONSHIP	*NumaNode;
		CACHE_RELATIONSHIP		*Cache;
		GROUP_RELATIONSHIP		*Group;
		
	public:
		void glpieFree() {
			if ( buffer ) {
				free((void*)buffer);
			}
			
			buffer = NULL;
			ptr = NULL;
			size = 0;
		}
		GLPIE() {
			buffer = NULL;
		}
		~GLPIE() {
			glpieFree();
		}
		
		bool init(LOGICAL_PROCESSOR_RELATIONSHIP select = RelationAll) {
			glpieFree();
			
			if ( !WinApi::GetLogicalProcessorInformationEx(select, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &size) && GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
				buffer = (uint8_t*)malloc(size);
				if ( WinApi::GetLogicalProcessorInformationEx(select, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &size) ) {
					ptr = (uint8_t*)buffer;
					return true;
				}
			} else {
				return false;
			}
		}
		bool each() {
			while (ptr < buffer + size) {
				slpi = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
				
				Processor	= &slpi->Processor;
				NumaNode	= &slpi->NumaNode;
				Cache		= &slpi->Cache;
				Group		= &slpi->Group;
				
				ptr += slpi->Size;
				return true;
			}
			
			return false;
		}
		
		bool eachNum(uint32_t num = 0) {
			while(1) {
				
				if ( !each() ) {
					return false;
				}
				
				if ( !num ) {
					return true;
				}
				
				num--;
			}
			
			return false;
		}
};

uint32_t getProcessorPackageCount() {
	uint32_t processorPackageCount = 0;
	GLPIE glpie;
	if ( glpie.init(RelationProcessorPackage) ) {
		while(glpie.each()) {
			processorPackageCount++;
		}
	}
	
	return processorPackageCount;
}

typedef struct {
	uint32_t processorPackageIndex, processorCoreIndex, processorLogicalIndex;
} bind_processor_t;

typedef std::map<uint32_t, bool> map_logical_processor_t;
map_logical_processor_t mapLogicalProcessor(PROCESSOR_RELATIONSHIP *Processor) {
	map_logical_processor_t issetLogProc;

	for(int i = 0; i < Processor->GroupCount; i++) {
		uint32_t Group = Processor->GroupMask[i].Group;
		KAFFINITY Mask  = Processor->GroupMask[i].Mask;
					
		uint32_t groupIndex = (Group & 0xFFFF) << 16;
					
		for(int i = 0; i < sizeof(Mask); i++) {
			if ( Mask & (1<<i) ) {
				uint32_t logicalProcessorId = groupIndex | (i & 0xFFFF);
				issetLogProc[logicalProcessorId] = true;
				//printf("logicalProcessorId %x \n", logicalProcessorId);
			}
		}	
	}
	
	return issetLogProc;
}
bool isMapInMapLogicalProcessor(const map_logical_processor_t mlpNeedle, const map_logical_processor_t mplHaystack) {
	for(auto val : mlpNeedle) {
		if ( !mplHaystack.count(val.first) ) {
			return false;
		}
	}
	
	return true;
}

bool getProcessorBind(uint32_t processorPackageIndex, uint32_t processorCoreIndex, uint32_t processorLogicalIndex, GROUP_AFFINITY *ga) {
	uint32_t _processorPackageIndex = 0;
	uint32_t _processorCoreIndex = 0;
	uint32_t _processorLogicalIndex = 0;

	GLPIE glpie;
	if ( glpie.init(RelationProcessorPackage) ) {
		while(glpie.each()) {
			if ( _processorPackageIndex++ == processorPackageIndex ) {
				
				map_logical_processor_t maplp = mapLogicalProcessor(glpie.Processor);
				
				if ( maplp.size() ) {
					//printf("issetLogProc %d \n", maplp.size());
					
					GLPIE glpieForCore;
					if ( glpieForCore.init(RelationProcessorCore) ) {
						while(glpieForCore.each()) {
							
							map_logical_processor_t maplpForCore = mapLogicalProcessor(glpieForCore.Processor);
							
							if ( isMapInMapLogicalProcessor(maplpForCore, maplp) ) {
								if ( _processorCoreIndex++ == processorCoreIndex ) {
									
									for(auto val : maplpForCore) {
										if ( _processorLogicalIndex++ == processorLogicalIndex ) {
											
											ga->Group = (val.first >> 16) & 0xFFFF;
											ga->Mask = 1 << (val.first & 0xFFFF);
											//printf("	>>> val.first %d \n", val.first);
											return true;
										}
									}
								}
							}
						}
					}	
				}
				
			}
		}
	}
	
	return false;
}
bool getProcessorBindForSeq(uint64_t seq, bind_processor_t *bp) {
	uint32_t _processorPackageIndex = 0;
	uint32_t _processorCoreIndex = 0;
	uint32_t _processorLogicalIndex = 0;
	
	GROUP_AFFINITY ga;

	uint32_t processorPackageCount = getProcessorPackageCount();
	
	for(_processorLogicalIndex = 0; ;_processorLogicalIndex++) {
		int fcount_lv1 = 0;
		
		for(_processorCoreIndex = 0; ;_processorCoreIndex++) {
			
			int fcount_lv2 = 0;
			for(_processorPackageIndex = 0; _processorPackageIndex < processorPackageCount; _processorPackageIndex++) {

				if ( getProcessorBind(_processorPackageIndex, _processorCoreIndex, _processorLogicalIndex, &ga) ) {
					if ( !(seq--) ) {
						bp->processorPackageIndex = _processorPackageIndex;
						bp->processorCoreIndex    = _processorCoreIndex;
						bp->processorLogicalIndex = _processorLogicalIndex;
						return true;
					}
					
					fcount_lv1++;
					fcount_lv2++;
				}
			}
			
			if ( !fcount_lv2 ) {break;}
		}
		
		if ( !fcount_lv1 ) {
			break;
		}
	}
	
	return false;
}
bool setThreadProcessorAffinity(HANDLE hThread, bind_processor_t *bp) {
	GROUP_AFFINITY ga;
	GROUP_AFFINITY prevGa;
	
	if ( !getProcessorBind(bp->processorPackageIndex, bp->processorCoreIndex, bp->processorLogicalIndex, &ga) ) {
		return false;
	}
	
	return WinApi::SetThreadGroupAffinity(hThread, &ga, &prevGa);
}
uint32_t getProcessorLogicalCount() {
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	return SystemInfo.dwNumberOfProcessors;
}

}