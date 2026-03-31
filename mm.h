#ifndef MM_H
#define MM_H

#include <stddef.h>

int mm_init(void);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
void mm_checkheap(void);

#endif /* MM_H */
