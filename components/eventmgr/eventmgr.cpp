#include "eventmgr.h"
#include "route.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static EventManager* runningInstance=NULL;

EventManager::EventManager(cJSON* cfg, cJSON* programs)
:config(cfg)
,programs(programs)
,eventBuffer(NULL)
,eventQueue(xQueueCreate(20, sizeof(postedEvent_t)))
{
    memset(eventInterpretors,0,sizeof(void*)*MAX_NUM_EVENTS);
    if (!ValidateConfig()) {
        ESP_LOGE(__FUNCTION__,"Event manager is invalid");
    }
    ESP_LOGV(__FUNCTION__,"Event Manager Running");
    CreateBackgroundTask(EventPoller,"EventPoller",4096,this,tskIDLE_PRIORITY,NULL);
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
        }
    }
    ESP_LOGI(__FUNCTION__,"We have %d Events",idx);
    if (eventQueue == NULL) {
        ESP_LOGE(__FUNCTION__,"Event Queue not set");
        isValid=false;
    }
    return isValid;
}

void EventManager::RegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_LOGV(__FUNCTION__,"Registering %s",(char*)eventHandlerDescriptor->GetEventBase());
    if (runningInstance == NULL) {
        ESP_LOGV(__FUNCTION__,"Getting EventManager instance");
        runningInstance = EventManager::GetInstance();
        ESP_ERROR_CHECK(esp_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, EventManager::EventProcessor, runningInstance, NULL));
    }
}

void EventManager::UnRegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor) {
    ESP_LOGV(__FUNCTION__,"UnRegistering %s",(char*)eventHandlerDescriptor->GetEventBase());
    ESP_ERROR_CHECK(esp_event_handler_unregister(eventHandlerDescriptor->GetEventBase(), ESP_EVENT_ANY_ID, EventManager::EventProcessor));
}

void EventManager::EventPoller(void* param){
    postedEvent_t* postedEvent = (postedEvent_t*)dmalloc(sizeof(postedEvent_t));
    memset(postedEvent,0,sizeof(postedEvent_t));
    EventManager* mgr = (EventManager*)param;
    EventGroupHandle_t appEg = getAppEG();
    ESP_LOGI(__FUNCTION__,"Event Poller Running");
    while(!(xEventGroupGetBits(appEg) & app_bits_t::HIBERNATE) && xQueueReceive(mgr->eventQueue,postedEvent,portMAX_DELAY)){
        ESP_LOGV(__FUNCTION__,"Processing Event %s %d",postedEvent->base, postedEvent->id);
        ProcessEvent(postedEvent);
        memset(postedEvent,0,sizeof(postedEvent_t));
    }
    ldfree(postedEvent);
}

