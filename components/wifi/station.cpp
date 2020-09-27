#include "station.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "freertos/semphr.h"
#include "esp_event.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "rest.h"
#include "../TinyGPS/TinyGPS++.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

esp_event_loop_handle_t event_handle;

uint8_t s_retry_num=0;
static wifi_config* config;
esp_netif_t *sta_netif = NULL;

bool isWifiying() {
    return xEventGroupGetBits(config->s_wifi_eg) & WIFI_UP_BIT;
}

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    default:
        ESP_LOGD(__FUNCTION__, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGD(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGD(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

bool wifiScan(){
    wifi_scan_config_t scan_config = {
	.ssid = 0,
	.bssid = 0,
	.channel = 0,
    .show_hidden = false,
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time = {
                    .active = {
                        .min = 100,
                        .max = 1000
                    },
                    .passive = 500
                 }
    };

    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
    if (esp_wifi_scan_start(&scan_config, false) != ESP_OK) {
        ESP_LOGW(__FUNCTION__,"Cannot scan");
        return false;
    }
    ESP_LOGD(__FUNCTION__,"Scanning APs");
    xEventGroupSetBits(config->s_wifi_eg,WIFI_SCANING_BIT);
    return true;
}

void ProcessScannedAPs(){
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_LOGD(__FUNCTION__, "Getting Scanned AP");
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGD(__FUNCTION__, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
            ESP_LOGD(__FUNCTION__, "SSID \t\t%s", ap_info[i].ssid);
            ESP_LOGD(__FUNCTION__, "RSSI \t\t%d", ap_info[i].rssi);
            print_auth_mode(ap_info[i].authmode);
            if (ap_info[i].authmode != WIFI_AUTH_WEP) {
                print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
            }
            ESP_LOGD(__FUNCTION__, "Channel \t\t%d\n", ap_info[i].primary);
        }
        if (strcmp((const char*)config->wname,(const char*)ap_info[i].ssid)==0){
            wifi_config_t wifi_config;
            strcpy((char*)wifi_config.sta.ssid,config->wname);
            strcpy((char*)wifi_config.sta.password,config->wpdw);
            memcpy(wifi_config.sta.bssid,ap_info[i].bssid,sizeof(uint8_t)*6);
            wifi_config.sta.channel=ap_info[i].primary;
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

            if (esp_wifi_connect() == ESP_OK){
                ESP_LOGD(__FUNCTION__,"%s Configured in Station Mode %s/%s",__func__,wifi_config.sta.ssid,wifi_config.sta.password);
                xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
            }
            break;
        }
    }
}

esp_err_t system_event_handler(void *ctx, system_event_t *event)
{
    ESP_LOGI(__FUNCTION__,"Sytstem event id:%d",event->event_id);
    switch(event->event_id) {
        case SYSTEM_EVENT_SCAN_DONE:
            ProcessScannedAPs();
            break;
        case SYSTEM_EVENT_STA_START:
            if (xEventGroupGetBits(config->s_wifi_eg)&WIFI_SCAN_READY_BIT)
                wifiScan();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(__FUNCTION__, "got ip::" IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            xTaskCreate(restSallyForth, "restSallyForth", 4096, config , tskIDLE_PRIORITY, NULL);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            wifi_ap_record_t ap_info;
            esp_wifi_sta_get_ap_info(&ap_info);
            ESP_LOGI(__FUNCTION__,"Wifi Connected to %s",ap_info.ssid);
            esp_netif_dhcpc_start(sta_netif);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(__FUNCTION__, "Got disconnected from AP");
            xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            esp_wifi_connect();
            break;
        default:
            ESP_LOGI(__FUNCTION__,"Unhandled system event id:%d",event->event_id);
            break;
    }
    return ESP_OK;
}

void network_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    ESP_LOGD(__FUNCTION__,"EVDNENT!!!!");
}

esp_err_t initWifi(){
    ESP_LOGD(__FUNCTION__,"%s Initializing netif",__func__);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGD(__FUNCTION__,"Initializing event loop");
    ESP_ERROR_CHECK(esp_event_loop_init(system_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGD(__FUNCTION__,"Initializing wifi");

    if (config->wifi_mode == WIFI_MODE_AP) {
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        wifi_config_t wifi_config;
        memset(&wifi_config,0,sizeof(wifi_config_t));
        ESP_LOGD(__FUNCTION__,"%s Configured in AP Mode %s",__func__,config->wname);
        strcpy((char*)&wifi_config.ap.ssid[0],&config->wname[0]);
        strcpy((char*)&wifi_config.ap.password[0],&config->wpdw[0]);
        wifi_config.ap.max_connection=4;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
        xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
    } else {
        sta_netif=esp_netif_create_default_wifi_sta();
        ESP_LOGD(__FUNCTION__,"Creating netif %li",(long int)sta_netif);
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL, NULL));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_LOGD(__FUNCTION__,"Setting Scan Ready Bit");
        xEventGroupSetBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
        ESP_LOGD(__FUNCTION__,"Starting Wifi");
        ESP_ERROR_CHECK(esp_wifi_start() );
        wifiScan();
    }

    s_retry_num = 0;
    xEventGroupSetBits(config->s_wifi_eg,WIFI_UP_BIT);
    ESP_LOGD(__FUNCTION__, "esp_wifi_start finished.");

    return ESP_OK;
}

static void gpsEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  switch (id)
  {
  case TinyGPSPlus::gpsEvent::atSyncPoint:
    ESP_LOGD(__FUNCTION__, "Synching");
    xTaskCreate(wifiStart, "wifiStart", 4096, NULL , tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::outSyncPoint:
    ESP_LOGD(__FUNCTION__, "Leaving Synching");
    xTaskCreate(wifiStop, "wifiStop", 4096, NULL , tskIDLE_PRIORITY, NULL);
    break;
  default:
    break;
  }
}

void wifiStop(void* pvParameter) {
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_event_loop_delete(event_handle));
    xEventGroupClearBits(config->s_wifi_eg,WIFI_UP_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_CONNECTED_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
    sta_netif=NULL;
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &network_event));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event));
}

