#ifndef __eventmgr_h
#define __eventmgr_h

// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include <limits.h>
// #include "driver/gpio.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
#include "esp_event.h"
// #include "freertos/event_groups.h"
#include "../../main/logs.h"
// #include "../../main/utils.h"
// #include "cJSON.h"
#include <regex>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define MAX_NUM_HANDLERS_PER_BASE 30
#define MAX_NUM_HANDLERS 100
#define MAX_NUM_EVENTS 200
#define MAX_NUM_DEVICES 50
#define MAX_THREADS 20

enum class event_data_type_tp 
{
    Unknown,
    JSON,
    String,
    Number,
    NumEntries
};

struct EventDescriptor_t
{
    int32_t id;
    const char *eventName;
    char *baseName;
    event_data_type_tp dataType;
};

class EventHandlerDescriptor
{
public:
    EventHandlerDescriptor(esp_event_base_t base, const char *name);
    ~EventHandlerDescriptor();
    bool AddEventDescriptor(int32_t id, const char *name);
    bool AddEventDescriptor(int32_t id, const char *name, event_data_type_tp dtp);
    static EventDescriptor_t *GetEventDescriptor(esp_event_base_t base,const char *eventName);
    static EventDescriptor_t *GetEventDescriptor(esp_event_base_t base, uint32_t id);
    static char *GetParsedValue(const char *value);
    char *GetName();
    const char *GetEventName(int32_t id);
    int32_t GetEventId(const char *name);
    void SetEventName(int32_t id, const char *name);
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

    static EventHandlerDescriptor::templateType_t GetTemplateType(const char *term);
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
    State,
    Config,
    Litteral
};

struct postedEvent_t
{
    esp_event_base_t base;
    int32_t id;
    void *event_data;
    event_data_type_tp eventDataType;
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

    double GetDoubleValue(bool isSrc,compare_origin_t origin,void *event_data);
    int GetIntValue(bool isSrc,compare_origin_t origin,void *event_data);
    const char* GetStringValue(bool isSrc,compare_origin_t origin, void *event_data);

    compare_operation_t compareOperation;
    compare_entity_type_t valType;
    compare_origin_t valOrigin;
    compare_entity_type_t compType;
    compare_origin_t compOrigin;
    std::regex regexp;
    bool isValid;
    char *compStrVal;
    char *valJsonPropName;
    char *compJsonPropName;
    int32_t compIntValue;
    double compDblValue;
    cJSON* srcJson;
    cJSON* compJson;
};

class EventInterpretor
{
public:
    EventInterpretor(cJSON *json, cJSON *programs);
    bool IsValid(esp_event_base_t eventBase, int32_t id, void *event_data);
    static uint8_t RunMethod(EventInterpretor* instance, const char* method, void *event_data, bool inBackground);
    uint8_t RunMethod(const char* method, void *event_data, bool inBackground);
    uint8_t RunMethod(void *event_data);
    void RunProgram(void *event_data, const char *progName);
    void RunProgram(const char *progName);
    bool IsProgram();
    const char *GetProgramName();
    cJSON* GetParams();
    char* ToString();

private:
    typedef struct
    {
        EventInterpretor* interpretor;
        const char* program;
    } LoopyArgs;
    
    cJSON *programs;
    cJSON *config;
    const char *programName;
    const char *eventBase;
    const char *eventId;
    const char *method;
    EventCondition *conditions[5];
    bool isAnd[5];
    cJSON *params;
    int32_t id;
    EventGroupHandle_t app_eg;
    static void _RunProgram(void* arg);
};

class ManagedDevice
{
public:
    ManagedDevice(const char *type);
    ManagedDevice(const char *type, const char *name);
    ManagedDevice(const char *type, const char *name,bool (*hcFnc)(void *),bool (*commandFnc)(ManagedDevice* instance, cJSON *));
    ManagedDevice(const char *type, const char *name,bool (*hcFnc)(void *),bool (*commandFnc)(ManagedDevice* instance, cJSON *),void (*processEventFnc)(ManagedDevice* instance, postedEvent_t*));
    ~ManagedDevice();
    const char *GetName();
    esp_err_t PostEvent(void *content, size_t len, int32_t event_id);
    esp_event_base_t eventBase;
    EventHandlerDescriptor *handlerDescriptors;

    bool ProcessCommand(cJSON *command);
    void ProcessEvent(postedEvent_t* postedEvent);

    static void RunHealthCheck(void* param);
    static uint32_t GetNumRunningInstances();
    static ManagedDevice** GetRunningInstances();
    static ManagedDevice* GetByName(const char* name);
    static cJSON* BuildConfigTemplate();
    static cJSON *BuildStatus(void *instance);
    static cJSON* GetConfigTemplates();
    void (*processEventFnc)(ManagedDevice*, postedEvent_t*);

    cJSON *status;

protected:
    EventHandlerDescriptor *BuildHandlerDescriptors();
    bool (*hcFnc)(void *);
    bool (*commandFnc)(ManagedDevice*, cJSON *);
    char *name;
    static bool HealthCheck(void *instance);

private:
    static bool ValidateDevices();
    static cJSON* configTemplates;
    static ManagedDevice *runningInstances[MAX_NUM_DEVICES];
    static uint8_t numDevices;
    static uint32_t numErrors;
    static uint64_t lastErrorTs;
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
    static void ProcessEvent(ManagedDevice* device, postedEvent_t* postedEvent);

private:
    cJSON *config;
    cJSON *programs;
    char* eventBuffer;
    static void EventPoller(void* param);
    static void EventProcessor(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    QueueHandle_t eventQueue;
};

class ManagedThreads
{
public:
    ManagedThreads();
    static ManagedThreads* GetInstance();

    uint8_t NumAllocatedThreads();

    uint8_t NumUnallocatedThreads();

    uint8_t NumRunningThreads();

    uint8_t NumDoneThreads();

    cJSON *GetStatus();

    void PrintState();

    void WaitToSleep();

    void WaitToSleepExceptFor(const char* name);

    void WaitForThreads(uint32_t bitsToWaitFor);

    static cJSON* GetRepeatingTaskStatus();

    uint8_t CreateBackgroundManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t *const pvCreatedTask,
        const bool allowRelaunch,
        const bool waitToSleep);

    BaseType_t CreateInlineManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        const bool canRelanch,
        const bool waitToSleep);

    BaseType_t CreateInlineManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        const bool canRelanch,
        const bool waitToSleep,
        const bool onMainThread);

protected:
    EventGroupHandle_t managedThreadBits;
    SemaphoreHandle_t threadSema;
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
        bool allocated;
        bool started;
        bool waitToSleep;
        ManagedThreads *parent;
    } **threads;
    uint8_t numThreadSlot;

    static void runThread(void *param);
    mThread_t* GetThreadByName(const char *const pcName);

    bool IsThreadRunning(const char *const pcName);

    uint32_t GetRunningBits();

    uint8_t GetFreeBit(const char* name);
};

BaseType_t CreateForegroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    void *const pvParameters);

void WaitToSleepExceptFor(const char* name);

void WaitToSleep();

uint8_t CreateBackgroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *const pvCreatedTask);

uint8_t CreateWokeBackgroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *const pvCreatedTask);

uint8_t CreateRepeatingTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    void *const pvParameters,
    const uint32_t repeatPeriod);

void UpdateRepeatingTaskPeriod(
    const uint32_t idx,
    const uint32_t repeatPeriod);

#endif