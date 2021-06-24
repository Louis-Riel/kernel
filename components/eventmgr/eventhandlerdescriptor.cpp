#include "./eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventDescriptor_t* EventHandlerDescriptor::eventDescriptorCache = NULL;
uint32_t EventHandlerDescriptor::numCacheEntries = 0;

EventHandlerDescriptor::EventHandlerDescriptor(esp_event_base_t eventBase,char* name){
    this->eventBase = eventBase;
    this->eventDescriptors = (EventDescriptor_t*)dmalloc(sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS_PER_BASE);
    memset(this->eventDescriptors,0,sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS_PER_BASE);
    if (EventHandlerDescriptor::eventDescriptorCache == NULL) {
        EventHandlerDescriptor::eventDescriptorCache = (EventDescriptor_t*)dmalloc(sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS);
        memset(EventHandlerDescriptor::eventDescriptorCache,0,sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS);
    }
    this->name = (char*)dmalloc(strlen(name)+1);
    strcpy(this->name,name);
}

bool EventHandlerDescriptor::AddEventDescriptor(int32_t id,char* eventName){
    for (int idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        EventDescriptor_t* descriptor = &this->eventDescriptors[idx];
        if (!descriptor->eventName){
            descriptor->id = id;
            descriptor->eventName = eventName;
            descriptor->baseName = name;
            EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries].id = id;
            EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries].baseName = name;
            EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries++].eventName = eventName;
            return true;
        }
    }
    return false;
}

char* EventHandlerDescriptor::GetName(){
    return this->name;
}

cJSON* EventHandlerDescriptor::GetEventBaseEvents(char* base, char* filter) {
    cJSON* ret = cJSON_CreateArray();
    for (int idx = 0; idx < MAX_NUM_EVENTS; idx++) {
        EventDescriptor_t* ei = &EventHandlerDescriptor::eventDescriptorCache[idx];
        if ((ei != NULL) || 
            (filter == NULL) || 
            (strlen(filter) == 0) || 
            ((indexOf(ei->baseName,base) != NULL) && (indexOf(ei->eventName,filter) != NULL))) {
            cJSON* curItem = NULL;
            cJSON_ArrayForEach(curItem,ret) {
                if ((strcmp(cJSON_GetObjectItem(curItem,"baseName")->valuestring,ei->baseName) == 0) && 
                    (strcmp(cJSON_GetObjectItem(curItem,"eventName")->valuestring,ei->eventName) == 0)) {
                    break;
                }
            };
            if (curItem == NULL){
                cJSON* item = cJSON_CreateObject();
                cJSON_AddStringToObject(item,"baseName",ei->baseName);
                cJSON_AddStringToObject(item,"eventName",ei->eventName);
                cJSON_AddNumberToObject(item,"eventId",ei->id);
                cJSON_AddItemToArray(ret,item);
            }
        }
    }
    return ret;
}

cJSON* EventHandlerDescriptor::GetEventBases(char* filter) {
    cJSON* ret = cJSON_CreateArray();
    for (int idx = 0; idx < MAX_NUM_EVENTS; idx++) {
        EventDescriptor_t* ei = &EventHandlerDescriptor::eventDescriptorCache[idx];
        if ((ei != NULL) || 
            (filter == NULL) || 
            (strlen(filter) == 0) || 
            (indexOf(ei->baseName,filter) != NULL)) {
            cJSON* curItem = NULL;
            cJSON_ArrayForEach(curItem,ret) {
                if (strcmp(cJSON_GetObjectItem(curItem,"name")->valuestring,ei->baseName) == 0) {
                    break;
                }
            };
            if (curItem == NULL){
                cJSON* item = cJSON_CreateObject();
                cJSON_AddStringToObject(item,"name",ei->eventName);
                cJSON_AddItemToArray(ret,item);
            }
        }
    }
    return ret;
}

void EventHandlerDescriptor::SetEventName(int32_t id,char* name){
    int freeSlot = -1;
    for (uint8_t idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        if (eventDescriptors[idx].eventName == NULL) {
            ESP_LOGV(__FUNCTION__,"%d:%s set at idx:%d", id, name, idx);
            eventDescriptors[idx].id = id;
            eventDescriptors[idx].eventName = (char*)dmalloc(strlen(name)+1);
            strcpy(eventDescriptors[idx].eventName,name);
            return;
        } else {
            ESP_LOGV(__FUNCTION__,"%d:%s not set at idx:%d", eventDescriptors[idx].id, name, idx);
        }
    }
}

int32_t EventHandlerDescriptor::GetEventId(char* name){
    if ((name == NULL) || (strlen(name)==0)) {
        return -1;
    }
    for (int idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        EventDescriptor_t* descriptor = &this->eventDescriptors[idx];
        if ((descriptor != NULL) && (strcmp(descriptor->eventName,name)==0)){
            return descriptor->id;
        }
    }
    return -1;
}

char* EventHandlerDescriptor::GetEventName(int32_t id){
    for (int idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        EventDescriptor_t* descriptor = &this->eventDescriptors[idx];
        if ((descriptor != NULL) && (descriptor->id == id)){
            return descriptor->eventName;
        }
    }
    return NULL;
}

esp_event_base_t EventHandlerDescriptor::GetEventBase(){
    return this->eventBase;
}
