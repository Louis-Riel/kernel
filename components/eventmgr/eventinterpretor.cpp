#include "eventmgr.h"
#include "rest.h"
#include "route.h"
#include "mfile.h"
#include "pins.h"
#include "lwip/inet.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

EventInterpretor::EventInterpretor(cJSON *json, cJSON *programs)
    : programs(programs), config(json), programName(NULL), method(NULL), params(NULL), app_eg(getAppEG())
{
    AppConfig *cfg = new AppConfig(json, NULL);
    memset(conditions, 0, 5 * sizeof(void *));
    memset(isAnd, 0, 5 * sizeof(bool));
    eventBase = cfg->GetStringProperty("eventBase");
    eventId = cfg->GetStringProperty("eventId");
    id = -1;
    if (cfg->HasProperty("method"))
        method = cfg->GetStringProperty("method");
    if (cJSON_HasObjectItem(json, "params"))
    {
        params = cfg->GetJSONConfig("params");
    }
    if (cfg->HasProperty("program") && programs)
    {
        programName = cfg->GetStringProperty("program");
    }
    delete cfg;
    if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
    {
        char *tmp = cJSON_Print(json);
        ESP_LOGV(__FUNCTION__, "%s", tmp);
        ldfree(tmp);
    }

    if (!method && !programName)
    {
        ESP_LOGE(__FUNCTION__, "Invalid Event, missing both method and program name and programs is %snull", programs ? "not " : "");
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            char *tmp = cJSON_Print(json);
            ESP_LOGV(__FUNCTION__, "%s", tmp);
            ldfree(tmp);
        }
    }
    if (cJSON_HasObjectItem(json, "conditions"))
    {
        cJSON *condition = NULL;
        int idx = 0;
        char *boper;
        cJSON_ArrayForEach(condition, cJSON_GetObjectItem(json, "conditions"))
        {
            if (cJSON_HasObjectItem(condition, "operator"))
            {
                conditions[idx] = new EventCondition(condition);
            }
            if (cJSON_HasObjectItem(condition, "boper"))
            {
                boper = cJSON_GetObjectItem(condition, "boper")->valuestring;
                if (boper == NULL)
                {
                    boper = cJSON_GetObjectItem(cJSON_GetObjectItem(condition, "boper"), "value")->valuestring;
                }
                isAnd[idx++] = strcmp("and", boper);
            }
        }
    }
}

bool EventInterpretor::IsValid(esp_event_base_t eventBase, int32_t id, void *event_data)
{
    if (this->id == -1)
    {
        EventDescriptor_t *desc = EventHandlerDescriptor::GetEventDescriptor(this->eventBase, this->eventId);
        if (desc)
        {
            this->id = desc->id;
            ESP_LOGD(__FUNCTION__, "Registered as %s %d", this->eventBase, this->id);
        }
        else
        {
            //ESP_LOGV(__FUNCTION__, "No descriptor for %s-%s yet", this->eventBase, this->eventId);
            return false;
        }
    }
    if (id > 128)
        ESP_LOGV(__FUNCTION__, "(this->eventBase(%s) == eventBase(%s)) && (id(%d) == this->id(%d))", this->eventBase, eventBase, id, this->id);
    if ((strcmp(this->eventBase, eventBase) == 0) && (id == this->id))
    {
        ESP_LOGD(__FUNCTION__, "%s-%d Match", eventBase, id);
        bool ret = true;
        for (int idx = 0; idx < 5; idx++)
        {
            if (conditions[idx])
            {
                ESP_LOGD(__FUNCTION__, "%s-%d Checking condition %d", eventBase, id, idx);
                ret = conditions[idx]->IsEventCompliant(event_data);
                if (!ret && (idx < 4) && (isAnd[idx + 1]))
                {
                    ESP_LOGD(__FUNCTION__, "%s-%d Matching condition %d", eventBase, id, idx);
                    return ret;
                }
            }
        }
        return ret;
    } else {
        if (id > 128)
            ESP_LOGV(__FUNCTION__, "%s-%d Non Matching", eventBase, id);
        return false;
    }
}

