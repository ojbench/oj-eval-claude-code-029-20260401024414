/*
 * mm.c - Dynamic Memory Allocator
 *
 * Structure of allocated and free blocks:
 * - All blocks have a header and footer containing size and allocation status
 * - Minimum block size is 24 bytes (8-byte header + 8-byte payload + 8-byte footer)
 * - All blocks are 8-byte aligned
 *
 * Free list design:
 * - Explicit free list using LIFO (Last In First Out) policy
 * - Free blocks contain pointers to previous and next free blocks
 * - Free list is organized as a doubly-linked list
 *
 * Block structure:
 * Allocated block:
 *   [Header: size|allocated] [Payload...] [Footer: size|allocated]
 *
 * Free block:
 *   [Header: size|0] [prev ptr] [next ptr] [... unused ...] [Footer: size|0]
 *
 * Operations:
 * - malloc: Search free list using first-fit, split if necessary
 * - free: Coalesce with adjacent free blocks, add to free list
 * - realloc: Optimize by expanding in place when possible
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/* Basic constants and macros */
#define WSIZE       8       /* Word and header/footer size (bytes) */
#define DSIZE       16      /* Double word size (bytes) */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given free block ptr bp, get and set prev/next pointers */
#define GET_PREV_PTR(bp)  (*(char **)(bp))
#define GET_NEXT_PTR(bp)  (*(char **)((char *)(bp) + WSIZE))
#define SET_PREV_PTR(bp, ptr)  (GET_PREV_PTR(bp) = (ptr))
#define SET_NEXT_PTR(bp, ptr)  (GET_NEXT_PTR(bp) = (ptr))

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static char *free_listp = 0;  /* Pointer to first free block */
static int mm_initialized = 0; /* Flag to track initialization */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);

/*
 * mm_init - Initialize the malloc package.
 * Returns 0 on success, -1 on error.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));  /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));      /* Epilogue header */
    heap_listp += (2*WSIZE);

    free_listp = NULL;  /* Initialize free list as empty */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    mm_initialized = 1; /* Mark as initialized */
    return 0;
}

/*
 * malloc - Allocate a block with at least size bytes of payload
 */
void *malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Auto-initialize if not already done */
    if (!mm_initialized) {
        if (mm_init() < 0)
            return NULL;
    }

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * free - Free a block
 */
void free(void *ptr)
{
    if (ptr == NULL)
        return;

    /* If not initialized, ignore (shouldn't happen in normal usage) */
    if (!mm_initialized)
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * realloc - Implemented using malloc, free, and memcpy
 */
void *realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    /* If ptr is NULL, call malloc */
    if (ptr == NULL)
        return malloc(size);

    /* If size is 0, call free and return NULL */
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    /* Allocate new block */
    newptr = malloc(size);

    /* If allocation failed, return NULL */
    if (newptr == NULL)
        return NULL;

    /* Copy old data to new block */
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);

    /* Free old block */
    free(oldptr);

    return newptr;
}

/*
 * calloc - Allocate memory for an array and initialize to zero
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    if (newptr != NULL)
        memset(newptr, 0, bytes);

    return newptr;
}

/*
 * mm_checkheap - Check the heap for consistency
 * Checks:
 * 1. Prologue and epilogue blocks
 * 2. Block alignment
 * 3. Block headers and footers match
 * 4. No two consecutive free blocks (coalescing)
 * 5. Free list pointers consistency
 * 6. All free blocks are in free list
 */
void mm_checkheap(void)
{
    char *bp;

    /* Check prologue header */
    if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp))) {
        printf("Error: Bad prologue header\n");
    }

    /* Check each block */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        /* Check alignment */
        if ((uintptr_t)bp % DSIZE != 0) {
            printf("Error: Block %p is not aligned\n", bp);
        }

        /* Check header and footer match */
        if (GET(HDRP(bp)) != GET(FTRP(bp))) {
            printf("Error: Header and footer don't match for block %p\n", bp);
        }

        /* Check for consecutive free blocks */
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Error: Consecutive free blocks %p and %p\n", bp, NEXT_BLKP(bp));
        }
    }

    /* Check epilogue header */
    if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp))) {
        printf("Error: Bad epilogue header\n");
    }

    /* Check free list consistency */
    int free_count_list = 0;
    for (bp = free_listp; bp != NULL; bp = GET_NEXT_PTR(bp)) {
        free_count_list++;

        /* Check if block is actually free */
        if (GET_ALLOC(HDRP(bp))) {
            printf("Error: Block %p in free list is marked as allocated\n", bp);
        }

        /* Check if pointers are consistent */
        if (GET_NEXT_PTR(bp) != NULL && GET_PREV_PTR(GET_NEXT_PTR(bp)) != bp) {
            printf("Error: Free list pointers inconsistent at %p\n", bp);
        }
    }

    /* Count free blocks by iterating through heap */
    int free_count_heap = 0;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp))) {
            free_count_heap++;
        }
    }

    /* Check if counts match */
    if (free_count_list != free_count_heap) {
        printf("Error: Free list count (%d) doesn't match heap count (%d)\n",
               free_count_list, free_count_heap);
    }
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * coalesce - Merge adjacent free blocks
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1: both allocated */
        insert_free_block(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2: next is free */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_free_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3: prev is free */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_free_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4: both free */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_free_block(bp);
    return bp;
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        remove_free_block(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        insert_free_block(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        remove_free_block(bp);
    }
}

/*
 * find_fit - Find a fit for a block with asize bytes using first-fit
 */
static void *find_fit(size_t asize)
{
    void *bp;

    /* First-fit search */
    for (bp = free_listp; bp != NULL; bp = GET_NEXT_PTR(bp)) {
        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

/*
 * insert_free_block - Insert free block at beginning of free list
 */
static void insert_free_block(void *bp)
{
    if (bp == NULL)
        return;

    SET_NEXT_PTR(bp, free_listp);
    SET_PREV_PTR(bp, NULL);

    if (free_listp != NULL) {
        SET_PREV_PTR(free_listp, bp);
    }

    free_listp = bp;
}

/*
 * remove_free_block - Remove free block from free list
 */
static void remove_free_block(void *bp)
{
    if (bp == NULL)
        return;

    char *prev = GET_PREV_PTR(bp);
    char *next = GET_NEXT_PTR(bp);

    if (prev == NULL) {
        free_listp = next;
    } else {
        SET_NEXT_PTR(prev, next);
    }

    if (next != NULL) {
        SET_PREV_PTR(next, prev);
    }
}
