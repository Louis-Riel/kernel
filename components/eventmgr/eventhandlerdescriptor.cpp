#include "eventmgr.h"
#include "../../main/utils.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventDescriptor_t* EventHandlerDescriptor::eventDescriptorCache = NULL;
uint32_t EventHandlerDescriptor::numCacheEntries = 0;

EventHandlerDescriptor::EventHandlerDescriptor(esp_event_base_t eventBase,const char* name){
    this->eventBase = eventBase;
    this->eventDescriptors = (EventDescriptor_t*)dmalloc(sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS_PER_BASE);
    ESP_LOGV(__FUNCTION__,"Event descriptor for base %s with name %s created", eventBase, name);
    memset(this->eventDescriptors,0,sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS_PER_BASE);
    for (int idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        this->eventDescriptors[idx].id=-1;
    }
    if (EventHandlerDescriptor::eventDescriptorCache == NULL) {
        ESP_LOGV(__FUNCTION__,"Initing Event descriptor cache from base %s with name %s", eventBase, name);
        EventHandlerDescriptor::eventDescriptorCache = (EventDescriptor_t*)dmalloc(sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS);
        memset(EventHandlerDescriptor::eventDescriptorCache,0,sizeof(EventDescriptor_t)*MAX_NUM_HANDLERS);
        for (int idx = 0; idx < MAX_NUM_HANDLERS; idx++) {
            EventHandlerDescriptor::eventDescriptorCache[idx].id=-1;
        }
    }
    this->name = (char*)dmalloc(strlen(name)+1);
    strcpy(this->name,name);
}

EventHandlerDescriptor::~EventHandlerDescriptor(){
    ldfree(eventDescriptors);
}

bool EventHandlerDescriptor::AddEventDescriptor(int32_t id,const char* eventName){
    return AddEventDescriptor(id,eventName,event_data_type_tp::Unknown);
}

bool EventHandlerDescriptor::AddEventDescriptor(int32_t id,const char* eventName,event_data_type_tp dtp){
    EventDescriptor_t* desc = GetEventDescriptor(eventBase,id);
    if (desc == NULL) {
        ESP_LOGV(__FUNCTION__,"Caching event descriptor %s(%d) registered in %s at idx %d", eventName,id,this->eventBase,EventHandlerDescriptor::numCacheEntries);
        desc=&EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries];
        EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries].id = id;
        EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries].baseName = name;
        EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries].eventName = (char*)dmalloc(strlen(eventName)+1);
        strcpy((char*)EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries].eventName, eventName);
        EventHandlerDescriptor::eventDescriptorCache[EventHandlerDescriptor::numCacheEntries++].dataType = dtp;
    }

    if (desc){
        for (int idx = 0; idx < MAX_NUM_HANDLERS; idx++) {
            EventDescriptor_t* descriptor = &this->eventDescriptors[idx];
            if (descriptor->id == -1){
                descriptor->id = desc->id;
                descriptor->eventName = desc->eventName;
                descriptor->baseName = desc->baseName;
                descriptor->dataType = desc->dataType;
                ESP_LOGV(__FUNCTION__,"Event descriptor %s(%d) registered in %s at idx %d", eventName,id,this->eventBase, idx);
                return true;
            }
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Event descriptor %s(%d) NOT cached in %s", eventName,id,this->eventBase);
    }


    ESP_LOGE(__FUNCTION__,"Event descriptor %s(%d) NOT registered in %s", eventName,id,this->eventBase);
    return false;
}

char* EventHandlerDescriptor::GetName(){
    return this->name;
}

EventDescriptor_t* EventHandlerDescriptor::GetEventDescriptor(esp_event_base_t base,const char* eventName) {
    ESP_LOGV(__FUNCTION__,"GetEventDescriptor %s(%s)", eventName, base);
    if ((base == NULL) || (eventName == NULL)) {
        ESP_LOGW(__FUNCTION__,"Missing base(%d) or name(%d)",base == NULL,eventName == NULL);
    }
    for (int idx = 0; idx < numCacheEntries; idx++) {
        EventDescriptor_t* ei = &EventHandlerDescriptor::eventDescriptorCache[idx];
        ESP_LOGV(__FUNCTION__,"%s-%s == %s-%s",base,eventName,ei->baseName,ei->eventName);
        if ((ei != NULL) && 
            (ei->baseName != NULL) &&
            (ei->eventName != NULL) &&
            (indexOf(ei->baseName,base) != NULL) &&
            (indexOf(ei->eventName,eventName) != NULL)) {
            ESP_LOGV(__FUNCTION__,"got a match");
            return ei;
        }
    }
    ESP_LOGV(__FUNCTION__,"no match");

    return NULL;
}

