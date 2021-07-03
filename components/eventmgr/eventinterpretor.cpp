#include "./eventmgr.h"
#include "../rest/rest.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventInterpretor::EventInterpretor(cJSON *json)
{
    memset(conditions, 0, 5 * sizeof(void *));
    memset(isAnd, 0, 5 * sizeof(bool));
    id = -1;
    handler = NULL;
    eventBase = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "eventBase"), "value")->valuestring;
    eventId = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "eventId"), "value")->valuestring;
    method = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "method"), "value")->valuestring;
    params = cJSON_GetObjectItem(json, "params");
    if (cJSON_HasObjectItem(json, "conditions"))
    {
        cJSON *condition = NULL;
        int idx = 0;
        char* boper;
        cJSON_ArrayForEach(condition, cJSON_GetObjectItem(json, "conditions"))
        {
            if (cJSON_HasObjectItem(condition, "operator"))
            {
                conditions[idx] = new EventCondition(condition);
            }
            if (cJSON_HasObjectItem(condition, "boper"))
            {
                boper = cJSON_GetObjectItem(condition, "boper")->valuestring;
                if (boper == NULL) {
                    boper = cJSON_GetObjectItem(cJSON_GetObjectItem(condition, "boper"),"value")->valuestring;
                }
                isAnd[idx++] = strcmp("and", boper);
            }
        }
    }
}

bool EventInterpretor::IsValid(EventHandlerDescriptor *handler, int32_t id, void *event_data)
{
    if (this->handler == NULL)
    {
        char *eventName = handler->GetEventName(id);
        if (eventName == NULL)
        {
            ESP_LOGV(__FUNCTION__, "%s-%d No Event Registered", handler->GetName(), id);
            return NULL;
        }
        if ((strcmp(this->eventBase, handler->GetName()) == 0) && (strcmp(eventName, this->eventId) == 0))
        {
            this->handler = handler;
            this->id = id;
            ESP_LOGD(__FUNCTION__, "%s-%s Event Registered", handler->GetName(), eventName);
        }
    }

    bool ret = (this->handler != NULL) && (handler->GetEventBase() == this->handler->GetEventBase()) && (id == this->id);
    if (ret)
    {
        ESP_LOGV(__FUNCTION__, "%s-%d Match", handler->GetName(), id);
        for (int idx = 0; idx < 5; idx++)
        {
            if (conditions[idx])
            {
                ret = conditions[idx]->IsEventCompliant(event_data);
                if (!ret && (idx < 4) && (isAnd[idx + 1]))
                {
                    return ret;
                }
            }
        }
    }
    return ret;
}

void EventInterpretor::RunIt(EventHandlerDescriptor *handler, int32_t id, void *event_data)
{
    system_event_info_t *systemEventInfo = NULL;
    cJSON *jeventbase;
    cJSON *jeventid;
    int32_t eventId = -1;
    esp_err_t ret;
    esp_event_base_t eventBase;
    if (strcmp(method, "commitTripToDisk") == 0)
    {
        jeventbase = cJSON_GetObjectItem(cJSON_GetObjectItem(params, "flags"), "value");
        xTaskCreate(commitTripToDisk, 
                    "commitTripToDisk", 
                    8192, 
                    (void *)(cJSON_HasObjectItem(params, "flags") ? cJSON_GetObjectItem(cJSON_GetObjectItem(params, "flags"), "value")->valueint:BIT3), 
                    tskIDLE_PRIORITY, 
                    NULL);
    }
    if (strcmp(method, "wifiSallyForth") == 0)
    {
        xTaskCreate(wifiSallyForth, "wifiSallyForth", 8192, NULL, tskIDLE_PRIORITY, NULL);
    }
    if (strcmp(method, "wifioff") == 0)
    {
        TheWifi::GetInstance()->wifiStop(NULL);
    }
    if (strcmp(method, "PullStation") == 0)
    {
        systemEventInfo = (system_event_info_t *)event_data;
        if (systemEventInfo != NULL)
        {
            ESP_LOGD(__FUNCTION__, "%s running %s", handler->GetName(), "pullStation");
            esp_ip4_addr_t *addr = (esp_ip4_addr_t *)dmalloc(sizeof(esp_ip4_addr_t));
            memcpy(addr, &systemEventInfo->ap_staipassigned.ip, sizeof(esp_ip4_addr_t));
            xTaskCreate(pullStation, "pullStation", 4096, (void *)addr, tskIDLE_PRIORITY, NULL);
        }
    }
    if (strcmp(method, "Post") == 0)
    {
        jeventbase = cJSON_GetObjectItem(cJSON_GetObjectItem(params, "eventBase"), "value");
        jeventid = cJSON_GetObjectItem(cJSON_GetObjectItem(params, "eventId"), "value");
        if ((jeventbase == NULL) || (jeventid == NULL))
        {
            ESP_LOGW(__FUNCTION__, "Missing event id or base");
            return;
        }
        if (strcmp(jeventbase->valuestring, "DigitalPin") == 0)
        {
            eventId = handler->GetEventId(jeventid->valuestring);
            if (eventId == -1)
            {
                ESP_LOGW(__FUNCTION__, "bad event id:%s", jeventid->valuestring);
                return;
            }
        }
        ESP_LOGV(__FUNCTION__, "Posting %s to %s(%d)", jeventid->valuestring, jeventbase->valuestring, eventId);
        if ((ret = esp_event_post(handler->GetEventBase(), eventId, &params, sizeof(void *), portMAX_DELAY)) != ESP_OK)
        {
            ESP_LOGW(__FUNCTION__, "Cannot post %s to %s:%s", jeventid->valuestring, jeventbase->valuestring, esp_err_to_name(ret));
        }
    }
}

