#pragma once

#include <cmath>
#include "windows.h"

class PerfomanceTime {
	public:
		inline static PerfomanceTime &i() {
			static PerfomanceTime *__self = NULL;
			if ( !__self ) {
				__self = (PerfomanceTime*)malloc(sizeof(PerfomanceTime));
				__self->init();
			}
			return *__self;
		}
	
	public:
		inline long double counterToSecDouble(uint64_t counter) {
			return ( ((long double)counter)  / this->frequencyDouble);
		}
		inline long double counterToMiliSecDouble(uint64_t counter) {
			return ( ((long double)counter)  / this->frequencyMiliSecDouble);
		}
		inline long double counterToMicroSecDouble(uint64_t counter) {
			return ( ((long double)counter)  / this->frequencyMicroSecDouble);
		}
	
	public:
		uint64_t frequency;
		uint64_t frequencyMliSec;
		uint64_t frequencyMicroSec;
		
		long double frequencyDouble;
		long double frequencyMiliSecDouble;
		long double frequencyMicroSecDouble;
		PerfomanceTime() {
			this->init();
		}
		inline void init() {
			QueryPerformanceFrequency(( LARGE_INTEGER*)&this->frequency);			
			this->frequencyMliSec   = this->frequency / 1000;
			this->frequencyMicroSec = this->frequency / (1000 * 1000);
			
			this->frequencyDouble = (long double)this->frequency;	
			this->frequencyMiliSecDouble  = this->frequencyDouble / 1e3;
			this->frequencyMicroSecDouble = this->frequencyDouble / 1e6;
		}
	
		inline uint64_t getCounter() {
			uint64_t time;
			QueryPerformanceCounter(( LARGE_INTEGER*)&time);
			return time;
		}
		inline long double   getCounterDouble() {
			return (long double)this->getCounter();
		}
	
		inline uint64_t getSec() {
			return std::round(getSecDouble());
			//return (this->getCounter() / this->frequency);
		}
		inline uint64_t getMiliSec() {
			return std::round(getMiliSecDouble());
			//return (this->getCounter() / this->frequencyMliSec);
		}
		inline uint64_t getMicroSec() {
			return std::round(getMicroSecDouble());
			//return (this->getCounter() / this->frequencyMicroSec);
		}
		
		inline long double   getSecDouble() {
			return this->counterToSecDouble( this->getCounter() );
		}
		inline long double   getMiliSecDouble() {
			return this->counterToMiliSecDouble( this->getCounter() );
		}
		inline long double   getMicroSecDouble() {
			return this->counterToMicroSecDouble( this->getCounter() );
		}
};
