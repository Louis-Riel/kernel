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

#define MAX_NUM_HANDLERS_PER_BASE 20
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
    static cJSON* GetEventBases(char* filter);
    static cJSON* GetEventBaseEvents(char* base, char* filter);
    char* GetName();
    char* GetEventName(int32_t id);
    int32_t GetEventId(char* name);
    void SetEventName(int32_t id, char* name);
    esp_event_base_t GetEventBase();
private:
    EventDescriptor_t* eventDescriptors;
    static EventDescriptor_t* eventDescriptorCache;
    static uint32_t numCacheEntries;
    esp_event_base_t eventBase;
    char* name;
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
    cJSON* params;
    EventHandlerDescriptor* handler;
    int32_t id;
};

class EventManager {
public:
    EventManager(cJSON*);
    static void RegisterEventHandler(EventHandlerDescriptor* eventHandlerDescriptor);
    EventInterpretor* eventInterpretors[MAX_NUM_EVENTS];
    static cJSON* GetConfig();
    static cJSON* SetConfig(cJSON*);
private:
    cJSON* config;
    static EventManager* instance;
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
};

class ManagedDevice {
public:
    ManagedDevice(AppConfig* config,char* type);
    void HandleEvent(cJSON* params);
    cJSON* GetStatus();
    static esp_event_base_t eventBase;
    static EventHandlerDescriptor* handlerDescriptors;
protected:
    AppConfig* config;
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    void PostEvent(void* content, size_t len,int32_t event_id);
    void InitDevice();
    static EventHandlerDescriptor* BuildHandlerDescriptors();
};

#endif