bool EventInterpretor::IsProgram()
{
    return (programName != NULL);
}

const char *EventInterpretor::GetProgramName()
{
    if (IsProgram())
    {
        return programName;
    }
    return NULL;
}

void EventInterpretor::RunProgram(const char *programName)
{
    RunProgram(NULL, programName);
}

void EventInterpretor::RunProgram(void *event_data, const char *programName)
{
    AppConfig *program = NULL;
    cJSON *prog;
    if (!programs)
    {
        ESP_LOGE(__FUNCTION__, "Missing programs, cannot launch %s", programName);
        return;
    }
    cJSON_ArrayForEach(prog, programs)
    {
        AppConfig *aprog = new AppConfig(prog, NULL);
        if (aprog->HasProperty("name") && (strcmp(aprog->GetStringProperty("name"), programName) == 0))
        {
            program = aprog;
            break;
        }
        free(aprog);
    }
    if (!program)
    {
        ESP_LOGE(__FUNCTION__, "Cannot fing a program named %s", programName);
        return;
    }
    ESP_LOGD(__FUNCTION__, "Running program %s", programName);
    if (program->HasProperty("inLineThreads"))
    {
        cJSON *item;
        cJSON *jprog = program->GetJSONConfig(NULL);
        ESP_LOGV(__FUNCTION__, "Running inline %s", programName);
        if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
        {
            char *ctmp = cJSON_Print(jprog);
            ESP_LOGV(__FUNCTION__, "jprog:%s", ctmp);
            ldfree(ctmp);
        }
        cJSON_ArrayForEach(item, cJSON_GetObjectItem(jprog, "inLineThreads"))
        {
            if ((item == NULL) || cJSON_IsInvalid(item))
            {
                ESP_LOGD(__FUNCTION__, "Unparsable JSON");
                continue;
            }
            if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
            {
                char *ctmp = cJSON_Print(item);
                ESP_LOGV(__FUNCTION__, "item:%s:%s", programName, ctmp);
                ldfree(ctmp);
            }
            AppConfig *aitem = new AppConfig(item, NULL);
            if (aitem->HasProperty("method"))
            {
                cJSON *mParams = aitem->HasProperty("params") ? aitem->GetJSONConfig("params") : NULL;
                if ((mParams != NULL) && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE))
                {
                    char *ctmp = cJSON_Print(mParams);
                    ESP_LOGV(__FUNCTION__, "%s mParams:%s",programName, ctmp);
                    ldfree(ctmp);
                }
                RunMethod(aitem->GetStringProperty("method"), mParams ? &mParams : NULL, false);
            }
            else if (aitem->HasProperty("program"))
            {
                if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
                {
                    char *tmp = cJSON_Print(aitem->GetJSONConfig(NULL));
                    ESP_LOGV(__FUNCTION__, "%s running program from %s",programName, tmp);
                    ldfree(tmp);
                }
                RunProgram(event_data, aitem->GetStringProperty("program"));
            }
            else if (item != NULL)
            {
                ESP_LOGE(__FUNCTION__, "Nothing to run for inline thread");
            }
            free(aitem);
        }
    }
    else if (program->HasProperty("parallelThreads"))
    {
        cJSON *item;
        uint32_t threads = 0;
        ESP_LOGV(__FUNCTION__, "Running parallel %s", programName);
        cJSON_ArrayForEach(item, cJSON_GetObjectItem(program->GetJSONConfig(NULL), "parallelThreads"))
        {
            AppConfig *aitem = new AppConfig(item, NULL);
            if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
            {
                char *tmp = cJSON_Print(aitem->GetJSONConfig(NULL));
                ESP_LOGV(__FUNCTION__, "%s running program from %s",programName, tmp);
                ldfree(tmp);
            }
            if (aitem->HasProperty("method"))
            {
                uint8_t bitno = RunMethod(aitem->GetStringProperty("method"), event_data, true);
                if (bitno != UINT8_MAX)
                {
                    threads |= (1 << bitno);
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Cannot get a thread slot for %s", aitem->GetStringProperty("method"));
                }
            }
            else if (aitem->HasProperty("program"))
            {
                ESP_LOGW(__FUNCTION__, "Program backgroup task not supported, running inline");
                RunProgram(event_data, aitem->GetStringProperty("program"));
            }
            else
            {
                char *tmp = cJSON_Print(item);
                ESP_LOGE(__FUNCTION__, "Nothing to run for %s", tmp);
                ldfree(tmp);
            }
            free(aitem);
        }
        if (threads)
        {
            ManagedThreads::GetInstance()->WaitForThreads(threads);
            ESP_LOGD(__FUNCTION__, "Program %s done %d", programName, threads);
        }
        else
        {
            ESP_LOGW(__FUNCTION__, "No threads started");
        }
    }
    else if (program->HasProperty("program"))
    {
        if (program->HasProperty("period"))
        {
            LoopyArgs *args = (LoopyArgs *)dmalloc(sizeof(LoopyArgs));
            args->interpretor = this;
            args->program = program->GetJSONConfig(NULL);
            xTaskCreate(RunLooper, program->GetStringProperty("program"), 4096, (void *)args, tskIDLE_PRIORITY, NULL);
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Missing Period");
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Missing the thing to run in program %s", programName);
    }
}

