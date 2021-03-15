#include "./eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventManager* EventManager::instance=NULL;

EventManager::EventManager(cJSON* cfg)
:config(NULL)
{
    EventManager::instance=this;
    memset(eventInterpretors,0,sizeof(void*)*MAX_NUM_EVENTS);
    SetConfig(cfg);
}

cJSON* EventManager::GetConfig(){
    return instance->config;
}

cJSON* EventManager::SetConfig(cJSON* config){
    if (instance->config != NULL) {
        cJSON_free(instance->config);
    }
    instance->config = config;
    uint8_t idx=0;
    cJSON* event;
    bool isValid = true;
    cJSON_ArrayForEach(event,instance->config) {
        if (!cJSON_HasObjectItem(event,"eventBase")){
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing event base");
        }
        if (!cJSON_HasObjectItem(event,"eventId")){
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing event id");
        }
        if (!cJSON_HasObjectItem(event,"method")){
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing method");
        }
        if (isValid){
            instance->eventInterpretors[idx++] = new EventInterpretor(event);
        }else{
            char* json = cJSON_Print(event);
            ESP_LOGW(__FUNCTION__,"Event:%s",json);
            free(json);
        }
    }
    return instance->config;
}

void EventManager::RegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_ERROR_CHECK(esp_event_handler_register(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::ProcessEvent, eventHandlerDescriptor));
}

void EventManager::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    EventHandlerDescriptor* handler = (EventHandlerDescriptor*)handler_args;
    uint8_t idx =0;
    EventInterpretor* interpretor;
    while ((idx < MAX_NUM_EVENTS) && ((interpretor = EventManager::instance->eventInterpretors[idx++])!=NULL)){
        if (interpretor->IsValid(handler,id,event_data)) {
            interpretor->RunIt(handler,id,event_data);
        }
    }
}
