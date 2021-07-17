#include "./eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static EventManager* runningInstance=NULL;

EventManager::EventManager(cJSON* cfg)
:config(NULL)
{
    if (runningInstance == NULL) {
        runningInstance = this;
    }
    ESP_LOGV(__FUNCTION__,"EventManager");
    memset(eventInterpretors,0,sizeof(void*)*MAX_NUM_EVENTS);
    SetConfig(cfg);
}

EventManager* EventManager::GetInstance(){
    if (runningInstance == NULL) {
        runningInstance = new EventManager(AppConfig::GetAppConfig()->GetJSONConfig("/events"));
    }
    return runningInstance;
}

cJSON* EventManager::GetConfig(){
    return config;
}

cJSON* EventManager::SetConfig(cJSON* config){
    if (config != NULL) {
        cJSON_free(this->config);
    }
    this->config = config;
    uint8_t idx=0;
    cJSON* event;
    bool isValid = true;
    cJSON_ArrayForEach(event,config) {
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
            eventInterpretors[idx++] = new EventInterpretor(event);
        }else{
            char* json = cJSON_Print(event);
            ESP_LOGW(__FUNCTION__,"Event:%s",json);
            free(json);
        }
    }
    return EventManager::GetInstance()->config;
}

void EventManager::RegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_LOGD(__FUNCTION__,"Registering %s",(char*)eventHandlerDescriptor->GetEventBase());
    ESP_ERROR_CHECK(esp_event_handler_register(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::ProcessEvent, eventHandlerDescriptor));
}

void EventManager::UnRegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_LOGV(__FUNCTION__,"UnRegistering %s",(char*)eventHandlerDescriptor->GetEventBase());
    ESP_ERROR_CHECK(esp_event_handler_unregister(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::ProcessEvent));
}

void EventManager::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    EventHandlerDescriptor* handler = (EventHandlerDescriptor*)handler_args;
    uint8_t idx =0;
    EventInterpretor* interpretor;
    ESP_LOGV(__FUNCTION__,"Event::::%s %d",handler->GetName(),EventManager::GetInstance()==NULL);
    while ((idx < MAX_NUM_EVENTS) && ((interpretor = EventManager::GetInstance()->eventInterpretors[idx++])!=NULL)){
        if (interpretor->IsValid(handler,id,event_data)) {
            interpretor->RunIt(handler,id,event_data);
        }
    }
}
