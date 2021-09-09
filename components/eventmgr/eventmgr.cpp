#include "eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static EventManager* runningInstance=NULL;

EventManager::EventManager(cJSON* cfg, cJSON* programs)
:config(cfg)
,programs(programs)
{
    memset(eventInterpretors,0,sizeof(void*)*MAX_NUM_EVENTS);
    esp_err_t ret=0;
// typedef struct {
//     int32_t queue_size;                         /**< size of the event loop queue */
//     const char *task_name;                      /**< name of the event loop task; if NULL,
//                                                         a dedicated task is not created for event loop*/
//     UBaseType_t task_priority;                  /**< priority of the event loop task, ignored if task name is NULL */
//     uint32_t task_stack_size;                   /**< stack size of the event loop task, ignored if task name is NULL */
//     BaseType_t task_core_id;                    /**< core to which the event loop task is pinned to,
//                                                         ignored if task name is NULL */
// } esp_event_loop_args_t;

    const esp_event_loop_args_t loopArgs = {
        .queue_size=5,
        .task_name="EventQueuer",
        .task_priority=10,
        .task_stack_size=8192,
        .task_core_id=1};
    //if ((ret=esp_event_loop_create(&loopArgs,&evtMgrLoopHandle)) != ESP_OK){
    //    ESP_LOGE(__FUNCTION__,"Failed in creating event loop %s",esp_err_to_name(ret));
    //}
    if (!ValidateConfig()) {
        ESP_LOGE(__FUNCTION__,"Event manager is invalid");
    }
    ESP_LOGV(__FUNCTION__,"Event Manager Running");
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
    ESP_LOGV(__FUNCTION__,"Registering %s",(char*)eventHandlerDescriptor->GetEventBase());
    if (runningInstance == NULL) {
        runningInstance = EventManager::GetInstance();
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_register(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::ProcessEvent, eventHandlerDescriptor, NULL));
    ESP_LOGV(__FUNCTION__,"Done Registering %s",(char*)eventHandlerDescriptor->GetEventBase());
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
                interpretor->RunProgram(handler,id,NULL,interpretor->GetProgramName());
            } else {
                interpretor->RunMethod(handler,id,NULL);
            }
        }
    }
}

