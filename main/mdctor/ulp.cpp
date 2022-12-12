#include "soc/rtc.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ulp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include <time.h>
#include <sys/time.h>
#include "esp_event.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <stdlib.h>
#include "esp32/clk.h"
#include "sdkconfig.h"
#include "math.h"


#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define GPS_BAUD_RATE 9600
#define GPS_BIT_STACK_LEN 20
#define portTICK_PERIOD_MMS (portTICK_PERIOD_MS/1000.0)

ESP_EVENT_DEFINE_BASE(SERIAL_EVENTS);

// calibrate 8M/256 clock against XTAL, get 8M/256 clock period
uint32_t rtc_8md256_period = 0;
uint32_t rtc_fast_freq_hz = 0;
uint32_t bit_tick_len = 0;
gpio_num_t pin;

typedef struct {
    uint64_t rizingTs;
    uint64_t loweringTs;
    uint32_t cnt;
    uint32_t len;
    uint8_t lastState;
} uaCalib_t;

uint32_t pushIdx = 0;
uint32_t numOnes;
uint32_t numZeros;

uint32_t diff;
QueueHandle_t serial_queue;
EventGroupHandle_t pinState;
EventGroupHandle_t serialState;
esp_timer_handle_t periodic_timer;

void* theFreakinArg=0;
TaskHandle_t calibrator=NULL;
typedef enum {
    rizing=BIT0,
    lowering=BIT1
} pin_state_t;

typedef enum {
    start_bit=BIT0,
    data_bits=BIT1,
    stop_bit=BIT2,
    init=BIT3
} serial_state_t;


serial_state_t sstate = serial_state_t::start_bit;
uint8_t curByte=0;
uint32_t curBytePos=1;
uint32_t periodLen=0;
int level=0;
uaCalib_t curCalib;

void setByteBit(pin_state_t val,uint32_t periodLen) {
    if (val==pin_state_t::rizing){
        curByte|=1UL<<(curBytePos++);
    } else
    {
        curByte&=~(1UL<<(curBytePos++));
    }
    if (curBytePos == 9){
        //printf("+");
        sstate=serial_state_t::stop_bit;
    }
}


void bitBlit(void* args) {
    level=gpio_get_level(pin);
    //printf(level?"0":"1");
    if (level==0){
        curCalib.loweringTs=esp_clk_rtc_time();
        periodLen=labs(curCalib.rizingTs-curCalib.loweringTs);

        switch (sstate) {
            case serial_state_t::init:
                sstate=serial_state_t::stop_bit;
                break;
            case serial_state_t::data_bits:
                setByteBit(pin_state_t::lowering,periodLen);
                break;
            case serial_state_t::start_bit:
                if (periodLen<bit_tick_len){
                    sstate=serial_state_t::data_bits;
                    //printf(".");
                    //ESP_LOGI(__FUNCTION__,"Start periodLen:%d bit_tick_len:%dstart:%" PRId64 " end:%" PRId64 ".",periodLen,bit_tick_len,curCalib.rizingTs,curCalib.loweringTs);
                } else {
                    sstate=serial_state_t::stop_bit;
                    //printf("&");
                    ESP_LOGW(__FUNCTION__,"Bad start periodLen:%d bit_tick_len:%dstart:%" PRId64 " end:%" PRId64 ".",periodLen,bit_tick_len,curCalib.rizingTs,curCalib.loweringTs);
                }
                curByte=0;
                curBytePos=1;
                break;
            default:
                //printf("^");
                curByte=0;
                curBytePos=1;
                sstate=serial_state_t::stop_bit;
                break;
        }
    } else {
        curCalib.rizingTs=esp_clk_rtc_time();
        periodLen=labs(curCalib.rizingTs-curCalib.loweringTs);

        switch (sstate) {
            case serial_state_t::data_bits:
                setByteBit(pin_state_t::rizing,periodLen);
            case serial_state_t::stop_bit:
                if (curBytePos==9) {
                    ESP_LOGI(__FUNCTION__,"WE GOT A CHAR:%d",curByte);
                }
                sstate=serial_state_t::start_bit;
                break;
            default:
                //printf("*");
                curByte=0;
                curBytePos=1;
                sstate=serial_state_t::stop_bit;
                break;
        }
    }
}

static void serialer(void *arg)
{
    if (gpio_get_level(pin) == 1){
        usleep(bit_tick_len/2);
        ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, bit_tick_len));
        gpio_isr_handler_remove(pin);
    }
}

void initUlp(gpio_num_t pinNo) {
    if (calibrator != NULL) {
        return;
    }

    while (rtc_8md256_period==0){
        rtc_8md256_period = rtc_clk_cal(RTC_CAL_8MD256, 100);
        if (rtc_8md256_period == 0)
            vTaskDelay(100/portTICK_PERIOD_MS);
    }
    rtc_fast_freq_hz = 1000000ULL * (1 << RTC_CLK_CAL_FRACT) * 256 / rtc_8md256_period;
    bit_tick_len = (1000000ULL/GPS_BAUD_RATE);
    pin=pinNo;
    pinState=xEventGroupCreate();
    serialState=xEventGroupCreate();

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &bitBlit,
            .name = "periodic"
    };
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

    ESP_LOGI(__FUNCTION__,"portTICK_PERIOD_MMS:%f rtc_8md256_period:%d rtc_fast_freq_hz:%d bit tick len:%d",portTICK_PERIOD_MMS,rtc_8md256_period,rtc_fast_freq_hz,bit_tick_len);

    serial_queue = xQueueCreate(10, sizeof(uint32_t));
    sstate=serial_state_t::stop_bit;
    ESP_ERROR_CHECK(gpio_isr_handler_add(pinNo, serialer, (void *)pinNo));
}