void EventInterpretor::RunLooper(void *param)
{
    if (param == NULL)
    {
        ESP_LOGE(__FUNCTION__, "Missing Looper Params");
        vTaskDelete(NULL);
    }
    LoopyArgs *la = (LoopyArgs *)param;
    EventInterpretor *interpretor = la->interpretor;
    AppConfig *program = new AppConfig(la->program, NULL);
    ldfree(la);
    uint32_t period = program->GetIntProperty("period");
    TickType_t ticks;
    bool done = false;
    const char *progName = program->GetStringProperty("program");
    ESP_LOGV(__FUNCTION__, "Looping %s every %dms", progName, period);
    while (!done)
    {
        interpretor->RunProgram(progName);
        vTaskDelayUntil(&ticks, pdMS_TO_TICKS(period));
    }
    ldfree(program);
    ldfree(la);
    vTaskDelete(NULL);
}

uint8_t EventInterpretor::RunMethod(void *event_data)
{
    if (!method)
    {
        ESP_LOGE(__FUNCTION__, "Missing method.");
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            char *tmp = cJSON_Print(config);
            ESP_LOGV(__FUNCTION__, "%s", tmp);
            ldfree(tmp);
        }
        return UINT8_MAX;
    }
    ESP_LOGV(__FUNCTION__, "Running inline method %s", method);
    return RunMethod(method, event_data, true);
}

char *EventInterpretor::ToString()
{
    return cJSON_Print(config);
}

static BaseType_t CreateWokeInlineTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters)
{
    return ManagedThreads::GetInstance()->CreateInlineManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, false, true);
}

cJSON *EventInterpretor::GetParams()
{
    return params;
}

uint8_t EventInterpretor::RunMethod(const char *method, void *event_data, bool inBackground)
{
    return RunMethod(this, method, event_data, inBackground);
}

