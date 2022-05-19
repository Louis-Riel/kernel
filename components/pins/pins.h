#ifndef __pins_h
#define __pins_h

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include <limits.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
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
    bool isRtcGpio;
    enum eventIds {
        OFF,ON,TRIGGER,STATUS
    };
    bool ProcessEvent(Pin::eventIds event,uint8_t param);
protected:
    static uint8_t numPins;
    static void queuePoller(void *arg);
    static void pinHandler(void *arg);
    static QueueHandle_t eventQueue;
    static bool HealthCheck(void* instance);
    static const char* PIN_BASE;
    static esp_event_handler_instance_t *handlerInstance;
    gpio_num_t pinNo;
    uint32_t flags;
    void InitDevice();
    void RefrestState();
    EventHandlerDescriptor* BuildHandlerDescriptors();
private:
    AppConfig* config;
    cJSON* pinStatus;
    char* buf;
};

class AnalogPin:ManagedDevice {
public:
    AnalogPin(AppConfig* config);
    ~AnalogPin();
    static void PollPins(void* instance);

protected:
    static const char* PIN_BASE;
    AppConfig* config;
    char* name;
    gpio_num_t pinNo;
    adc1_channel_t channel;
    adc_bits_width_t  channel_width;
    adc_atten_t  channel_atten;
    uint32_t waitTime;
    cJSON* currentMinValue;
    cJSON* currentMaxValue;
    cJSON* currentPercentage;
    cJSON* configuredMinValue;
    cJSON* configuredMaxValue;
    cJSON* value;
    bool isRunning;

    void InitDevice();
    static adc1_channel_t PinNoToChannel(gpio_num_t pinNo);
    static adc_bits_width_t GetChannelWidth(uint8_t value);
    static adc_atten_t GetChannelAtten(double value);
    static char* getName(AppConfig* config);
};

#endif