void wifiStart(void *pvParameter) {
    if (pvParameter != NULL) {
        config = (wifi_config*)pvParameter;
    } else {
        ESP_LOGE(__FUNCTION__,"%s Missing config, going default",__func__);
        config = (wifi_config*)malloc(sizeof(wifi_config));
        memset(config,0,sizeof(wifi_config));
        config->workPeriod = 11;
        config->scanPeriod = 4000;
        config->disconnectWaitTime = 2000;
        config->poolWaitTime = 3000;
        config->wifi_mode = WIFI_MODE_STA;
        strcpy((char*)config->wname, "BELL820");
        strcpy((char*)config->wpdw, "5FE39625");
        //strcpy((char*)config->wname, "kvhCrib");
        //strcpy((char*)config->wpdw, "W1f1Passw0rd8008");
    }
    config->s_wifi_eg = xEventGroupCreate();

    ESP_LOGD(__FUNCTION__,"%s initing wifi",__func__);
    if (!nvs_flash_init()){
        if (initWifi()) {
            ESP_LOGE(__FUNCTION__, "%s wifi init failed", __func__);
            vTaskDelete( NULL );
        }
    } else {
        ESP_LOGE(__FUNCTION__, "Cannot init nvs");
        vTaskDelete( NULL );
    }

    vTaskDelete( NULL );
}

void wifiSallyForth(void *pvParameter) {
    TinyGPSPlus* gps = (TinyGPSPlus*)pvParameter;
    ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::atSyncPoint, gpsEvent, &gps));
    ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::outSyncPoint, gpsEvent, &gps));
    //xTaskCreate(wifiStart, "wifiStart", 4096, NULL , tskIDLE_PRIORITY, NULL);

    vTaskDelete( NULL );
}
