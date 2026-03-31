#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"

/* Private global variables */
static char *mem_heap;     /* Points to first byte of heap */
static char *mem_brk;      /* Points to last byte of heap plus 1 */
static char *mem_max_addr; /* Max legal heap addr plus 1*/

#define MAX_HEAP (1UL << 32)  /* 4 GB */

/* Initialize on first use */
static void mem_init_internal(void)
{
    static int initialized = 0;
    if (initialized) return;

    mem_heap = (char *)mmap(NULL, MAX_HEAP, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem_heap == MAP_FAILED) {
        fprintf(stderr, "mem_init: mmap error\n");
        exit(1);
    }
    mem_brk = mem_heap;
    mem_max_addr = mem_heap + MAX_HEAP;
    initialized = 1;
}

/*
 * mem_sbrk - Simple model of the sbrk function. Extends the heap
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */
void *mem_sbrk(int incr)
{
    mem_init_internal();

    char *old_brk = mem_brk;

    if (incr < 0 || (mem_brk + incr) > mem_max_addr) {
        errno = ENOMEM;
        fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
        return (void *)-1;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

/*
 * mem_heap_lo - Return address of the first heap byte
 */
void *mem_heap_lo(void)
{
    mem_init_internal();
    return (void *)mem_heap;
}

/*
 * mem_heap_hi - Return address of last heap byte
 */
void *mem_heap_hi(void)
{
    mem_init_internal();
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - Returns the heap size in bytes
 */
size_t mem_heapsize(void)
{
    mem_init_internal();
    return (size_t)(mem_brk - mem_heap);
}

/*
 * mem_pagesize() - Returns the page size of the system
 */
size_t mem_pagesize(void)
{
    return (size_t)getpagesize();
}
