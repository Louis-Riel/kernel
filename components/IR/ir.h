#ifndef __ir_h
#define __ir_h

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
#include "driver/rmt.h"

class IRDecoder:ManagedDevice {
public:
    IRDecoder(AppConfig* config);
    ~IRDecoder();
    enum eventIds {
        CODE
    };
    static bool IsRunning();
protected:
    static void IRPoller(void *arg);
    gpio_num_t pinNo;
    rmt_channel_t channelNo;
    EventHandlerDescriptor* BuildHandlerDescriptors();
    static const char* IRDECODER_BASE;

    int8_t available();
    uint32_t read(rmt_item32_t* item, size_t length);
    void setMargin(uint16_t margin_us);
    bool inPrefVector(uint8_t element);
    int setPreferred(const char* timing_group);
private:
    typedef struct {
        const char* tag;
        uint16_t carrier_freq_hz;
        uint8_t duty_cycle;
        uint8_t bit_length;
        bool invert;
        uint16_t header_mark_us;
        uint16_t header_space_us;
        uint16_t one_mark_us;
        uint16_t one_space_us;
        uint16_t zero_mark_us;
        uint16_t zero_space_us;
    } rmt_timing_t;

    AppConfig* config;
    cJSON* lastCode;
    cJSON* lastProvider;
    cJSON* numCodes;
    RingbufHandle_t rb;
    char* timingGroup;
    uint16_t _margin_us = 80;
    std::vector<uint8_t> _preferred;
    rmt_timing_t _timing;

    static rmt_timing_t timing_groups[];
    bool rx_check_in_range(int duration_ticks, int target_us);
    bool rx_header_if(rmt_item32_t* item, uint8_t timing);
    bool rx_bit_one_if(rmt_item32_t* item, uint8_t timing);
    bool rx_bit_zero_if(rmt_item32_t* item, uint8_t timing);
    uint32_t rx_parse_items(rmt_item32_t* item, int item_num, uint8_t timing);
    uint8_t timingGroupElement(const char* tag);

};

#endif