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
#include "esp_adc_cal.h"

class Pin:ManagedDevice {
public:
    Pin(AppConfig* config);
    ~Pin();
    static void PollPins();
    void ProcessTheEvent(postedEvent_t* postedEvent);
    static void ProcessEvent(ManagedDevice* device, postedEvent_t* postedEvent);
    bool isRtcGpio;
    enum eventIds {
        OFF,ON,TRIGGER,STATUS
    };
    static cJSON* BuildConfigTemplate();
    static bool ProcessCommand(ManagedDevice* pin, cJSON * parms);
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
};

class AnalogPin:ManagedDevice {
public:
    AnalogPin(AppConfig* config);
    ~AnalogPin();
    static void PollPin(void* instance);
    static cJSON* BuildConfigTemplate();

protected:
    static const char* PIN_BASE;
    AppConfig* config;
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
    cJSON* voltage;
    bool isRunning;
    esp_adc_cal_characteristics_t chars;

    void InitDevice();
    static adc1_channel_t PinNoToChannel(gpio_num_t pinNo);
    static adc_bits_width_t GetChannelWidth(uint8_t value);
    static adc_atten_t GetChannelAtten(double value);
};

#endif