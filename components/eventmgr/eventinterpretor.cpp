#include "./eventmgr.h"
#include "../rest/rest.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventInterpretor::EventInterpretor(cJSON* json):condition(NULL){
    id = -1;
    handler = NULL;
    eventBase=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"eventBase"),"value")->valuestring;
    eventId=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"eventId"),"value")->valuestring;
    method=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"method"),"value")->valuestring;
    params=cJSON_GetObjectItem(json,"params");
    if (cJSON_HasObjectItem(json,"condition")){
        condition = new EventCondition(cJSON_GetObjectItem(json,"condition"));
    }
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
        if (condition != NULL) {
            return condition->IsEventCompliant(event_data);
        }
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

EventCondition::EventCondition(cJSON* json)
:compareOperation(GetCompareOperator(json))
,valType(GetEntityType(json,"src"))
,valOrigin(GetOriginType(json,"src"))
,compType(GetEntityType(json,"comp"))
,compOrigin(GetOriginType(json,"comp"))
,isValid(true)
,eventJsonPropName(NULL)
{
    char* ctmp;
    cJSON* comp = cJSON_GetObjectItem(json,"comp");

    isValid = valType != compare_entity_type_t::Invalid && 
              compType != compare_entity_type_t::Invalid && 
              compareOperation != compare_operation_t::Invalid &&
              valOrigin != compare_origin_t::Invalid &&
              compOrigin != compare_origin_t::Invalid &&
              valType == compType &&
              valOrigin != compOrigin;

    if (isValid){
        switch (compType)
        {
        case compare_entity_type_t::Fractional:
            compDblValue = cJSON_GetObjectItem(comp,"value")->valuedouble;
            break;
        case compare_entity_type_t::Integer:
            compIntValue = cJSON_GetObjectItem(comp,"value")->valueint;
            break;
        case compare_entity_type_t::String:
            compStrVal = cJSON_GetObjectItem(comp,"value")->valuestring;
            break;
        default:
            isValid = false;
            break;
        }
    } else {
        char* stmp = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__,"Invalid compare operation in %s",stmp);
        ESP_LOGW(__FUNCTION__,"valType:%d compType:%d compareOperation:%d valOrigin:%d compOrigin:%d",(int)valType,(int)compType,(int)compareOperation,(int)valOrigin,(int)compOrigin);
        free(stmp);
    }
}

bool EventCondition::IsEventCompliant(void *event_data){
    if (!IsValid()){
        return false;
    }
    double dcval=0.0;
    int icval=0;
    char* scval = NULL;
    switch (compType)
    {
    case compare_entity_type_t::Fractional:
        dcval=*((double*)event_data);
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            return dcval == compDblValue;
            break;
        case compare_operation_t::Bigger:
            return dcval > compDblValue;
            break;
        case compare_operation_t::BiggerOrEqual:
            return dcval >= compDblValue;
            break;
        case compare_operation_t::Smaller:
            return dcval < compDblValue;
            break;
        case compare_operation_t::SmallerOrEqual:
            return dcval <= compDblValue;
            break;
        default:
            return false;
            break;
        }
        break;
    case compare_entity_type_t::Integer:
        icval=*((int*)event_data);;
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            ESP_LOGV(__FUNCTION__,"%d==%d",icval, compIntValue);
            return icval == compIntValue;
            break;
        case compare_operation_t::Bigger:
            return icval > compIntValue;
            break;
        case compare_operation_t::BiggerOrEqual:
            return icval >= compIntValue;
            break;
        case compare_operation_t::Smaller:
            return icval < compIntValue;
            break;
        case compare_operation_t::SmallerOrEqual:
            return icval <= compIntValue;
            break;
        default:
            return false;
            break;
        }
        break;
    case compare_entity_type_t::String:
        scval=(char*)event_data;
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            return strcmp(scval,compStrVal) == 0;
            break;
        case compare_operation_t::RexEx:
            return std::regex_search(std::string(scval),regexp,std::regex_constants::match_default);
            break;
        default:
            return false;
            break;
        }
        break;
    default:
        return false;
        break;
    }
}

bool EventCondition::IsValid(){
    return isValid;
}

compare_operation_t EventCondition::GetCompareOperator(cJSON* json){
    if (json == NULL) {
        ESP_LOGW(__FUNCTION__,"Missing input param");
        return compare_operation_t::Invalid;
    }
    cJSON* val = cJSON_GetObjectItem(json,"operator");
    if (val == NULL) {
        char* sjson = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__,"Invalid operator:%s",sjson);
        free(sjson);
        return compare_operation_t::Invalid;
    }
    if (strncmp("==",val->valuestring,2) == 0){
        return compare_operation_t::Equal;    
    }
    if (strncmp(">",val->valuestring,1) == 0){
        return compare_operation_t::Bigger;    
    }
    if (strncmp(">=",val->valuestring,2) == 0){
        return compare_operation_t::BiggerOrEqual;    
    }
    if (strncmp("<",val->valuestring,1) == 0){
        return compare_operation_t::Smaller;    
    }
    if (strncmp("<=",val->valuestring,2) == 0){
        return compare_operation_t::SmallerOrEqual;    
    }
    if (strncmp("~=",val->valuestring,2) == 0){
        return compare_operation_t::RexEx;    
    }
    char* sjson = cJSON_Print(json);
    ESP_LOGW(__FUNCTION__,"Invalid operator value:%s",sjson);
    free(sjson);
    return compare_operation_t::Invalid;
}

compare_entity_type_t EventCondition::GetEntityType(cJSON* json,char* fldName){
    if ((json == NULL) || (fldName == NULL) || (strlen(fldName) == 0)) {
        ESP_LOGW(__FUNCTION__,"Missing input param");
        return compare_entity_type_t::Invalid;
    }
    cJSON* val = cJSON_GetObjectItem(cJSON_GetObjectItem(json,fldName),"otype");
    if ((val == NULL) || (val->valuestring == NULL)) {
        char* sjson = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__,"Invalid operator(%s):%s",fldName, sjson);
        free(sjson);
        return compare_entity_type_t::Invalid;
    }
    if (strncmp("fractional",val->valuestring,10) == 0){
        return compare_entity_type_t::Fractional;    
    }
    if (strncmp("integer",val->valuestring,7) == 0){
        return compare_entity_type_t::Integer;    
    }
    if (strncmp("string",val->valuestring,6) == 0){
        return compare_entity_type_t::String;    
    }
    char* sjson = cJSON_Print(json);
    ESP_LOGW(__FUNCTION__,"Invalid value:%s",sjson);
    free(sjson);
    return compare_entity_type_t::Invalid;
}

compare_origin_t EventCondition::GetOriginType(cJSON* json,char* fldName){
    if ((json == NULL) || (fldName == NULL) || (strlen(fldName) == 0)) {
        ESP_LOGW(__FUNCTION__,"Missing input param");
        return compare_origin_t::Invalid;
    }
    cJSON* val = cJSON_GetObjectItem(json,fldName);
    if (val == NULL) {
        char* sjson = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__,"Invalid origin:%s",sjson);
        free(sjson);
        return compare_origin_t::Invalid;
    }
    if (cJSON_HasObjectItem(val,"name")) {
        return compare_origin_t::Event;        
    }
    return compare_origin_t::Litteral;
}