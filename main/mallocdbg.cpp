#include "mallocdbg.h"

#ifdef DEBUG_MALLOC
static cJSON* memstats = NULL;

cJSON* getMemoryStats(){
    cJSON* memstat;
    for (mallocdbg &dmalloc : mallocs)
    {
        if (dmalloc.name != NULL) {
            if (memstats == NULL) {
                memstats = cJSON_CreateArray();
            }
            bool found=false;
            cJSON_ArrayForEach(memstat,memstats) {
                cJSON* func = memstat ? cJSON_GetObjectItem(memstat,"function") : NULL;
                if (func != NULL) {
                    if (strcmp(dmalloc.name,func->valuestring)==0) {
                        found=true;
                        cJSON_SetIntValue(cJSON_GetObjectItem(memstat,"hitcount"),dmalloc.hitCount);
                        cJSON_SetIntValue(cJSON_GetObjectItem(memstat,"bytes"),dmalloc.bytes);
                    }
                }
            }
            if (!found && cJSON_AddItemToArray(memstats,memstat=cJSON_CreateObject())) {
                cJSON_AddStringToObject(memstat,"function",dmalloc.name);
                cJSON_AddNumberToObject(memstat,"hitcount",dmalloc.hitCount);
                cJSON_AddNumberToObject(memstat,"bytes",dmalloc.bytes);
            }
        }
    }
    return memstats;
}

void* dbgmalloc(const char* funcname, size_t size)
{
    void* ptr = NULL;

    if(size > 0)
    {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

        #ifdef DEBUG_MALLOC
        mallocdbg* freeSpot=NULL;
        bool foundIt=false;
        for (mallocdbg &dmalloc : mallocs)
        {
            if (dmalloc.name != NULL){
                if (strcmp(dmalloc.name,funcname)==0){
                    dmalloc.hitCount++;
                    dmalloc.bytes += size;
                    foundIt=true;
                    break;
                }
            } else if (freeSpot == NULL) {
                freeSpot = &dmalloc;
            }
        }
        if ((!foundIt) && (freeSpot != NULL)) {
            freeSpot->name = (char*)malloc(strlen(funcname)+1);
            strcpy(freeSpot->name,funcname);
            freeSpot->hitCount++;
            freeSpot->bytes=size;
        }
        #endif
    }

    return ptr;
}

void dbgfree(const char* funcname, void* ptr)
{
    if(ptr != NULL)
    {

        #ifdef DEBUG_MALLOC
        for (mallocdbg &dmalloc : mallocs)
        {
            if ((dmalloc.name != NULL) && (strcmp(dmalloc.name,funcname))==0){
                dmalloc.hitCount--;
                //dmalloc.bytes-=sizeof(&ptr);
                break;
            }
        }
        #endif
        vPortFree (ptr);
    }
}

#endif

