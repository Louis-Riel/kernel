#ifndef __eventmgr_h
#define __eventmgr_h

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include <limits.h>
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "cJSON.h"
#include <regex>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define MAX_NUM_HANDLERS_PER_BASE 30
#define MAX_NUM_HANDLERS 100
#define MAX_NUM_EVENTS 200
#define MAX_NUM_DEVICES 50
#define MAX_THREADS 20

struct EventDescriptor_t
{
    int32_t id;
    char *eventName;
    char *baseName;
};

class EventHandlerDescriptor
{
public:
    EventHandlerDescriptor(esp_event_base_t base, char *name);
    bool AddEventDescriptor(int32_t id, char *name);
    static EventDescriptor_t *GetEventDescriptor(char *base, char *eventName);
    static EventDescriptor_t *GetEventDescriptor(esp_event_base_t base, uint32_t id);
    static cJSON *GetEventBaseEvents(char *base, char *filter);
    static char *GetParsedValue(const char *value);
    char *GetName();
    char *GetEventName(int32_t id);
    int32_t GetEventId(char *name);
    void SetEventName(int32_t id, char *name);
    esp_event_base_t GetEventBase();
    enum templateType_t
    {
        Invalid = 0,
        Status,
        Config,
        CurrentDate,
        CurrentDateNoSpace,
        RAM,
        Battery
    };

private:
    EventDescriptor_t *eventDescriptors;
    static EventDescriptor_t *eventDescriptorCache;
    static uint32_t numCacheEntries;
    esp_event_base_t eventBase;
    char *name;

    static EventHandlerDescriptor::templateType_t GetTemplateType(char *term);
};

enum class compare_operation_t
{
    Invalid,
    Equal,
    Bigger,
    BiggerOrEqual,
    Smaller,
    SmallerOrEqual,
    RexEx
};

enum class compare_entity_type_t
{
    Invalid,
    Integer,
    Fractional,
    String
};

enum class compare_origin_t
{
    Invalid,
    Event,
    Litteral
};

class EventCondition
{
public:
    EventCondition(cJSON *json);
    bool IsEventCompliant(void *event_data);
    bool IsValid();
    void PrintState();

private:
    static compare_operation_t GetCompareOperator(cJSON *json);
    static compare_entity_type_t GetEntityType(cJSON *json, const char *fldName);
    static compare_origin_t GetOriginType(cJSON *json, const char *fldName);

    compare_operation_t compareOperation;
    compare_entity_type_t valType;
    compare_origin_t valOrigin;
    compare_entity_type_t compType;
    compare_origin_t compOrigin;
    std::regex regexp;
    bool isValid;
    char *compStrVal;
    char *eventJsonPropName;
    int32_t compIntValue;
    double compDblValue;
};

class EventInterpretor
{
public:
    EventInterpretor(cJSON *json, cJSON *programs);
    bool IsValid(EventHandlerDescriptor *handler, int32_t id, void *event_data);
    uint8_t RunMethod(EventHandlerDescriptor *handler, int32_t id, void *event_data, const char *method, bool inBackground);
    uint8_t RunMethod(EventHandlerDescriptor *handler, int32_t id, void *event_data);
    void RunProgram(EventHandlerDescriptor *handler, int32_t id, void *event_data, const char *progName);
    void RunProgram(const char *progName);
    bool IsProgram();
    char *GetProgramName();
    char* ToString();

private:
    typedef struct
    {
        EventInterpretor* interpretor;
        cJSON* program;
    } LoopyArgs;
    
    static void RunLooper(void* param);

    cJSON *programs;
    cJSON *config;
    char *programName;
    char *eventBase;
    char *eventId;
    char *method;
    EventCondition *conditions[5];
    bool isAnd[5];
    cJSON *params;
    EventHandlerDescriptor *handler;
    int32_t id;
    EventGroupHandle_t app_eg;
};

class EventManager
{
public:
    EventManager(cJSON *, cJSON *);
    static void RegisterEventHandler(EventHandlerDescriptor *eventHandlerDescriptor);
    static void UnRegisterEventHandler(EventHandlerDescriptor *eventHandlerDescriptor);
    EventInterpretor *eventInterpretors[MAX_NUM_EVENTS];
    cJSON *GetConfig();
    bool ValidateConfig();
    static EventManager *GetInstance();

private:
    cJSON *config;
    cJSON *programs;
    esp_event_loop_handle_t evtMgrLoopHandle;
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
};

