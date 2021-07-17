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

#define MAX_NUM_HANDLERS_PER_BASE 30
#define MAX_NUM_HANDLERS 100
#define MAX_NUM_EVENTS 200

struct EventDescriptor_t {
        int32_t id;
        char* eventName;
        char* baseName;
};

class EventHandlerDescriptor {
public:
    EventHandlerDescriptor(esp_event_base_t base,char* name);
    bool AddEventDescriptor(int32_t id,char* name);
    static EventDescriptor_t* GetEventDescriptor(char* base,char* eventName);
    static EventDescriptor_t* GetEventDescriptor(esp_event_base_t base,uint32_t id);     
    static cJSON* GetEventBaseEvents(char* base, char* filter);
    static char* GetParsedValue(const char* value);
    char* GetName();
    char* GetEventName(int32_t id);
    int32_t GetEventId(char* name);
    void SetEventName(int32_t id, char* name);
    esp_event_base_t GetEventBase();
    enum templateType_t {
        Invalid=0,
        Status,
        Config,
        CurrentDate,
        CurrentDateNoSpace,
        RAM,
        Battery
    };
private:
    EventDescriptor_t* eventDescriptors;
    static EventDescriptor_t* eventDescriptorCache;
    static uint32_t numCacheEntries;
    esp_event_base_t eventBase;
    char* name;

    static EventHandlerDescriptor::templateType_t GetTemplateType(char* term);
};

enum class compare_operation_t {
    Invalid,
    Equal,
    Bigger,
    BiggerOrEqual,
    Smaller,
    SmallerOrEqual,
    RexEx
};

enum class compare_entity_type_t {
    Invalid,
    Integer,
    Fractional,
    String
};

enum class compare_origin_t {
    Invalid,
    Event,
    Litteral
};

class EventCondition {
public:
    EventCondition(cJSON* json);
    bool IsEventCompliant(void *event_data);
    bool IsValid();
    void PrintState();
private:
    static compare_operation_t GetCompareOperator(cJSON* json);
    static compare_entity_type_t GetEntityType(cJSON* json,const char* fldName);
    static compare_origin_t GetOriginType(cJSON* json,const char* fldName);

    compare_operation_t compareOperation;
    compare_entity_type_t valType;
    compare_origin_t valOrigin;
    compare_entity_type_t compType;
    compare_origin_t compOrigin;
    std::regex regexp;
    bool isValid;
    char* compStrVal;
    char* eventJsonPropName;
    int32_t compIntValue;
    double compDblValue;
};

class EventInterpretor {
public:
    EventInterpretor(cJSON* json);
    bool IsValid(EventHandlerDescriptor *handler, int32_t id, void *event_data);
    void RunIt(EventHandlerDescriptor *handler, int32_t id,void *event_data);
private:
    char* eventBase;
    char* eventId;
    char* method;
    EventCondition* conditions[5];
    bool isAnd[5];
    cJSON* params;
    EventHandlerDescriptor* handler;
    int32_t id;
};

class EventManager {
public:
    EventManager(cJSON*);
    static void RegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor);
    static void UnRegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor);
    EventInterpretor* eventInterpretors[MAX_NUM_EVENTS];
    cJSON* GetConfig();
    cJSON* SetConfig(cJSON*);
    static EventManager* GetInstance();
private:
    cJSON* config;
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
};

class ManagedDevice {
public:
    ManagedDevice(char* type);
    ~ManagedDevice();
    cJSON* GetStatus();
    esp_event_base_t eventBase;
    EventHandlerDescriptor* handlerDescriptors;
protected:
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    void PostEvent(void* content, size_t len,int32_t event_id);
    EventHandlerDescriptor* BuildHandlerDescriptors();
    cJSON* BuildStatus();
    cJSON* status;
};

#endif