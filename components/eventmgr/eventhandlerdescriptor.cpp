#include "eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventDescriptor_t* EventHandlerDescriptor::eventDescriptorCache = NULL;
uint32_t EventHandlerDescriptor::numCacheEntries = 0;

EventHandlerDescriptor::EventHandlerDescriptor(esp_event_base_t eventBase,char* name){
    this->eventBase = eventBase;
    this->eventDescriptors = (EventDescriptor_t*)dmalloc(sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS_PER_BASE);
    ESP_LOGV(__FUNCTION__,"Event descriptor for base %s with name %s created", eventBase, name);
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
            ESP_LOGV(__FUNCTION__,"Event descriptor %s(%d) registered in %s", eventName,id,this->eventBase);
            return true;
        }
    }
    ESP_LOGE(__FUNCTION__,"No space left for event descriptor, cannot register %s(%d) for base %s", eventName,id,this->eventBase);
    for (int idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        EventDescriptor_t* descriptor = &this->eventDescriptors[idx];
        if (descriptor->eventName){
            ESP_LOGV(__FUNCTION__,"%d Event descriptor %s(%d) registered in %s", idx, eventName,id,this->eventBase);
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

EventDescriptor_t* EventHandlerDescriptor::GetEventDescriptor(char* base,char* eventName) {
    ESP_LOGV(__FUNCTION__,"GetEventDescriptor %s(%s)", eventName, base);
    for (int idx = 0; idx < MAX_NUM_EVENTS; idx++) {
        EventDescriptor_t* ei = &EventHandlerDescriptor::eventDescriptorCache[idx];
        if ((ei != NULL) && 
            (ei->baseName != NULL) &&
            (ei->eventName != NULL) &&
            (base != NULL) && 
            (eventName != NULL) && 
            (indexOf(ei->baseName,base) != NULL) &&
            (indexOf(ei->eventName,eventName) != NULL)) {
            return ei;
        }
    }
    return NULL;
}

EventDescriptor_t* EventHandlerDescriptor::GetEventDescriptor(esp_event_base_t base,uint32_t id) {
    ESP_LOGV(__FUNCTION__,"GetEventDescriptor %s(%d)",  base, id);
    for (int idx = 0; idx < MAX_NUM_EVENTS; idx++) {
        EventDescriptor_t* ei = &EventHandlerDescriptor::eventDescriptorCache[idx];
        if ((ei != NULL) && 
            (ei->baseName != NULL) &&
            (ei->id >= 0) &&
            (base != NULL) && 
            (indexOf(ei->baseName,base) != NULL) &&
            (ei->id == id)) {
            return ei;
        }
    }
    return NULL;
}

void EventHandlerDescriptor::SetEventName(int32_t id,char* name){
    for (uint8_t idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        if (eventDescriptors[idx].eventName == NULL) {
            ESP_LOGV(__FUNCTION__,"%d:%s set at idx:%d", id, name, idx);
            eventDescriptors[idx].id = id;
            eventDescriptors[idx].eventName = (char*)dmalloc(strlen(name)+1);
            strcpy(eventDescriptors[idx].eventName,name);
            return;
        }
    }
    ESP_LOGE(__FUNCTION__,"No space to set event name for %s-%d",name, id);
}

int32_t EventHandlerDescriptor::GetEventId(char* name){
    if ((name == NULL) || (strlen(name)==0)) {
        ESP_LOGW(__FUNCTION__,"Missing name for descriptor");
        return -1;
    }
    for (int idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        EventDescriptor_t* descriptor = &this->eventDescriptors[idx];
        if ((descriptor != NULL) && (descriptor->eventName != NULL) && (strcmp(descriptor->eventName,name)==0)){
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

EventHandlerDescriptor::templateType_t EventHandlerDescriptor::GetTemplateType(char* term) {
    if (indexOf(term+2,"Status.")==term+2) {
        return EventHandlerDescriptor::templateType_t::Status;
    }
    if (indexOf(term+2,"Config.")==term+2) {
        return EventHandlerDescriptor::templateType_t::Config;
    }
    if (indexOf(term+2,"CurrentDateNoSpace")==term+2) {
        return EventHandlerDescriptor::templateType_t::CurrentDateNoSpace;
    }
    if (indexOf(term+2,"CurrentDate")==term+2) {
        return EventHandlerDescriptor::templateType_t::CurrentDate;
    }
    if (indexOf(term+2,"RAM")==term+2) {
        return EventHandlerDescriptor::templateType_t::RAM;
    }
    if (indexOf(term+2,"Battery")==term+2) {
        return EventHandlerDescriptor::templateType_t::Battery;
    }
    return EventHandlerDescriptor::templateType_t::Invalid;
}

char* EventHandlerDescriptor::GetParsedValue(const char* sourceValue){
    if (!sourceValue || !strlen(sourceValue)) {
        return NULL;
    }
    ESP_LOGV(__FUNCTION__,"before:%s", sourceValue);
    char* sourceShadow = (char*)dmalloc(strlen(sourceValue)+1);
    uint32_t retLen = strlen(sourceShadow)+200;
    char* termName = NULL;
    char* srcCursor = sourceShadow;
    char* retCursor=(char*)dmalloc(retLen);
    char* retVal = retCursor;
    char* openPos=sourceShadow;
    char* closePos=NULL;
    char* lastClosePos=NULL;
    char strftime_buf[64];
    cJSON* jtmp=NULL;
    memset(retCursor,0,retLen);
    struct tm timeinfo;
    time_t now = 0;
    time(&now);
    localtime_r(&now, &timeinfo);
    EventHandlerDescriptor::templateType_t tt;
    
    strcpy(sourceShadow,sourceValue);

    AppConfig* cfg = NULL;
    while ((openPos=indexOf(openPos,"${")) && (closePos=indexOf(openPos,"}"))) {
        lastClosePos=closePos;
        if (openPos != sourceShadow) {
            memcpy(retCursor,sourceShadow,openPos-sourceShadow);
            ESP_LOGV(__FUNCTION__,"added:%s(%d)", retCursor,openPos-sourceShadow);
        }
        retCursor+=strlen(retCursor);
        *closePos=0;
        switch (tt=GetTemplateType(openPos))
        {
        case EventHandlerDescriptor::templateType_t::Config:
        case EventHandlerDescriptor::templateType_t::Status:
            cfg = tt==EventHandlerDescriptor::templateType_t::Status?AppConfig::GetAppStatus():AppConfig::GetAppConfig();
            ESP_LOGV(__FUNCTION__,"Getting %s", tt==EventHandlerDescriptor::templateType_t::Status?"Status":"Config");
            termName=indexOf(openPos,".");
            if (termName) {
                termName++;
                ESP_LOGV(__FUNCTION__,"termname:%s", termName);
                if (cfg->HasProperty(termName)){
                    if (cfg->isItemObject(termName)) {
                        cJSON_PrintPreallocated(cfg->GetJSONConfig(termName),retCursor,200,false);
                        ESP_LOGV(__FUNCTION__,"added json:%s", retCursor);
                    } else {
                        jtmp = cfg->GetPropertyHolder(cfg->GetJSONConfig(termName));
                        if (cJSON_IsNumber(jtmp)) {
                            sprintf(strftime_buf,"%f",cJSON_GetNumberValue(jtmp));
                            ESP_LOGV(__FUNCTION__,"Number Value %s", strftime_buf);
                            strcpy(retCursor,strftime_buf);
                        } else if (cJSON_IsBool(jtmp)) {
                            *retCursor=cJSON_IsTrue(jtmp)?'Y':'N';
                            ESP_LOGV(__FUNCTION__,"Bool Value %s", cJSON_IsTrue(jtmp)?"Y":"N");
                        } else {
                            strcpy(retCursor,cfg->GetStringProperty(termName));
                            ESP_LOGV(__FUNCTION__,"String Value:%s", retCursor);
                        }
                    }
                } else {
                    ESP_LOGD(__FUNCTION__,"Cannot find %s in %s", termName, tt==EventHandlerDescriptor::templateType_t::Status?"Status":"Config");
                }
            }
            break;
        case EventHandlerDescriptor::templateType_t::CurrentDate:
            strftime(strftime_buf, sizeof(strftime_buf), "%Y/%m/%d %H:%M:%S", &timeinfo);
            strcpy(retCursor, strftime_buf);
            ESP_LOGV(__FUNCTION__,"added datetime:%s", retCursor);
            break;
        case EventHandlerDescriptor::templateType_t::RAM:
            sprintf(strftime_buf, "%d", esp_get_free_heap_size());
            strcpy(retCursor, strftime_buf);
            ESP_LOGV(__FUNCTION__,"added RAM:%s", retCursor);
            break;
        case EventHandlerDescriptor::templateType_t::Battery:
            sprintf(strftime_buf, "%f", getBatteryVoltage());
            strcpy(retCursor, strftime_buf);
            ESP_LOGV(__FUNCTION__,"added Batterry:%s", retCursor);
            break;
        case EventHandlerDescriptor::templateType_t::CurrentDateNoSpace:
            strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d", &timeinfo);
            strcpy(retCursor, strftime_buf);
            ESP_LOGV(__FUNCTION__,"added datetime:%s", retCursor);
            break;
        case EventHandlerDescriptor::templateType_t::Invalid:
            break;
        }
        sourceShadow=openPos=closePos+1;
        retCursor+=strlen(retCursor);
        ESP_LOGV(__FUNCTION__,"now at:%s", sourceShadow);
    }

    if (lastClosePos != NULL) {
        strcpy(retCursor,lastClosePos+1);
        ESP_LOGV(__FUNCTION__,"Added last chunck:%s", lastClosePos+1);
        ESP_LOGV(__FUNCTION__,"Translated:%s", retVal);
    } else {
        strcpy(retVal,srcCursor);
        ESP_LOGV(__FUNCTION__,"Straight copy:%s", retVal);
    }

    ldfree(srcCursor);
    return retVal;
}