class ManagedDevice
{
public:
    ManagedDevice(char *type);
    ManagedDevice(char *type, char *name, cJSON *(*statusFnc)(void *));
    ManagedDevice(char *type, char *name, cJSON *(*statusFnc)(void *),bool (hcFnc)(void *));
    ~ManagedDevice();
    static void UpdateStatuses();
    const char *GetName();
    esp_event_base_t eventBase;
    EventHandlerDescriptor *handlerDescriptors;
    static void RunHealthCheck(void* param);

protected:
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    esp_err_t PostEvent(void *content, size_t len, int32_t event_id);
    EventHandlerDescriptor *BuildHandlerDescriptors();
    static cJSON *BuildStatus(void *instance);
    cJSON *(*statusFnc)(void *);
    bool (*hcFnc)(void *);
    cJSON *status;
    char *name;
    static bool HealthCheck(void *instance);

private:
    static bool ValidateDevices();
    static ManagedDevice *runningInstances[MAX_NUM_DEVICES];
    static uint8_t numDevices;
};

class ManagedThreads
{
public:
    ManagedThreads()
        : managedThreadBits(xEventGroupCreate()) 
        ,threads((ManagedThreads::mThread_t **)dmalloc(32 * sizeof(void *))), numThreadSlot(0)
    {
        memset(&threads[0], 0, 32 * sizeof(void *));
        xEventGroupSetBits(managedThreadBits,0xffff);
    }

