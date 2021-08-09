#include "eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static EventManager* runningInstance=NULL;

EventManager::EventManager(cJSON* cfg, cJSON* programs)
:config(cfg)
,programs(programs)
{
    memset(eventInterpretors,0,sizeof(void*)*MAX_NUM_EVENTS);
    if (!ValidateConfig()) {
        ESP_LOGE(__FUNCTION__,"Event manager is invalid");
    }
}

EventManager* EventManager::GetInstance(){
    if (runningInstance == NULL) {
        runningInstance = new EventManager(AppConfig::GetAppConfig()->GetJSONConfig("/events"),
                                           AppConfig::GetAppConfig()->GetJSONConfig("/programs"));
    }
    return runningInstance;
}

cJSON* EventManager::GetConfig(){
    return config;
}

bool EventManager::ValidateConfig(){
    uint8_t idx=0;
    bool isValid = true;
    cJSON* event;
    cJSON_ArrayForEach(event,config) {
        if (!cJSON_HasObjectItem(event,"eventBase")){
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing event base");
        }
        if (!cJSON_HasObjectItem(event,"eventId")){
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing event id");
        }
        if (!cJSON_HasObjectItem(event,"method") && !cJSON_HasObjectItem(event,"program")){
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing method..");
        }
        if (cJSON_HasObjectItem(event,"program") && !programs) {
            isValid=false;
            ESP_LOGW(__FUNCTION__,"Missing programs");
        }
        if (isValid){
            eventInterpretors[idx++] = new EventInterpretor(event,programs);
        }else{
            char* json = cJSON_PrintUnformatted(event);
            ESP_LOGW(__FUNCTION__,"Event:%s",json);
            free(json);
        }
    }
    return isValid;
}

void EventManager::RegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_LOGD(__FUNCTION__,"Registering %s",(char*)eventHandlerDescriptor->GetEventBase());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::ProcessEvent, eventHandlerDescriptor, NULL));
    ESP_LOGD(__FUNCTION__,"Done Registering %s",(char*)eventHandlerDescriptor->GetEventBase());
}

void EventManager::UnRegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_LOGV(__FUNCTION__,"UnRegistering %s",(char*)eventHandlerDescriptor->GetEventBase());
    ESP_ERROR_CHECK(esp_event_handler_unregister(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::ProcessEvent));
}

void EventManager::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    EventHandlerDescriptor* handler = (EventHandlerDescriptor*)handler_args;
    uint8_t idx =0;
    EventInterpretor* interpretor;
    if ((strcmp(handler->GetName(),"GPSPLUS_EVENTS") != 0) || (!(id && BIT7|BIT0))){
        ESP_LOGV(__FUNCTION__,"Event::::%s-%d",handler->GetName(),id);
    }
    while ((idx < MAX_NUM_EVENTS) && ((interpretor = EventManager::GetInstance()->eventInterpretors[idx++])!=NULL)){
        if (interpretor->IsValid(handler,id,event_data)) {
            if (interpretor->IsProgram()) {
                interpretor->RunProgram(handler,id,event_data,interpretor->GetProgramName());
            } else {
                interpretor->RunMethod(handler,id,event_data);
            }
        }
    }
}

