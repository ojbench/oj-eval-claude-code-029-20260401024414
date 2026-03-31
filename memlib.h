#ifndef MEMLIB_H
#define MEMLIB_H

#include <stddef.h>

void *mem_sbrk(int incr);
void *mem_heap_lo(void);
void *mem_heap_hi(void);
size_t mem_heapsize(void);
size_t mem_pagesize(void);

#endif /* MEMLIB_H */
