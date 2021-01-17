#ifndef __station_h
#define __station_h

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "cJSON.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_SCAN_READY_BIT BIT1
#define WIFI_SCANING_BIT BIT2
#define WIFI_UP_BIT BIT3
#define WIFI_DOWN_BIT BIT4
#define WIFI_CLIENT_DONE BIT5
#define WIFI_STA_CONFIGURED BIT5

#define DEFAULT_SCAN_LIST_SIZE 10
#define MAX_NUM_CLIENTS 20

typedef	struct {
                uint32_t                        disconnectWaitTime;
                uint32_t                        poolWaitTime;
                EventGroupHandle_t              s_wifi_eg;
                QueueHandle_t                   s_wf_msgqueue;
                EventGroupHandle_t              s_bt_eg;
                wifi_mode_t                     wifi_mode;
                char                            wname[40];
                char                            wpdw[40];
} the_wifi_config;

class Aper
{
public:
    uint8_t mac[6];
    ip4_addr_t ip;
    struct tm timeinfo;
    time_t connectTime = 0;
    time_t disconnectTime = 0;
    time_t connectionTime = 0;

    Aper(uint8_t station[6]);
    ~Aper();

    void Associate();
    void Dissassociate();

    void Update(ip4_addr_t* station);
    void Update(uint8_t station[6]);

    bool isSameDevice(Aper* other);
    bool isConnected();
    cJSON* toJson();
};

Aper** GetClients();
tcpip_adapter_ip_info_t* GetIpInfo();
void wifiSallyForth(void *pvParameter);
void wifiStart(void *pvParameter);
void wifiStop(void *pvParameter);
the_wifi_config*  getWifiConfig();
#endif