uint8_t EventInterpretor::RunMethod(EventInterpretor *instance, const char *method, void *event_data, bool inBackground)
{
    cJSON *jeventbase;
    cJSON *jeventid;
    esp_err_t ret;
    EventGroupHandle_t app_eg = instance ? instance->app_eg : getAppEG();

    if (!method)
    {
        ESP_LOGE(__FUNCTION__, "Missing method");
        return UINT8_MAX;
    }

    ESP_LOGV(__FUNCTION__, "Got %s method:(%s)", inBackground ? "backgroung" : "foreground", method);

    TaskFunction_t theFunc = NULL;
    cJSON *mParams = event_data == NULL ? instance->params : *(cJSON **)event_data;

    if (strcmp(method, "wifioff") == 0)
    {
        xEventGroupClearBits(app_eg, app_bits_t::WIFI_ON);
        xEventGroupSetBits(app_eg, app_bits_t::WIFI_OFF);
        return UINT8_MAX;
    }
    else if (strcmp(method, "mergeconfig") == 0)
    {
       // theFunc = TheRest::MergeConfig;
    }
    else if (strcmp(method, "sendstatus") == 0)
    {
        theFunc = TheRest::SendStatus;
    }
    else if (strcmp(method, "healthcheck") == 0)
    {
        theFunc = ManagedDevice::RunHealthCheck;
    }
    else if (strcmp(method, "wifiSallyForth") == 0)
    {
        xEventGroupSetBits(app_eg, app_bits_t::WIFI_ON);
        xEventGroupClearBits(app_eg, app_bits_t::WIFI_OFF);
        return UINT8_MAX;
    }
    else if (strcmp(method, "service") == 0)
    {
        if ((jeventbase = cJSON_GetObjectItem(mParams, "service")) &&
            (jeventbase = cJSON_GetObjectItem(jeventbase, "value")) &&
            (jeventid = cJSON_GetObjectItem(mParams, "status")) &&
            (jeventid = cJSON_GetObjectItem(jeventid, "value")))
        {
            if (strcmp(jeventbase->string, "wifi") == 0)
            {
                if (strcmp(jeventid->string, "stop"))
                {
                    xEventGroupSetBits(app_eg, app_bits_t::WIFI_OFF);
                    xEventGroupClearBits(app_eg, app_bits_t::WIFI_ON);
                }
                else
                {
                    xEventGroupSetBits(app_eg, app_bits_t::WIFI_ON);
                    xEventGroupClearBits(app_eg, app_bits_t::WIFI_OFF);
                }
            }
            if (strcmp(jeventbase->string, "gps") == 0)
            {
                if (strcmp(jeventid->string, "stop"))
                {
                    xEventGroupSetBits(app_eg, app_bits_t::GPS_OFF);
                    xEventGroupClearBits(app_eg, app_bits_t::GPS_ON);
                }
                else
                {
                    xEventGroupSetBits(app_eg, app_bits_t::GPS_ON);
                    xEventGroupClearBits(app_eg, app_bits_t::GPS_OFF);
                }
            }
            if (strcmp(jeventbase->string, "rest") == 0)
            {
                if (strcmp(jeventid->string, "stop"))
                {
                    xEventGroupClearBits(app_eg, app_bits_t::REST);
                }
                else
                {
                    xEventGroupSetBits(app_eg, app_bits_t::REST);
                }
            }
        }
        return UINT8_MAX;
    }
    else if (strcmp(method, "hibernate") == 0)
    {
        xEventGroupSetBits(app_eg, app_bits_t::HIBERNATE);
        return UINT8_MAX;
    }
    else if (strcmp(method, "checkupgrade") == 0)
    {
        theFunc = TheRest::CheckUpgrade;
    }
    else if (strcmp(method, "sendtar") == 0)
    {
        theFunc = TheRest::SendTar;
    }
    else if (strcmp(method, "Sleep") == 0)
    {
        cJSON *jtime = cJSON_GetObjectItem(mParams, "time");
        if (jtime && (jtime = cJSON_GetObjectItem(jtime, "value")))
        {
            ESP_LOGV(__FUNCTION__, "Sleeping for %dms", jtime->valueint);
            vTaskDelay(pdMS_TO_TICKS(jtime->valueint));
            ESP_LOGV(__FUNCTION__, "Wokeup after %dms", jtime->valueint);
        }
        else
        {
            char *tmp = cJSON_Print(mParams);
            ESP_LOGW(__FUNCTION__, "Missing param sleep:%s", tmp == NULL ? "null" : tmp);
            if (tmp)
                ldfree(tmp);
        }
        return UINT8_MAX;
    }
    else if (strcmp(method, "Post") == 0)
    {
        if ((jeventbase = cJSON_GetObjectItem(mParams, "eventBase")) &&
            (jeventbase = cJSON_GetObjectItem(jeventbase, "value")) &&
            (jeventid = cJSON_GetObjectItem(mParams, "eventId")) &&
            (jeventid = cJSON_GetObjectItem(jeventid, "value")))
        {
            EventDescriptor_t *edesc = EventHandlerDescriptor::GetEventDescriptor(jeventbase->valuestring, jeventid->valuestring);
            if (edesc)
            {
                // if (strcmp(jeventbase->valuestring, "MFile") == 0)
                // {
                //     BufferedFile::ProcessEvent(NULL, edesc->baseName, edesc->id, &mParams);
                // }
                // else if (strcmp(jeventbase->valuestring, "DigitalPin") == 0)
                // {
                //     Pin::ProcessEvent(NULL, edesc->baseName, edesc->id, &mParams);
                // }
                // else 
                // {
                    ESP_LOGV(__FUNCTION__, "Posting %s to %s(%d)..", edesc->eventName, edesc->baseName, edesc->id);
                    if ((ret = esp_event_post(edesc->baseName, edesc->id, &mParams, sizeof(void *), portMAX_DELAY)) != ESP_OK)
                    {
                        ESP_LOGW(__FUNCTION__, "Cannot post %s to %s:%s..", jeventid->valuestring, jeventbase->valuestring, esp_err_to_name(ret));
                    }
                // }
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "No descriptor for %s-%s", jeventbase->valuestring, jeventid->valuestring);
            }
        }
        else
        {
            char *tmp = cJSON_Print(mParams);
            ESP_LOGW(__FUNCTION__, "Missing event id or base:%s", tmp == NULL ? "null" : tmp);
            if (tmp)
                ldfree(tmp);
        }
        return UINT8_MAX;
    }
    if (theFunc)
    {
        if (inBackground)
            return CreateWokeBackgroundTask(theFunc, method, 4096, NULL, tskIDLE_PRIORITY, NULL);
        else
            CreateWokeInlineTask(theFunc, method, 4096, NULL);
    }
    return UINT8_MAX;
}