EventCondition::EventCondition(cJSON *json)
    : compareOperation(GetCompareOperator(json)), valType(GetEntityType(json, "src")), valOrigin(GetOriginType(json, "src")), compType(GetEntityType(json, "comp")), compOrigin(GetOriginType(json, "comp")), isValid(true), compStrVal(NULL), eventJsonPropName(NULL), compIntValue(0), compDblValue(0.0)
{
    char *ctmp;
    cJSON *comp = cJSON_GetObjectItem(json, "comp");

    isValid = valType != compare_entity_type_t::Invalid &&
              compType != compare_entity_type_t::Invalid &&
              compareOperation != compare_operation_t::Invalid &&
              valOrigin != compare_origin_t::Invalid &&
              compOrigin != compare_origin_t::Invalid &&
              valType == compType &&
              valOrigin != compOrigin;

    if (isValid)
    {
        switch (compType)
        {
        case compare_entity_type_t::Fractional:
            compDblValue = cJSON_GetObjectItem(comp, "value")->valuedouble;
            break;
        case compare_entity_type_t::Integer:
            compIntValue = cJSON_GetObjectItem(comp, "value")->valueint;
            break;
        case compare_entity_type_t::String:
            compStrVal = cJSON_GetObjectItem(comp, "value")->valuestring;
            break;
        default:
            isValid = false;
            break;
        }
    }
    else
    {
        char *stmp = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__, "Invalid compare operation in %s", stmp);
        ESP_LOGW(__FUNCTION__, "valType:%d compType:%d compareOperation:%d valOrigin:%d compOrigin:%d", (int)valType, (int)compType, (int)compareOperation, (int)valOrigin, (int)compOrigin);
        ldfree(stmp);
    }
}

void EventCondition::PrintState()
{
    if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    {
        ESP_LOGV(__FUNCTION__, "isValid:%d compareOperation:%d valType:%d valOrigin:%d,compType:%d compOrigin:%d compStrVal:%s eventJsonPropName:%s compIntValue:%d compDblValue:%f",
                 isValid,
                 (int)compareOperation,
                 (int)valType,
                 (int)valOrigin,
                 (int)compType,
                 (int)compOrigin,
                 compStrVal == NULL ? "*null*" : compStrVal,
                 eventJsonPropName == NULL ? "*null*" : eventJsonPropName,
                 compIntValue,
                 compDblValue);
    }
}

