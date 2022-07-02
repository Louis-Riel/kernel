#ifndef __station_h
#define __station_h

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "eventmgr.h"
#include "cJSON.h"
#include "../../main/utils.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_SCANING_BIT BIT1
#define WIFI_AP_UP_BIT BIT2
#define WIFI_STA_UP_BIT BIT3
#define WIFI_STA_CONFIGURED BIT4
#define WIFI_DISCONNECTED_BIT BIT5

#define DEFAULT_SCAN_LIST_SIZE 10
#define MAX_NUM_CLIENTS 20


void wifiSallyForth(void *pvParameter);

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

class TheWifi:ManagedDevice {
public:
    TheWifi(AppConfig* config);
    ~TheWifi();
    bool state;
    bool valid;
    EventGroupHandle_t eventGroup;

    tcpip_adapter_ip_info_t* GetStaIp();
    tcpip_adapter_ip_info_t* GetApIp();
    bool wifiScan();
    static TheWifi* GetInstance();
    static EventGroupHandle_t GetEventGroup();
    AppConfig* cfg;
    AppConfig* stationStat;
    AppConfig* apStat;
    tcpip_adapter_ip_info_t apIp;
    tcpip_adapter_ip_info_t staIp;

protected:
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    EventHandlerDescriptor* BuildHandlerDescriptors();

    void ParseStateBits(AppConfig* state);
    Aper** GetClients();
    static void network_event(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data);

    Aper *clients[MAX_NUM_CLIENTS];
    wifi_config_t wifi_config;
    char* name;
    cJSON* status;
private:
    int RefreshApMembers(AppConfig* state);
    void InferPassword(const char *sid, char *pwd);
    bool isSidPuller(const char *sid, bool isTracker);
    void generateSidConfig(wifi_config_t *wc, bool hasGps);
    void ProcessScannedAPs();
    bool isSidManaged(const char *sid, bool isTracker);
    Aper *GetAperByMac(uint8_t *mac);
    Aper *GetAperByIp(esp_ip4_addr_t ip);
    
    EventGroupHandle_t              s_app_eg;
    esp_event_handler_instance_t wifiEvtHandler;
    esp_event_handler_instance_t ipEvtHandler;
    uint32_t healthCheckCount;
};

#endif