EventDescriptor_t* EventHandlerDescriptor::GetEventDescriptor(esp_event_base_t base,uint32_t id) {
    ESP_LOGV(__FUNCTION__,"Looking for GetEventDescriptor %s(%d)-%d",  base, id, numCacheEntries);
    for (int idx = 0; idx < numCacheEntries; idx++) {
        EventDescriptor_t* ei = &EventHandlerDescriptor::eventDescriptorCache[idx];
        if (ei->id == -1)
            continue;

        if ((ei != NULL) && 
            (ei->baseName != NULL) &&
            (ei->id >= 0) &&
            (base != NULL) && 
            (indexOf(ei->baseName,base) != NULL) &&
            (ei->id == id)) {
            ESP_LOGV(__FUNCTION__,"GetEventDescriptor %s(%d)-%d",  ei->baseName, ei->id, idx);
            return ei;
        }
    }
    ESP_LOGV(__FUNCTION__,"no match");
    return NULL;
}

void EventHandlerDescriptor::SetEventName(int32_t id,const char* name){
    for (uint8_t idx = 0; idx < MAX_NUM_HANDLERS_PER_BASE; idx++) {
        if (eventDescriptors[idx].eventName == NULL) {
            ESP_LOGV(__FUNCTION__,"%d:%s set at idx:%d", id, name, idx);
            eventDescriptors[idx].id = id;
            eventDescriptors[idx].eventName=name;
            return;
        }
    }
    ESP_LOGE(__FUNCTION__,"No space to set event name for %s-%d",name, id);
}

int32_t EventHandlerDescriptor::GetEventId(const char* name){
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

const char* EventHandlerDescriptor::GetEventName(int32_t id){
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

EventHandlerDescriptor::templateType_t EventHandlerDescriptor::GetTemplateType(const char* term) {
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
    uint32_t retLen = strlen(sourceValue)+200;
    char* retCursor=(char*)dmalloc(retLen);
    memset(retCursor,0,retLen);

    char* srcCursor = (char*)dmalloc(strlen(sourceValue)+1);
    char* srcCopy = srcCursor;
    strcpy(srcCursor,sourceValue);
    char* retVal = retCursor;
    char* termName = NULL;
    char* openPos=srcCursor;
    char* closePos=NULL;
    char* lastClosePos=NULL;
    char* strftime_buf = (char*)dmalloc(64);

    cJSON* jtmp=NULL;
    struct tm timeinfo;
    time_t now = 0;
    time(&now);
    localtime_r(&now, &timeinfo);
    EventHandlerDescriptor::templateType_t tt;

    AppConfig* cfg = NULL;
    AppConfig* jstat = AppConfig::GetAppStatus();
    AppConfig* jcfg = AppConfig::GetAppConfig();

    while ((openPos=indexOf(openPos,"${")) && (closePos=indexOf(openPos,"}"))) {
        lastClosePos=closePos;
        if (openPos != srcCursor) {
            memcpy(retCursor,srcCursor,openPos-srcCursor);
            ESP_LOGV(__FUNCTION__,"added:%s(%d)", retCursor,openPos-srcCursor);
        }
        retCursor+=strlen(retCursor);
        *closePos=0;
        switch (tt=GetTemplateType(openPos))
        {
        case EventHandlerDescriptor::templateType_t::Config:
        case EventHandlerDescriptor::templateType_t::Status:
            cfg = tt==EventHandlerDescriptor::templateType_t::Status?jstat:jcfg;
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
                        jtmp = cfg->GetPropertyHolder(termName);
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
                    ESP_LOGI(__FUNCTION__,"Cannot find %s in %s", termName, tt==EventHandlerDescriptor::templateType_t::Status?"Status":"Config");
                }
            }
            break;
        case EventHandlerDescriptor::templateType_t::CurrentDate:
            strftime(strftime_buf, 64, "%Y/%m/%d %H:%M:%S", &timeinfo);
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
            strftime(strftime_buf, 64, "%Y-%m-%d", &timeinfo);
            strcpy(retCursor, strftime_buf);
            ESP_LOGV(__FUNCTION__,"added datetime:%s - %s", retCursor, strftime_buf);
            break;
        case EventHandlerDescriptor::templateType_t::Invalid:
            break;
        }
        srcCursor=openPos=closePos+1;
        retCursor+=strlen(retCursor);
        ESP_LOGV(__FUNCTION__,"now at:%s", srcCursor);
    }

    if (lastClosePos != NULL) {
        strcpy(retCursor,lastClosePos+1);
        ESP_LOGV(__FUNCTION__,"Added last chunck:%s", lastClosePos+1);
        ESP_LOGV(__FUNCTION__,"Translated:%s", retVal);
    } else {
        strcpy(retVal,srcCursor);
        ESP_LOGV(__FUNCTION__,"Straight copy:%s", retVal);
    }

    ldfree(strftime_buf);
    ldfree(srcCopy);
    return retVal;
}