bool EventCondition::IsEventCompliant(void *event_data)
{
    if (!IsValid())
    {
        return false;
    }
    double dcval = 0.0;
    int icval = 0;
    char *scval = NULL;
    PrintState();
    switch (compType)
    {
    case compare_entity_type_t::Fractional:
        dcval = *((double *)event_data);
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
        icval = *((int *)event_data);
        ;
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            ESP_LOGV(__FUNCTION__, "%d==%d", icval, compIntValue);
            return icval == compIntValue;
            break;
        case compare_operation_t::Bigger:
            ESP_LOGV(__FUNCTION__, "%d>%d", icval, compIntValue);
            return icval > compIntValue;
            break;
        case compare_operation_t::BiggerOrEqual:
            ESP_LOGV(__FUNCTION__, "%d>=%d", icval, compIntValue);
            return icval >= compIntValue;
            break;
        case compare_operation_t::Smaller:
            ESP_LOGV(__FUNCTION__, "%d<%d", icval, compIntValue);
            return icval < compIntValue;
            break;
        case compare_operation_t::SmallerOrEqual:
            ESP_LOGV(__FUNCTION__, "%d<=%d", icval, compIntValue);
            return icval <= compIntValue;
            break;
        default:
            return false;
            break;
        }
        break;
    case compare_entity_type_t::String:
        scval = (char *)event_data;
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            return strcmp(scval, compStrVal) == 0;
            break;
        case compare_operation_t::RexEx:
            return std::regex_search(std::string(scval), regexp, std::regex_constants::match_default);
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

bool EventCondition::IsValid()
{
    return isValid;
}

compare_operation_t EventCondition::GetCompareOperator(cJSON *json)
{
    if (json == NULL)
    {
        ESP_LOGW(__FUNCTION__, "Missing input param");
        return compare_operation_t::Invalid;
    }
    cJSON *val = cJSON_GetObjectItem(json, "operator");
    if (val == NULL)
    {
        char *sjson = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__, "Invalid operator:%s", sjson);
        ldfree(sjson);
        return compare_operation_t::Invalid;
    }
    if (strncmp("==", val->valuestring, 2) == 0)
    {
        return compare_operation_t::Equal;
    }
    if (strncmp(">=", val->valuestring, 2) == 0)
    {
        return compare_operation_t::BiggerOrEqual;
    }
    if (strncmp("<=", val->valuestring, 2) == 0)
    {
        return compare_operation_t::SmallerOrEqual;
    }
    if (strncmp("~=", val->valuestring, 2) == 0)
    {
        return compare_operation_t::RexEx;
    }
    if (strncmp(">", val->valuestring, 1) == 0)
    {
        return compare_operation_t::Bigger;
    }
    if (strncmp("<", val->valuestring, 1) == 0)
    {
        return compare_operation_t::Smaller;
    }
    char *sjson = cJSON_Print(json);
    ESP_LOGW(__FUNCTION__, "Invalid operator value:%s", sjson);
    ldfree(sjson);
    return compare_operation_t::Invalid;
}

compare_entity_type_t EventCondition::GetEntityType(cJSON *json, char *fldName)
{
    if ((json == NULL) || (fldName == NULL) || (strlen(fldName) == 0))
    {
        ESP_LOGW(__FUNCTION__, "Missing input param");
        return compare_entity_type_t::Invalid;
    }
    cJSON *val = cJSON_GetObjectItem(cJSON_GetObjectItem(json, fldName), "otype");
    if ((val == NULL) || (val->valuestring == NULL))
    {
        char *sjson = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__, "Invalid operator(%s):%s", fldName, sjson);
        ldfree(sjson);
        return compare_entity_type_t::Invalid;
    }
    if (strncmp("fractional", val->valuestring, 10) == 0)
    {
        return compare_entity_type_t::Fractional;
    }
    if (strncmp("integer", val->valuestring, 7) == 0)
    {
        return compare_entity_type_t::Integer;
    }
    if (strncmp("string", val->valuestring, 6) == 0)
    {
        return compare_entity_type_t::String;
    }
    char *sjson = cJSON_Print(json);
    ESP_LOGW(__FUNCTION__, "Invalid value:%s", sjson);
    ldfree(sjson);
    return compare_entity_type_t::Invalid;
}

compare_origin_t EventCondition::GetOriginType(cJSON *json, char *fldName)
{
    if ((json == NULL) || (fldName == NULL) || (strlen(fldName) == 0))
    {
        ESP_LOGW(__FUNCTION__, "Missing input param");
        return compare_origin_t::Invalid;
    }
    cJSON *val = cJSON_GetObjectItem(json, fldName);
    if (val == NULL)
    {
        char *sjson = cJSON_Print(json);
        ESP_LOGW(__FUNCTION__, "Invalid origin:%s", sjson);
        ldfree(sjson);
        return compare_origin_t::Invalid;
    }
    if (cJSON_HasObjectItem(val, "name"))
    {
        return compare_origin_t::Event;
    }
    return compare_origin_t::Litteral;
}