void EventManager::ProcessEvent(postedEvent_t* postedEvent){
    if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
        EventDescriptor_t* desc = EventHandlerDescriptor::GetEventDescriptor(postedEvent->base,postedEvent->id);
        if (desc) {
            ESP_LOGV(__FUNCTION__,"Event %s %s",desc->baseName, desc->eventName);
        } else {
            ESP_LOGV(__FUNCTION__,"Parsing Event %s %d",postedEvent->base, postedEvent->id);
        }
    }
    EventManager* evtMgr = EventManager::GetInstance();
    if (WebsocketManager::HasOpenedWs()){
        EventDescriptor_t* edesc = EventHandlerDescriptor::GetEventDescriptor(postedEvent->base,postedEvent->id);
        if (edesc != NULL) {
            cJSON* jevt = cJSON_CreateObject();

            if (evtMgr->eventBuffer == NULL) {
                evtMgr->eventBuffer = (char*) dmalloc(JSON_BUFFER_SIZE);
            }
            cJSON_AddStringToObject(jevt,"eventBase",postedEvent->base);
            cJSON_AddStringToObject(jevt,"eventId",edesc->eventName);
            cJSON* jpl = NULL;
            switch (postedEvent->eventDataType)
            {
            case event_data_type_tp::JSON:
                jpl = cJSON_Parse((const char*)postedEvent->event_data);
                if (jpl) {
                    cJSON_AddItemToObject(jevt,"data",jpl);
                } else if (cJSON_IsInvalid((cJSON*)postedEvent->event_data)) {
                    ESP_LOGW(__FUNCTION__,"Cannot jsonify data for %s %s %s",edesc->baseName,edesc->eventName, (char*)postedEvent->event_data);
                } else {
                    cJSON_AddItemReferenceToObject(jevt,"data",(cJSON*)postedEvent->event_data);
                }
                break;
            case event_data_type_tp::String:
                cJSON_AddStringToObject(jevt,"data",(char*)postedEvent->event_data);
                break;
            case event_data_type_tp::Number:
                ESP_LOGV(__FUNCTION__,"Got %s(%d) %d",postedEvent->base,postedEvent->id ,(int)postedEvent->event_data);
                cJSON_AddNumberToObject(jevt,"data",(int)postedEvent->event_data);
                break;
            default:
                ESP_LOGV(__FUNCTION__,"Unknown event data type for %s-%s",edesc->baseName,edesc->eventName);
                break;
            }
            if (cJSON_PrintPreallocated(jevt,evtMgr->eventBuffer,JSON_BUFFER_SIZE,false)){
                WebsocketManager::EventCallback(evtMgr->eventBuffer);
            }
            cJSON_Delete(jevt);
        }
    } else if (evtMgr->eventBuffer) {
        ldfree(evtMgr->eventBuffer);
        evtMgr->eventBuffer = NULL;
    }
    EventInterpretor* interpretor;
    uint8_t idx =0;
    while ((idx < MAX_NUM_EVENTS) && 
           ((interpretor = evtMgr->eventInterpretors[idx++])!=NULL)) {
        if (interpretor->IsValid(postedEvent->base,postedEvent->id,postedEvent->event_data)){
            ESP_LOGV(__FUNCTION__,"Running event at idx %d for %s",idx-1, postedEvent->base);
            if (interpretor->IsProgram()) {
                ESP_LOGV(__FUNCTION__,"Running program %s at idx %d", postedEvent->base, idx);
                interpretor->RunProgram(interpretor->GetProgramName());
            } else {
                interpretor->RunMethod(NULL);
            }
        }
    }
    if ((postedEvent->eventDataType == event_data_type_tp::JSON) ||
        (postedEvent->eventDataType == event_data_type_tp::String)){
        ldfree(postedEvent->event_data);
    }
}

void EventManager::EventProcessor(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    if (!base || !strlen(base)) {
        ESP_LOGE(__FUNCTION__,"Missing base name");
        return;
    }
    EventManager* mgr = (EventManager*) handler_args;
    ESP_LOGV(__FUNCTION__,"Base:%s id:%d eventQueue:%" PRIXPTR "",base,id,(uintptr_t)mgr->eventQueue);
    EventDescriptor_t* ed=EventHandlerDescriptor::GetEventDescriptor(base,id);
    if (!ed) {
        ESP_LOGV(__FUNCTION__,"No desciptor for %s %d", base, id);
    } else {
        postedEvent_t* postedEvent = (postedEvent_t*)dmalloc(sizeof(postedEvent_t));
        postedEvent->base=base;
        postedEvent->id=id;
        postedEvent->eventDataType=ed->dataType;
        ESP_LOGV(__FUNCTION__,"type for %s %s(%d) is %d", postedEvent->base, ed->eventName, postedEvent->id, (int)ed->dataType);
        switch (ed->dataType)
        {
        case event_data_type_tp::String:
        case event_data_type_tp::JSON:
            postedEvent->event_data=dmalloc(strlen((char*)event_data)+1);
            strcpy((char*)postedEvent->event_data,(char*)event_data);
            ESP_LOGV(__FUNCTION__,"json (%s)", (char*)postedEvent->event_data);
            break;
        case event_data_type_tp::Number:
            postedEvent->event_data=(void*)*(int*)event_data;
            break;
        default:
            break;
        }
        xQueueSendFromISR(mgr->eventQueue, postedEvent, NULL);
        ldfree(postedEvent);
    }
}

static ManagedThreads* theInstance = NULL;

ManagedThreads* ManagedThreads::GetInstance() {
    if (theInstance == NULL) {
        theInstance = new ManagedThreads();
    }
    return theInstance;
}