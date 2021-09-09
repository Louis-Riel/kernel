#include "eventmgr.h"
#include "rest.h"
#include "route.h"
#include "mfile.h"
#include "pins.h"
#include "lwip/inet.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventInterpretor::EventInterpretor(cJSON *json, cJSON* programs)
:programs(programs)
,config(json)
,programName(NULL)
,method(NULL)
,params(NULL)
,app_eg(getAppEG())
{
    AppConfig* cfg = new AppConfig(json,NULL);
    memset(conditions, 0, 5 * sizeof(void *));
    memset(isAnd, 0, 5 * sizeof(bool));
    id = -1;
    handler = NULL;
    eventBase = cfg->GetStringProperty("eventBase");
    eventId = cfg->GetStringProperty("eventId");
    if (cfg->HasProperty("method"))
        method = cfg->GetStringProperty("method");
    if (cJSON_HasObjectItem(json,"params")){
        params = cfg->GetJSONConfig("params");
    }
    if (cfg->HasProperty("program") && programs){
        programName = cfg->GetStringProperty("program");
    }

    if (!method && !programName) {
        ESP_LOGE(__FUNCTION__,"Invalid Event, missing both method and program name and programs is %snull", programs?"not ":"");
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
            char* tmp = cJSON_Print(json);
            ESP_LOGV(__FUNCTION__,"%s",tmp);
            ldfree(tmp);
        }
    }
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
            return NULL;
        }
        if ((strcmp(this->eventBase, handler->GetName()) == 0) && (strcmp(eventName, this->eventId) == 0))
        {
            this->handler = handler;
            this->id = id;
            ESP_LOGV(__FUNCTION__, "%s %s %s-%s-%d Event Registered", IsProgram()?"Program ":"Method",IsProgram()?programName:method, handler->GetName(), eventName, id);
        }
    }


    bool ret = (handler != NULL) && (this->handler != NULL) && (handler->GetEventBase() == this->handler->GetEventBase()) && (id == this->id);
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

bool EventInterpretor::IsProgram() {
    return (programName != NULL);
}

char* EventInterpretor::GetProgramName() {
    if (IsProgram()) {
        return programName;
    }
    return NULL;
}

void EventInterpretor::RunProgram(const char* programName){
    RunProgram(NULL,0,NULL,programName);
}

