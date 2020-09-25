//#ifndef __station_h
//#define __station_h

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_SCAN_READY_BIT BIT2
#define WIFI_SCANING_BIT BIT4
#define WIFI_UP_BIT BIT3

#define DEFAULT_SCAN_LIST_SIZE 10

typedef	struct {
                uint32_t                        workPeriod;
                uint32_t                        scanPeriod;
                uint32_t                        disconnectWaitTime;
                uint32_t                        poolWaitTime;
                EventGroupHandle_t              s_wifi_eg;
                QueueHandle_t                   s_wf_msgqueue;
                EventGroupHandle_t              s_bt_eg;
                wifi_mode_t                     wifi_mode;
                char                            wname[40];
                char                            wpdw[40];
	} wifi_config;

void wifiSallyForth(void *pvParameter);

//#endif