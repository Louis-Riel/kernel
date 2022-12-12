#ifndef __apa102_h
#define __apa102_h

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/mcpwm.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "cJSON.h"
#include "eventmgr.h"
#include "driver/spi_master.h"

class Apa102:ManagedDevice {
public:
    Apa102(AppConfig* config);
    ~Apa102();
    static cJSON* BuildConfigTemplate();

protected:
    static const char* APA102_BASE;
    AppConfig* config;
    char* name;
    gpio_num_t pwrPin;
    gpio_num_t dataPin;
    gpio_num_t clkPin;
    uint32_t numLeds;
    uint32_t refreshFreq;
    bool cpol;
    bool cpha;

    cJSON* jPower;
    cJSON* jSpiReady;
    cJSON* jDevReady;
    cJSON* jPainting;
    cJSON* jIdle;

    void InitDevice();
    EventHandlerDescriptor* BuildHandlerDescriptors();
    static bool ProcessCommand(ManagedDevice*, cJSON *);

private:
    spi_bus_config_t buscfg;
    spi_device_interface_config_t devcfg;
    spi_transaction_t spiTransObject;
    static void PaintIt(void* instance);
    spi_device_handle_t spi;
    uint32_t spiBufferLen;
    EventGroupHandle_t eg;

    typedef enum {
        spi_ready = BIT0,
        device_ready = BIT1,
        powered_on = BIT2,
        sending = BIT3,
        sent = BIT4,
        painting = BIT5,
        idle = BIT6,
        brightness = BIT7,
        color = BIT8
    } apa102_state_t;
};

#endif