#ifndef __pins_h
#define __pins_h

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include <limits.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "cJSON.h"
#include "../eventmgr/eventmgr.h"
class Pin:ManagedDevice {
public:
    Pin(AppConfig* config);
    ~Pin();
    static void PollPins();
    bool state;
    bool valid;
    enum eventIds {
        ON,OFF,TRIGGER
    };
protected:
    static Pin* pins[MAX_NUM_PINS];
    static uint8_t numPins;
    static void queuePoller(void *arg);
    static void pinHandler(void *arg);
    void HandleEvent(cJSON* params);
    static QueueHandle_t eventQueue;
    gpio_num_t pinNo;
    uint32_t flags;
    char* name;
    void InitDevice();
    void RefrestState();
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    EventHandlerDescriptor* BuildHandlerDescriptors();
    cJSON* BuildStatus();
    cJSON* status;
};

#endif