void EventInterpretor::RunProgram(EventHandlerDescriptor *handler, int32_t id, void *event_data, const char* programName){
    AppConfig* program=NULL;
    cJSON *prog;
    if (!programs) {
        ESP_LOGE(__FUNCTION__,"Missing programs, cannot launch %s",programName);
        return;
    }
    cJSON_ArrayForEach(prog,programs) {
        AppConfig* aprog = new AppConfig(prog,NULL);
        if (aprog->HasProperty("name") && (strcmp(aprog->GetStringProperty("name"),programName)==0)) {
            program = aprog;
            break;
        }
        free(aprog);
    }
    if (!program) {
        ESP_LOGE(__FUNCTION__, "Cannot fing a program named %s", programName);
        return;
    }
    ESP_LOGV(__FUNCTION__,"Running program %s", programName);
    if (program->HasProperty("inLineThreads")) {
        cJSON* item;
        cJSON* jprog = program->GetJSONConfig(NULL);
        ESP_LOGV(__FUNCTION__,"Running inline %s",programName);
        if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE) {
            char* ctmp = cJSON_Print(jprog);
            ESP_LOGV(__FUNCTION__,"jprog:%s",ctmp);
            ldfree(ctmp);
        }
        cJSON_ArrayForEach(item,cJSON_GetObjectItem(jprog,"inLineThreads")) {
            if ((item == NULL) || cJSON_IsInvalid(item)) {
                ESP_LOGV(__FUNCTION__,"Unparsable JSON");
                continue;
            }
            if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE) {
                char* ctmp = cJSON_Print(item);
                ESP_LOGV(__FUNCTION__,"item:%s",ctmp);
                ldfree(ctmp);
            }
            AppConfig* aitem = new AppConfig(item,NULL);
            if (aitem->HasProperty("method")) {
                cJSON* mParams = aitem->HasProperty("params") ? aitem->GetJSONConfig("params") : NULL;
                if ((mParams != NULL) && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)) {
                    char* ctmp = cJSON_Print(mParams);
                    ESP_LOGV(__FUNCTION__,"mParams:%s",ctmp);
                    ldfree(ctmp);
                }
                RunMethod(handler,id, mParams ? &mParams : NULL, aitem->GetStringProperty("method"),false);
            } else if (aitem->HasProperty("program")) {
                if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE){
                    char* tmp = cJSON_Print(aitem->GetJSONConfig(NULL));
                    ESP_LOGV(__FUNCTION__,"running program from %s", tmp);
                    ldfree(tmp);
                }
                RunProgram(handler,id,event_data,aitem->GetStringProperty("program"));
            } else if (item != NULL) {
                char* tmp = cJSON_Print(item);
                ESP_LOGE(__FUNCTION__,"Nothing to run for %s",tmp);
                ldfree(tmp);
            }
            free(aitem);
        }
    } else if (program->HasProperty("parallelThreads")) {
        cJSON* item;
        uint32_t threads = 0;
        ESP_LOGV(__FUNCTION__,"Running parallel %s",programName);
        cJSON_ArrayForEach(item,cJSON_GetObjectItem(program->GetJSONConfig(NULL),"parallelThreads")) {
            AppConfig* aitem = new AppConfig(item,NULL);
            if (aitem->HasProperty("method")) {
                uint8_t bitno = RunMethod(handler,id,event_data,aitem->GetStringProperty("method"),true);
                if (bitno != UINT8_MAX) {
                    threads |= (1<<bitno);
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot get a thread slot for %s",aitem->GetStringProperty("method"));
                }
            } else if (aitem->HasProperty("program")) {
                ESP_LOGW(__FUNCTION__,"Program backgroup task not supported, running inline");
                RunProgram(handler,id,event_data,aitem->GetStringProperty("program"));
            } else {
                char* tmp = cJSON_Print(item);
                ESP_LOGE(__FUNCTION__,"Nothing to run for %s",tmp);
                ldfree(tmp);
            }
            free(aitem);
        }
        if (threads) {
            managedThreads.WaitForThreads(threads);
            ESP_LOGD(__FUNCTION__,"Program %s done %d",programName, threads);
        } else {
            ESP_LOGW(__FUNCTION__,"No threads started");
        }
    } else if (program->HasProperty("program")) {
        if (program->HasProperty("period")) {
            LoopyArgs* args = (LoopyArgs*)dmalloc(sizeof(LoopyArgs));
            args->interpretor=this;
            args->program=program->GetJSONConfig(NULL);
            xTaskCreate(RunLooper,program->GetStringProperty("program"),4096,(void*)args,tskIDLE_PRIORITY,NULL);
        } else {
            ESP_LOGE(__FUNCTION__,"Missing Period");
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Missing the thing to run in program %s",programName);
    }
}

void EventInterpretor::RunLooper(void* param) {
    if (param == NULL){
        ESP_LOGE(__FUNCTION__,"Missing Looper Params");
        vTaskDelete(NULL);
    }
    LoopyArgs* la = (LoopyArgs*)param;
    EventInterpretor* interpretor = la->interpretor;
    AppConfig* program = new AppConfig(la->program,NULL);
    ldfree(la);
    uint32_t period = program->GetIntProperty("period");
    TickType_t ticks;
    bool done = false;
    char* progName = program->GetStringProperty("program");
    ESP_LOGV(__FUNCTION__,"Looping %s every %dms",progName,period);
    while (!done) {
        interpretor->RunProgram(progName);
        vTaskDelayUntil(&ticks, pdMS_TO_TICKS( period ));
    }
    ldfree(program);
}

