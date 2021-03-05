#include "./eventmgr.h"
#include "../rest/rest.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventInterpretor::EventInterpretor(cJSON* json){
    id = -1;
    handler = NULL;
    eventBase=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"eventBase"),"value")->valuestring;
    eventId=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"eventId"),"value")->valuestring;
    method=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"method"),"value")->valuestring;
    params=cJSON_GetObjectItem(json,"params");
}

bool EventInterpretor::IsValid(EventHandlerDescriptor *handler, int32_t id, void *event_data){
    if (this->handler == NULL) {
        char* eventName = handler->GetEventName(id);
        if (eventName == NULL) {
            ESP_LOGW(__FUNCTION__,"%s-%d No Event Registered",handler->GetName(),id);
            return NULL;
        }
        if ((strcmp(this->eventBase,handler->GetName())==0) && (strcmp(eventName,this->eventId) ==0)) {
            this->handler = handler;
            this->id = id;
            ESP_LOGD(__FUNCTION__,"%s-%s Event Registered",handler->GetName(),eventName);
        }
    }

    bool ret = (this->handler != NULL) && (handler->GetEventBase() == this->handler->GetEventBase()) && (id == this->id);
    if (ret) {
        ESP_LOGV(__FUNCTION__,"%s-%d Match",handler->GetName(),id);
    }
    return ret;
}

void EventInterpretor::RunIt(EventHandlerDescriptor *handler, int32_t id,void *event_data){
    system_event_info_t* systemEventInfo=NULL;
    cJSON* jeventbase;
    cJSON* jeventid;
    int32_t eventId=-1;
    esp_err_t ret;
    esp_event_base_t eventBase;
    if (strcmp(method,"PullStation") == 0){
        systemEventInfo = (system_event_info_t*)event_data;
        if (systemEventInfo != NULL) {
            ESP_LOGD(__FUNCTION__,"%s running %s",handler->GetName(),"pullStation");
            esp_ip4_addr_t* addr = (esp_ip4_addr_t*)malloc(sizeof(esp_ip4_addr_t));
            memcpy(addr,&systemEventInfo->ap_staipassigned.ip,sizeof(esp_ip4_addr_t));
            xTaskCreate(pullStation, "pullStation", 4096, (void*)addr , tskIDLE_PRIORITY, NULL);
        }
    }
    if (strcmp(method,"Post") == 0){
        jeventbase = cJSON_GetObjectItem(cJSON_GetObjectItem(params,"eventBase"),"value");
        jeventid = cJSON_GetObjectItem(cJSON_GetObjectItem(params,"eventId"),"value");
        if ((jeventbase == NULL) || (jeventid == NULL)) {
            ESP_LOGW(__FUNCTION__,"Missing event id or base");
            return;
        }
        if (strcmp(jeventbase->valuestring,"DigitalPin") == 0) {
            eventId = handler->GetEventId(jeventid->valuestring);
            if (eventId == -1){
                ESP_LOGW(__FUNCTION__,"bad event id:%s",jeventid->valuestring);
                return;
            }
        }
        ESP_LOGV(__FUNCTION__,"Posting %s to %s(%d)",jeventid->valuestring,jeventbase->valuestring,eventId);
        if ((ret=esp_event_post(handler->GetEventBase(),eventId,&params,sizeof(void*),portMAX_DELAY))!=ESP_OK){
            ESP_LOGW(__FUNCTION__,"Cannot post %s to %s:%s",jeventid->valuestring,jeventbase->valuestring,esp_err_to_name(ret));
        }
    }
}
