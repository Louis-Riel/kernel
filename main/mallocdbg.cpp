#include "mallocdbg.h"

#ifdef DEBUG_MALLOC
cJSON* getMemoryStats(){
    cJSON* memstats = cJSON_CreateArray();
    for (mallocdbg &dmalloc : mallocs)
    {
        if (dmalloc.name != NULL) {
            cJSON* memstat = cJSON_CreateObject();
            if (cJSON_AddItemToArray(memstats,memstat)) {
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
        ptr = pvPortMalloc (size);

        #ifdef DEBUG_MALLOC
        mallocdbg* freeSpot=NULL;
        bool foundIt=false;
        for (mallocdbg &dmalloc : mallocs)
        {
            if (dmalloc.name != NULL){
                if (strcmp(dmalloc.name,funcname)==0){
                    dmalloc.hitCount++;
                    dmalloc.bytes =+ size;
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
                dmalloc.bytes-=sizeof(&ptr);
                break;
            }
        }
        #endif
        vPortFree (ptr);
    }
}

#endif