uint8_t EventInterpretor::RunMethod(EventHandlerDescriptor *handler, int32_t id, void *event_data) {
    if (!method) {
        ESP_LOGE(__FUNCTION__,"Missing method.");
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
            char* tmp = cJSON_Print(config);
            ESP_LOGV(__FUNCTION__,"%s",tmp);
            ldfree(tmp);
        }
        return UINT8_MAX;
    }
    ESP_LOGV(__FUNCTION__,"Running inline method %s", method);
    return RunMethod(handler,id,event_data,method,true);
}

char* EventInterpretor::ToString() {
    return cJSON_Print(config);
}

uint8_t EventInterpretor::RunMethod(EventHandlerDescriptor *handler, int32_t id, void *event_data, const char* method, bool inBackground)
{
    cJSON *jeventbase;
    cJSON *jeventid;
    int32_t eventId = -1;
    esp_err_t ret;

    if (!method) {
        ESP_LOGE(__FUNCTION__,"Missing method");
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
            char* tmp = cJSON_Print(config);
            ESP_LOGV(__FUNCTION__,"%s",tmp);
            ldfree(tmp);
        }
        return UINT8_MAX;
    }

    ESP_LOGV(__FUNCTION__,"Got %s method:(%s) with id %d",inBackground?"backgroung":"foreground",method,id);

    cJSON* mParams = event_data == NULL ? params : *(cJSON**)event_data;
    if (mParams && cJSON_HasObjectItem(mParams,"params")) {
        ESP_LOGV(__FUNCTION__,"Passing method params");
        mParams = cJSON_GetObjectItem(mParams,"params");
    } 
    if ((mParams) && (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)) {
        char* tmp = cJSON_Print(mParams);
        ESP_LOGV(__FUNCTION__,"%s",tmp);
        ldfree(tmp);
    }

    TaskFunction_t theFunc = NULL;
    
    if (strcmp(method, "commitTripToDisk") == 0)
    {
        theFunc = commitTripToDisk;
        jeventbase = cJSON_GetObjectItem(cJSON_GetObjectItem(params, "flags"), "value");
    } else if (strcmp(method, "wifioff") == 0)
    {
        ESP_LOGD(__FUNCTION__, "%s from %s", handler->GetName(), "wifioff");
        xEventGroupClearBits(app_eg,app_bits_t::WIFI_ON);
        xEventGroupSetBits(app_eg,app_bits_t::WIFI_OFF);
        return UINT8_MAX;
    } else if (strcmp(method, "mergeconfig") == 0)
    {
        theFunc = TheRest::MergeConfig;
    } else if (strcmp(method, "sendstatus") == 0)
    {
        theFunc = TheRest::SendStatus;
    } else if (strcmp(method, "healthcheck") == 0)
    {
        theFunc = ManagedDevice::RunHealthCheck;
    } else if (strcmp(method, "wifiSallyForth") == 0)
    {
        xEventGroupSetBits(app_eg,app_bits_t::WIFI_ON);
        xEventGroupClearBits(app_eg,app_bits_t::WIFI_OFF);
        return UINT8_MAX;
    } else if (strcmp(method, "checkupgrade") == 0)
    {
        theFunc = TheRest::CheckUpgrade;
    } else if (strcmp(method, "sendtar") == 0)
    {
        theFunc = TheRest::SendTar;
    } else if (strcmp(method,"Sleep")==0) {
        cJSON* jtime = cJSON_GetObjectItem(mParams, "time");
        if (jtime && (jtime = cJSON_GetObjectItem(jtime,"value"))) {
            ESP_LOGV(__FUNCTION__,"Sleeping for %dms", jtime->valueint);
            vTaskDelay(pdMS_TO_TICKS(jtime->valueint));
            ESP_LOGV(__FUNCTION__,"Wokeup after %dms", jtime->valueint);
        } else {
            ESP_LOGW(__FUNCTION__,"Missing param sleep");
        }
        return UINT8_MAX;
    } else if (strcmp(method, "Post") == 0) {
        jeventbase = cJSON_GetObjectItem(cJSON_GetObjectItem(mParams, "eventBase"), "value");
        jeventid = cJSON_GetObjectItem(cJSON_GetObjectItem(mParams, "eventId"), "value");
        if ((jeventbase == NULL) || (jeventid == NULL))
        {
            char* tmp = cJSON_Print(mParams);
            ESP_LOGW(__FUNCTION__, "Missing event id or base:%s",tmp == NULL ? "null" : tmp);
            if (tmp)
                ldfree(tmp);
            return UINT8_MAX;
        }
        EventDescriptor_t* edesc = handler->GetEventDescriptor(jeventbase->valuestring,jeventid->valuestring);
        if (strcmp(jeventbase->valuestring,"MFile")==0) {
            BufferedFile::ProcessEvent(NULL,edesc->baseName,edesc->id, &mParams);
        } else if (strcmp(jeventbase->valuestring,"DigitalPin")==0) {
            Pin::ProcessEvent(NULL,edesc->baseName,edesc->id, &mParams);
        } else if (edesc) {
            ESP_LOGV(__FUNCTION__, "Posting %s to %s(%d)..", edesc->eventName, edesc->baseName, edesc->id);
            if ((ret = esp_event_post(edesc->baseName, edesc->id, &mParams, sizeof(void *), portMAX_DELAY)) != ESP_OK)
            {
                ESP_LOGW(__FUNCTION__, "Cannot post %s to %s:%s..", jeventid->valuestring, jeventbase->valuestring, esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(__FUNCTION__, "Cannot find %s to %s..", jeventid->valuestring, jeventbase->valuestring);
        }
        return UINT8_MAX;
    }
    if (inBackground)
        return CreateWokeBackgroundTask(theFunc, method, 4096, NULL, tskIDLE_PRIORITY, NULL);
    else
        CreateWokeInlineTask(theFunc, method, 4096, NULL);
    return UINT8_MAX;
}

EventCondition::EventCondition(cJSON *json)
    : compareOperation(GetCompareOperator(json)), valType(GetEntityType(json, "src")), valOrigin(GetOriginType(json, "src")), compType(GetEntityType(json, "comp")), compOrigin(GetOriginType(json, "comp")), isValid(true), compStrVal(NULL), eventJsonPropName(NULL), compIntValue(0), compDblValue(0.0)
{
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
        char *stmp = cJSON_PrintUnformatted(json);
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
        char *sjson = cJSON_PrintUnformatted(json);
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
    char *sjson = cJSON_PrintUnformatted(json);
    ESP_LOGW(__FUNCTION__, "Invalid operator value:%s", sjson);
    ldfree(sjson);
    return compare_operation_t::Invalid;
}

compare_entity_type_t EventCondition::GetEntityType(cJSON *json, const char *fldName)
{
    if ((json == NULL) || (fldName == NULL) || (strlen(fldName) == 0))
    {
        ESP_LOGW(__FUNCTION__, "Missing input param");
        return compare_entity_type_t::Invalid;
    }
    cJSON *val = cJSON_GetObjectItem(cJSON_GetObjectItem(json, fldName), "otype");
    if ((val == NULL) || (val->valuestring == NULL))
    {
        char *sjson = cJSON_PrintUnformatted(json);
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
    char *sjson = cJSON_PrintUnformatted(json);
    ESP_LOGW(__FUNCTION__, "Invalid value:%s", sjson);
    ldfree(sjson);
    return compare_entity_type_t::Invalid;
}

compare_origin_t EventCondition::GetOriginType(cJSON *json, const char *fldName)
{
    if ((json == NULL) || (fldName == NULL) || (strlen(fldName) == 0))
    {
        ESP_LOGW(__FUNCTION__, "Missing input param");
        return compare_origin_t::Invalid;
    }
    cJSON *val = cJSON_GetObjectItem(json, fldName);
    if (val == NULL)
    {
        char *sjson = cJSON_PrintUnformatted(json);
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