EventCondition::EventCondition(cJSON *json)
    : compareOperation(GetCompareOperator(json))
    , valType(GetEntityType(json, "src"))
    , valOrigin(GetOriginType(json, "src"))
    , compType(GetEntityType(json, "comp"))
    , compOrigin(GetOriginType(json, "comp"))
    , compStrVal(NULL)
    , valJsonPropName(NULL)
    , compJsonPropName(NULL)
    , compIntValue(0)
    , compDblValue(0.0)
    , srcJson(NULL)
    , compJson(NULL)
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
        if ((valOrigin == compare_origin_t::Config) || (valOrigin == compare_origin_t::State))
        {
            ESP_LOGV(__FUNCTION__, "val json config(%d)", (int)valOrigin);
            if (cJSON_HasObjectItem(cJSON_GetObjectItem(json, "src"), "path"))
            {
                const char *ctmp = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "src"), "path"), "value"));
                if (ctmp && strlen(ctmp))
                {
                    valJsonPropName = (char *)dmalloc(strlen(ctmp) + 1);
                    strcpy(valJsonPropName, ctmp);
                    ESP_LOGV(__FUNCTION__, "val json config(%s)", valJsonPropName);
                    if (indexOf(valJsonPropName, "/") > valJsonPropName)
                    {
                        ESP_LOGV(__FUNCTION__, "val source json config(%s) is in memory", valJsonPropName);
                        AppConfig *cfg = valOrigin == compare_origin_t::Config ? AppConfig::GetAppConfig() : AppConfig::GetAppStatus();
                        srcJson = cfg->GetPropertyHolder(valJsonPropName);
                    }
                }
                else
                {
                    isValid = false;
                    char *stmp = cJSON_PrintUnformatted(json);
                    ESP_LOGW(__FUNCTION__, "Empty source path property in %s", stmp);
                    ldfree(stmp);
                }
            }
            else
            {
                isValid = false;
                char *stmp = cJSON_PrintUnformatted(json);
                ESP_LOGW(__FUNCTION__, "Missing source path property in %s", stmp);
                ldfree(stmp);
            }
        }
        if ((compOrigin == compare_origin_t::Config) || (compOrigin == compare_origin_t::State))
        {
            if (cJSON_HasObjectItem(cJSON_GetObjectItem(json, "comp"), "path"))
            {
                const char *ctmp = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "comp"), "path"), "value"));
                if (ctmp && strlen(ctmp))
                {
                    compJsonPropName = (char *)dmalloc(strlen(ctmp) + 1);
                    strcpy(compJsonPropName, ctmp);
                    if (indexOf(compJsonPropName, "/") > compJsonPropName)
                    {
                        AppConfig *cfg = compOrigin == compare_origin_t::Config ? AppConfig::GetAppConfig() : AppConfig::GetAppStatus();
                        compJson = cfg->GetPropertyHolder(compJsonPropName);
                    }
                    ldfree(compJsonPropName);
                }
                else
                {
                    char *stmp = cJSON_PrintUnformatted(json);
                    ESP_LOGW(__FUNCTION__, "Empty comp path property in %s", stmp);
                    ldfree(stmp);
                    isValid = false;
                }
            }
            else
            {
                char *stmp = cJSON_PrintUnformatted(json);
                ESP_LOGW(__FUNCTION__, "Missing comp path property in %s", stmp);
                ldfree(stmp);
                isValid = false;
            }
        }

        if (compOrigin == compare_origin_t::Litteral)
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
    }
    else
    {
        char *stmp = cJSON_PrintUnformatted(json);
        ESP_LOGW(__FUNCTION__, "Invalid compare operation in %s", stmp);
        ESP_LOGW(__FUNCTION__, "valType:%d compType:%d compareOperation:%d valOrigin:%d compOrigin:%d", (int)valType, (int)compType, (int)compareOperation, (int)valOrigin, (int)compOrigin);
        ldfree(stmp);
    }
}

