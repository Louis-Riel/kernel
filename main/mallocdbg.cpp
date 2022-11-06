#include "mallocdbg.h"

#ifdef DEBUG_MALLOC
static cJSON* memstats = nullptr;

cJSON* getMemoryStats(){
    cJSON* memstat;
    for (mallocdbg &dmalloc : mallocs)
    {
        if (dmalloc.name != nullptr) {
            if (memstats == nullptr) {
                memstats = cJSON_CreateArray();
            }
            bool found=false;
            cJSON_ArrayForEach(memstat,memstats) {
                cJSON const* func = memstat ? cJSON_GetObjectItem(memstat,"function") : nullptr;
                if (func != nullptr) {
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
    void* ptr = nullptr;

    if(size > 0)
    {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

        #ifdef DEBUG_MALLOC
        mallocdbg* freeSpot=nullptr;
        bool foundIt=false;
        for (mallocdbg &dmalloc : mallocs)
        {
            if (dmalloc.name != nullptr){
                if (strcmp(dmalloc.name,funcname)==0){
                    dmalloc.hitCount++;
                    dmalloc.bytes += size;
                    foundIt=true;
                    break;
                }
            } else if (freeSpot == nullptr) {
                freeSpot = &dmalloc;
            }
        }
        if ((!foundIt) && (freeSpot != nullptr)) {
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
    if(ptr != nullptr)
    {

        #ifdef DEBUG_MALLOC
        for (mallocdbg &dmalloc : mallocs)
        {
            if ((dmalloc.name != nullptr) && (strcmp(dmalloc.name,funcname))==0){
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