    uint8_t NumAllocatedThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 0 : 1;
        }
        return ret;
    }

    uint8_t NumUnallocatedThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 1 : 0;
        }
        return ret;
    }

    uint8_t NumRunningThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 0 : thread->isRunning ? 1
                                                          : 0;
        }
        return ret;
    }

    uint8_t NumDoneThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 0 : thread->started && !thread->isRunning ? 1
                                                                              : 0;
        }
        return ret;
    }

    cJSON *GetStatus()
    {
        cJSON *stat = cJSON_CreateObject();
        cJSON *jthreads = cJSON_AddArrayToObject(stat, "threads");
        cJSON_AddNumberToObject(stat, "allocated", NumAllocatedThreads());
        cJSON_AddNumberToObject(stat, "availableslots", NumUnallocatedThreads());
        cJSON_AddNumberToObject(stat, "running", NumRunningThreads());
        cJSON_AddNumberToObject(stat, "done", NumDoneThreads());
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            if (thread)
            {
                cJSON *jthread = cJSON_CreateObject();
                cJSON_AddItemToArray(jthreads, jthread);
                if (thread->pcName)
                    cJSON_AddStringToObject(jthread, "name", thread->pcName);
                cJSON_AddBoolToObject(jthread, "started", thread->started);
                cJSON_AddBoolToObject(jthread, "running", thread->isRunning);
            }
        }
        return stat;
    }

    void PrintState() {
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            for (uint8_t idx = 0; idx < 32; idx++)
            {
                mThread_t *thread = threads[idx];
                if (thread)
                {
                    ESP_LOGV(__FUNCTION__,"%d %s:isRunning:%d started:%d waitToSleep:%d",idx,thread->pcName,thread->isRunning, thread->started, thread->waitToSleep);
                }
            }
        }
    }

    void WaitToSleep()
    {
        uint32_t bitsToWaitFor = 0;
        PrintState();
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            if (thread && thread->waitToSleep && thread->isRunning)
            {
                ESP_LOGD(__FUNCTION__,"%s is sleep blocked running", thread->pcName);
                bitsToWaitFor += (1 << idx);
            }
        }
        WaitForThreads(bitsToWaitFor);
    }

    void WaitToSleepExceptFor(char* name)
    {
        uint32_t bitsToWaitFor = 0;
        PrintState();
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            if (thread && thread->waitToSleep && thread->isRunning && !startsWith(thread->pcName,name))
            {
                ESP_LOGD(__FUNCTION__,"%s is sleep blocked running", thread->pcName);
                bitsToWaitFor += (1 << idx);
            }
        }
        if (bitsToWaitFor)
            WaitForThreads(bitsToWaitFor);
    }

    void WaitForThreads(uint32_t bitsToWaitFor)
    {
        if (bitsToWaitFor)
        {
            ESP_LOGD(__FUNCTION__, "Waiting for threads %d", bitsToWaitFor);
            xEventGroupWaitBits(managedThreadBits, bitsToWaitFor, false, true, portMAX_DELAY);
        }
        else
        {
            ESP_LOGV(__FUNCTION__, "No threads to wait for");
        }
    }

    uint8_t CreateBackgroundManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t *const pvCreatedTask,
        const bool allowRelaunch,
        const bool waitToSleep)
    {

        if (!allowRelaunch && IsThreadRunning(pcName))
        {
            ESP_LOGW(__FUNCTION__, "Cannot run %s as it is already running", pcName);
            return UINT8_MAX;
        }
        uint8_t bitNo = GetFreeBit();
        if (bitNo != UINT8_MAX)
        {
            mThread_t *thread = threads[bitNo];
            thread->pcName = (char *)dmalloc(strlen(pcName) + 1);
            strcpy(thread->pcName, pcName);
            thread->pvTaskCode = pvTaskCode;
            thread->usStackDepth = usStackDepth;
            thread->pvParameters = pvParameters;
            thread->uxPriority = uxPriority;
            thread->bitNo = bitNo;
            thread->waitToSleep = waitToSleep;
            xEventGroupClearBits(managedThreadBits, 1 << bitNo);
            ESP_LOGV(__FUNCTION__, "Running %s", pcName);

            BaseType_t ret = pdPASS;
            uint8_t retryCtn = 10;
            uint32_t runningBits = 0;
            while (retryCtn-- && (ret = xTaskCreate(ManagedThreads::runThread,
                                                    pcName,
                                                    usStackDepth,
                                                    (void *)thread,
                                                    uxPriority,
                                                    &thread->pvCreatedTask)) != pdPASS)
            {
                if ((runningBits = GetRunningBits()))
                {
                    ESP_LOGW(__FUNCTION__, "Error in creating thread for %s, retry %d, waiting on %d. %s", pcName, retryCtn, runningBits, esp_err_to_name(ret));
                    xEventGroupWaitBits(managedThreadBits, runningBits, pdFALSE, pdFALSE, portMAX_DELAY);
                }
                else
                {
                    ESP_LOGW(__FUNCTION__, "Error in creating thread for %s, retry %d. %s", pcName, retryCtn, esp_err_to_name(ret));
                }
            }
            if (ret != pdPASS)
            {
                ESP_LOGE(__FUNCTION__, "Failed in creating thread for %s. %s", pcName, esp_err_to_name(ret));
                dumpTheLogs(NULL);
                esp_restart();
            }
            if (pvCreatedTask != NULL)
            {
                *pvCreatedTask = thread->pvCreatedTask;
            }
            return bitNo;
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "No more bits for %s", pcName);
        }

        return UINT8_MAX;
    };

    void printMemStat()
    {
        return;
        heap_caps_print_heap_info(MALLOC_CAP_EXEC);
        heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
        heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    }

    BaseType_t CreateInlineManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        const bool canRelanch,
        const bool waitToSleep)
    {
        return CreateInlineManagedTask(
            pvTaskCode, pcName, usStackDepth, pvParameters, canRelanch, waitToSleep, false);
    }

    BaseType_t CreateInlineManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        const bool canRelanch,
        const bool waitToSleep,
        const bool onMainThread)
    {
        if (!canRelanch && IsThreadRunning(pcName))
        {
            return ESP_ERR_INVALID_STATE;
        }

        uint8_t bitNo = GetFreeBit();
        if (bitNo != UINT8_MAX)
        {
            mThread_t *thread = threads[bitNo];
            thread->pcName = (char *)dmalloc(strlen(pcName) + 1);
            strcpy(thread->pcName, pcName);
            thread->pvTaskCode = pvTaskCode;
            thread->pvParameters = pvParameters;
            thread->usStackDepth = usStackDepth;
            thread->bitNo = bitNo;
            thread->isRunning = true;
            thread->started = true;
            thread->waitToSleep = waitToSleep;
            xEventGroupClearBits(managedThreadBits, 1 << bitNo);
            ESP_LOGV(__FUNCTION__, "Running %s", thread->pcName);
            printMemStat();
            BaseType_t ret = ESP_OK;
            if (onMainThread)
            {
                ESP_LOGD(__FUNCTION__, "Starting the %s service", thread->pcName);
                thread->pvTaskCode(thread->pvParameters);
                ESP_LOGV(__FUNCTION__, "Done initializing the %s service", thread->pcName);
                if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
                {
                    char *tmp = cJSON_Print(thread->parent->GetStatus());
                    ESP_LOGV(__FUNCTION__, "%s", tmp);
                    ldfree(tmp);
                }
            }
            else
            {
                uint8_t retryCtn = 10;
                if (!heap_caps_check_integrity_all(true)) {
                    ESP_LOGE(__FUNCTION__,"bcaps integrity error");
                }
                if ((ret = xTaskCreate(ManagedThreads::runThread,
                                       pcName,
                                       usStackDepth,
                                       (void *)thread,
                                       tskIDLE_PRIORITY,
                                       &thread->pvCreatedTask)) == pdPASS)
                {
                    ESP_LOGV(__FUNCTION__, "Waiting for %s to finish", thread->pcName);
                    xEventGroupWaitBits(managedThreadBits, 1 << bitNo, pdFALSE, pdTRUE, portMAX_DELAY);
                    printMemStat();
                    if (!heap_caps_check_integrity_all(true)) {
                        ESP_LOGE(__FUNCTION__,"caps integrity error");
                    }
                    ESP_LOGV(__FUNCTION__, "Done running %s", thread->pcName);
                    thread->isRunning = false;
                    return ESP_OK;
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Error running %s: %s stack depth:%d", thread->pcName, esp_err_to_name(ret), usStackDepth);
                }
            }
        }
        return ESP_ERR_NO_MEM;
    };