double EventCondition::GetDoubleValue(bool isSrc, compare_origin_t origin, void *event_data)
{
    cJSON *json = NULL;
    double retVal = 0.0;
    switch (origin)
    {
    case compare_origin_t::Config:
    case compare_origin_t::State:
        if ((origin == compare_origin_t::State) && (srcJson == NULL))
        {
            json = TheRest::status_json();
            retVal = cJSON_GetNumberValue(cJSON_GetObjectItem(json, isSrc ? valJsonPropName : compJsonPropName));
            cJSON_Delete(json);
        }
        else
        {
            retVal = cJSON_GetNumberValue(isSrc ? srcJson : compJson);
        }
        ESP_LOGV(__FUNCTION__, "Getting double %s value from json %s (%d):%f", isSrc ? "src" : "dest", isSrc ? valJsonPropName == NULL ? "null" : valJsonPropName : compJsonPropName == NULL ? "null"
                                                                                                                                                                                             : compJsonPropName,
                 json == NULL, retVal);
        return retVal;
        break;
    case compare_origin_t::Event:
        ESP_LOGV(__FUNCTION__, "Getting double %s value from event:%f", isSrc ? "src" : "dest", *((double *)event_data));
        return *((double *)event_data);
        break;
    case compare_origin_t::Litteral:
        ESP_LOGV(__FUNCTION__, "Getting litteral double %s value:%f", isSrc ? "src" : "dest", compDblValue);
        return compDblValue;
        break;
    default:
        ESP_LOGW(__FUNCTION__, "Invalid double");
        break;
    }
    return 0.0;
}

int EventCondition::GetIntValue(bool isSrc, compare_origin_t origin, void *event_data)
{
    cJSON *json = NULL;
    int retVal = 0;
    switch (origin)
    {
    case compare_origin_t::Config:
    case compare_origin_t::State:
        if ((origin == compare_origin_t::State) && (srcJson == NULL))
        {
            json = TheRest::status_json();
            retVal = cJSON_GetNumberValue(cJSON_GetObjectItem(json, isSrc ? valJsonPropName : compJsonPropName));
            cJSON_Delete(json);
        }
        else
        {
            retVal = cJSON_GetNumberValue(isSrc ? srcJson : compJson);
        }
        ESP_LOGV(__FUNCTION__, "Getting double %s value from json %s (%d):%d", isSrc ? "src" : "dest", isSrc ? valJsonPropName == NULL ? "null" : valJsonPropName : compJsonPropName == NULL ? "null"
                                                                                                                                                                                             : compJsonPropName,
                 json == NULL, retVal);
        return retVal;
        break;
    case compare_origin_t::Event:
        return *((int *)event_data);
        break;
    case compare_origin_t::Litteral:
        return compIntValue;
        break;
    default:
        ESP_LOGW(__FUNCTION__, "Invalid int");
        break;
    }
    return 0;
}

