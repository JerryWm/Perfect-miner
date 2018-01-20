#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <ntdef.h>

namespace WinApi {
	
	typedef LPVOID  (WINAPI *f_VirtualAllocExNuma) (_In_ HANDLE hProcess, _In_opt_ LPVOID lpAddress, _In_ SIZE_T dwSize, _In_ DWORD  flAllocationType, _In_ DWORD  flProtect, _In_ DWORD  nndPreferred);
	
	typedef WINBOOL (WINAPI *f_GetLogicalProcessorInformationEx) (LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
	typedef WINBOOL (WINAPI *f_SetThreadGroupAffinity ) (HANDLE hThread , CONST GROUP_AFFINITY *GroupAffinity, PGROUP_AFFINITY PreviousGroupAffinity);

	typedef VOID  (WINAPI *f_GetCurrentProcessorNumberEx) (_Out_ PPROCESSOR_NUMBER ProcNumber);
	typedef WINBOOL (WINAPI *f_GetNumaProcessorNodeEx) (_In_  PPROCESSOR_NUMBER Processor, _Out_ PUSHORT NodeNumber);
	
	typedef WINBOOL (WINAPI *f_GetNumaHighestNodeNumber) (_Out_ PULONG HighestNodeNumber);
	
	typedef HANDLE (WINAPI *f_CreateFileMappingNumaA)(
		  _In_     HANDLE                hFile,
		  _In_opt_ LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		  _In_     DWORD                 flProtect,
		  _In_     DWORD                 dwMaximumSizeHigh,
		  _In_     DWORD                 dwMaximumSizeLow,
		  _In_opt_ LPCTSTR               lpName,
		  _In_     DWORD                 nndPreferred
	);		
	
	f_VirtualAllocExNuma VirtualAllocExNuma;
	f_GetLogicalProcessorInformationEx GetLogicalProcessorInformationEx;
	f_SetThreadGroupAffinity SetThreadGroupAffinity;
	
	f_GetCurrentProcessorNumberEx GetCurrentProcessorNumberEx;
	f_GetNumaProcessorNodeEx GetNumaProcessorNodeEx;
	
	f_GetNumaHighestNodeNumber GetNumaHighestNodeNumber;

	f_CreateFileMappingNumaA CreateFileMappingNumaA;
	
	#define __INIT_FUN_KERNEL32(name)	\
		name = (f_##name)GetProcAddress(GetModuleHandle(TEXT("kernel32")), #name);	\
		if ( !name ) {					\
			error_text += #name;		\
			error_text += "\r\n";		\
			return false; 				\
		}
		
	
	std::string error_text = "";
	
	bool init() {
		__INIT_FUN_KERNEL32(VirtualAllocExNuma)
		__INIT_FUN_KERNEL32(GetLogicalProcessorInformationEx)
		__INIT_FUN_KERNEL32(SetThreadGroupAffinity)
		__INIT_FUN_KERNEL32(GetNumaProcessorNodeEx)
		__INIT_FUN_KERNEL32(GetCurrentProcessorNumberEx)
		__INIT_FUN_KERNEL32(GetNumaHighestNodeNumber)
		__INIT_FUN_KERNEL32(CreateFileMappingNumaA)
		
		return !error_text.length();
	}
}