protected:
    EventGroupHandle_t managedThreadBits;
    struct mThread_t
    {
        TaskFunction_t pvTaskCode;
        char *pcName;
        uint32_t usStackDepth;
        void *pvParameters;
        UBaseType_t uxPriority;
        TaskHandle_t pvCreatedTask;
        uint8_t bitNo;
        bool isRunning;
        bool started;
        bool waitToSleep;
        ManagedThreads *parent;
    } * *threads;
    uint8_t numThreadSlot;

    static void runThread(void *param)
    {
        mThread_t *thread = (mThread_t *)param;
        ESP_LOGV(__FUNCTION__, "Running the %s thread", thread->pcName);
        xEventGroupClearBits(thread->parent->managedThreadBits, 1 << thread->bitNo);
        thread->started = true;
        thread->isRunning = true;
        thread->pvTaskCode(thread->pvParameters);
        thread->isRunning = false;
        ESP_LOGV(__FUNCTION__, "Done running the %s thread", thread->pcName);
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            char *tmp = cJSON_Print(thread->parent->GetStatus());
            ESP_LOGV(__FUNCTION__, "%s", tmp);
        }

        xEventGroupSetBits(thread->parent->managedThreadBits, 1 << thread->bitNo);
        vTaskDelete(NULL);
    }

    mThread_t *GetThread(const char *const pcName)
    {
        for (uint8_t idx = 0; idx < numThreadSlot; idx++)
        {
            if (strcmp(threads[idx]->pcName, pcName) == 0)
            {
                return threads[idx];
            }
        }
        return NULL;
    }

    bool IsThreadRunning(const char *const pcName)
    {
        mThread_t *t = GetThread(pcName);
        return t ? t->isRunning : false;
    }

    uint32_t GetRunningBits()
    {
        uint32_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            if ((threads[idx] == NULL) || (threads[idx]->started && threads[idx]->isRunning))
            {
                ret |= (1 >> idx);
            }
        }
        return ret;
    }

    uint8_t GetFreeBit()
    {
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            if ((threads[idx] == NULL) || (threads[idx]->started && !threads[idx]->isRunning))
            {
                if (threads[idx])
                {
                    if (threads[idx]->pcName)
                        ldfree((void *)threads[idx]->pcName);
                    memset(threads[idx], 0, sizeof(mThread_t));
                    threads[idx]->parent = this;
                }
                else
                {
                    threads[idx] = (mThread_t *)dmalloc(sizeof(mThread_t));
                    memset(threads[idx], 0, sizeof(mThread_t));
                    threads[idx]->parent = this;
                }
                return idx;
            }
        }
        return UINT8_MAX;
    }
};

static ManagedThreads managedThreads;

static BaseType_t CreateMainlineTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    void *const pvParameters)
{
    return managedThreads.CreateInlineManagedTask(pvTaskCode, pcName, 8192, pvParameters, false, false, false);
};

static void WaitToSleepExceptFor(char* name)
{
    managedThreads.WaitToSleepExceptFor(name);
}

static void WaitToSleep()
{
    managedThreads.WaitToSleep();
}

static uint8_t CreateBackgroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *const pvCreatedTask)
{
    return managedThreads.CreateBackgroundManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask, false, false);
};

static uint8_t CreateWokeBackgroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *const pvCreatedTask)
{
    return managedThreads.CreateBackgroundManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask, false, true);
};

static BaseType_t CreateInlineTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters)
{
    return managedThreads.CreateInlineManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, false, false);
};

static BaseType_t CreateWokeInlineTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters)
{
    return managedThreads.CreateInlineManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, false, true);
};

#endif