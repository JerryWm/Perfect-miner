#pragma once


void _memcopy(uint8_t *dst, uint8_t *src, size_t size) {
	while(size--) {
		*dst = *src;
		dst++;src++;
	}
}

#define MEMCOPY(dst, src, size)	\
	_memcopy((uint8_t*)(dst), (uint8_t*)(src), (size_t)(size));
	
	