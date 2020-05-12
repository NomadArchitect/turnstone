#ifndef ___SETUP_H
#define ___SETUP_H 0

#include <types.h>
#include <memory.h>
#include <systeminfo.h>
#include "os_io.h"

#define RAMSIZE 0x1000000

#define REDCOLOR "\033[1;31m"
#define GREENCOLOR "\033[1;32m"
#define RESETCOLOR "\033[0m"

int printf(const char* format, ...);

uint8_t mem_area[RAMSIZE] = {0};
uint64_t __kheap_bottom = 0;
system_info_t* SYSTEM_INFO = NULL;

void print_success(const char* msg){
	printf("%s%s%s%s", GREENCOLOR, msg, RESETCOLOR, "\r\n");
}

void print_error(const char* msg){
	printf("%s%s%s%s", REDCOLOR, msg, RESETCOLOR, "\r\n");
}

void setup_ram() {
	memory_heap_t* heap = memory_create_heap_simple((size_t)&mem_area[0], (size_t)&mem_area[RAMSIZE]);
	printf("%p\n", heap);
	memory_set_default_heap(heap);
}

#endif