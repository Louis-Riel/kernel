#ifndef __mallocdbg_h
#define __mallocdbg_h
#include "freertos/FreeRTOS.h"

#include <unistd.h>
#include <string.h>
#include "cJSON.h"

#define DEBUG_MALLOC

#ifdef DEBUG_MALLOC
cJSON* getMemoryStats();
struct mallocdbg
{
    char* name=0;
    int32_t hitCount=0;
    int32_t bytes=0;
};
static mallocdbg mallocs[256];

void* dbgmalloc(const char* funcname, size_t size);
void dbgfree(const char* funcname,void* ptr);

#define dmalloc(size) dbgmalloc(__PRETTY_FUNCTION__, size)
#define ldfree(ptr) dbgfree(__PRETTY_FUNCTION__,ptr)
#else
#define dmalloc(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define ldfree(ptr) free(ptr)
#endif


#endif