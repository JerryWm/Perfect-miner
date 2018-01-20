#pragma once

#include <windows.h>

#define PROC_PRIO_IDLE		(0)
#define PROC_PRIO_NORMAL 	(1)
#define PROC_PRIO_HIGH		(2)
#define PROC_PRIO_REALTIME	(3)
#define PROC_PRIO_UNDEFINED (999)

#define THR_PRIO_IDLE			(0)
#define THR_PRIO_LOWEST			(1)
#define THR_PRIO_NORMAL			(2)
#define THR_PRIO_HIGHEST		(3)
#define THR_PRIO_TIME_CRITICAL	(4)
#define THR_PRIO_UNDEFINED		(999)


int procPrioToWinApi(int prio) {
	switch(prio) {
		case PROC_PRIO_IDLE    : return IDLE_PRIORITY_CLASS;
		case PROC_PRIO_NORMAL  : return NORMAL_PRIORITY_CLASS;
		case PROC_PRIO_HIGH    : return HIGH_PRIORITY_CLASS;
		case PROC_PRIO_REALTIME: return REALTIME_PRIORITY_CLASS;
		default:  return NORMAL_PRIORITY_CLASS;
	}
}
int procPrioOfWinApi(DWORD prio) {
	switch(prio) {
		case IDLE_PRIORITY_CLASS    : return PROC_PRIO_IDLE;
		case NORMAL_PRIORITY_CLASS  : return PROC_PRIO_NORMAL;
		case HIGH_PRIORITY_CLASS    : return PROC_PRIO_HIGH;
		case REALTIME_PRIORITY_CLASS: return PROC_PRIO_REALTIME;
		default:  return PROC_PRIO_UNDEFINED;
	}
}


int thrPrioToWinApi(int prio) {
	switch(prio) {
		case THR_PRIO_IDLE         : return THREAD_PRIORITY_IDLE;
		case THR_PRIO_LOWEST       : return THREAD_PRIORITY_LOWEST;
		case THR_PRIO_NORMAL       : return THREAD_PRIORITY_NORMAL;
		case THR_PRIO_HIGHEST      : return THREAD_PRIORITY_HIGHEST;
		case THR_PRIO_TIME_CRITICAL: return THREAD_PRIORITY_TIME_CRITICAL;
		default: return THREAD_PRIORITY_NORMAL;
	}
}
int thrPrioOfWinApi(int prio) {
	switch(prio) {
		case THREAD_PRIORITY_IDLE         : return THR_PRIO_IDLE;
		case THREAD_PRIORITY_LOWEST       : return THR_PRIO_LOWEST;
		case THREAD_PRIORITY_NORMAL       : return THR_PRIO_NORMAL;
		case THREAD_PRIORITY_HIGHEST      : return THR_PRIO_HIGHEST;
		case THREAD_PRIORITY_TIME_CRITICAL: return THR_PRIO_TIME_CRITICAL;
		default: return THR_PRIO_UNDEFINED;
	}
}