const char *EventCondition::GetStringValue(bool isSrc, compare_origin_t origin, void *event_data)
{
    switch (origin)
    {
    case compare_origin_t::Config:
    case compare_origin_t::State:
        return cJSON_GetStringValue(isSrc ? srcJson : compJson);
        break;
    case compare_origin_t::Event:
        return (const char *)event_data;
        break;
    case compare_origin_t::Litteral:
        return compStrVal;
        break;
    default:
        ESP_LOGW(__FUNCTION__, "Invalid Str");
        break;
    }
    return "";
}

void EventCondition::PrintState()
{
    if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    {
        ESP_LOGV(__FUNCTION__, "isValid:%d compareOperation:%d valType:%d valOrigin:%d,compType:%d compOrigin:%d compStrVal:%s compIntValue:%d compDblValue:%f valJsonPropName:%s compJsonPropName:%s",
                 isValid,
                 (int)compareOperation,
                 (int)valType,
                 (int)valOrigin,
                 (int)compType,
                 (int)compOrigin,
                 compStrVal == NULL ? "*null*" : compStrVal,
                 compIntValue,
                 compDblValue,
                 valJsonPropName == NULL ? "*null*" : valJsonPropName,
                 compJsonPropName == NULL ? "*null*" : compJsonPropName);
    }
}

bool EventCondition::IsEventCompliant(void *event_data)
{
    if (!IsValid())
    {
        ESP_LOGW(__FUNCTION__, "Condition is invalid");
        return false;
    }
    PrintState();
    switch (compType)
    {
    case compare_entity_type_t::Fractional:
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            return GetDoubleValue(true, valOrigin, event_data) == GetDoubleValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::Bigger:
            return GetDoubleValue(true, valOrigin, event_data) > GetDoubleValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::BiggerOrEqual:
            return GetDoubleValue(true, valOrigin, event_data) >= GetDoubleValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::Smaller:
            return GetDoubleValue(true, valOrigin, event_data) < GetDoubleValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::SmallerOrEqual:
            return GetDoubleValue(true, valOrigin, event_data) <= GetDoubleValue(false, compOrigin, event_data);
            break;
        default:
            return false;
            break;
        }
        break;
    case compare_entity_type_t::Integer:
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            return GetIntValue(true, valOrigin, event_data) == GetIntValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::Bigger:
            return GetIntValue(true, valOrigin, event_data) > GetIntValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::BiggerOrEqual:
            return GetIntValue(true, valOrigin, event_data) >= GetIntValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::Smaller:
            return GetIntValue(true, valOrigin, event_data) < GetIntValue(false, compOrigin, event_data);
            break;
        case compare_operation_t::SmallerOrEqual:
            return GetIntValue(true, valOrigin, event_data) <= GetIntValue(false, compOrigin, event_data);
            break;
        default:
            return false;
            break;
        }
        break;
    case compare_entity_type_t::String:
        switch (compareOperation)
        {
        case compare_operation_t::Equal:
            return strcmp(GetStringValue(true, valOrigin, event_data), GetStringValue(false, compOrigin, event_data)) == 0;
            break;
        case compare_operation_t::RexEx:
            return std::regex_search(GetStringValue(true, valOrigin, event_data), regexp, std::regex_constants::match_default);
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
        if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(val, "name")), "event") == 0)
            return compare_origin_t::Event;
        if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(val, "name")), "state") == 0)
            return compare_origin_t::State;
        if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(val, "name")), "config") == 0)
            return compare_origin_t::Config;
    }
    return compare_origin_t::Litteral;
}
