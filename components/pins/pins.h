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
#include "eventmgr.h"
class Pin:ManagedDevice {
public:
    Pin(AppConfig* config);
    ~Pin();
    static void PollPins();
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    bool state;
    bool isRtcGpio;
    enum eventIds {
        ON,OFF,TRIGGER
    };
protected:
    static uint8_t numPins;
    static void queuePoller(void *arg);
    static void pinHandler(void *arg);
    static QueueHandle_t eventQueue;
    gpio_num_t pinNo;
    uint32_t flags;
    void InitDevice();
    void RefrestState();
    EventHandlerDescriptor* BuildHandlerDescriptors();
    static cJSON* BuildStatus(void* instance);
    cJSON* status;
private:
    AppConfig* config;
    cJSON* pinStatus;
};

#endif