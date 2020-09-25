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

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

uint8_t s_retry_num=0;
static wifi_config* config;
static bool wifiActive=false;
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    ESP_LOGD(__FUNCTION__,"Wifi Event e:%d ei:%d ed:%li",(int)event_base,event_id,(long int)event_data);
    uint32_t waitTime=0;
    ip_event_got_ip_t* event=NULL;
    wifi_event_ap_stadisconnected_t* event2=NULL;
    switch (event_id) {
        case WIFI_EVENT_STA_DISCONNECTED:
            if (xEventGroupGetBits(config->s_wifi_eg) | WIFI_CONNECTED_BIT) {
                waitTime=config->disconnectWaitTime;
                ESP_LOGI(__FUNCTION__, "Got disconnected from AP");
            } else if (s_retry_num++ < 10) {
                waitTime=config->poolWaitTime;
                ESP_LOGI(__FUNCTION__, "retry to connect to the AP");
            } else {
                ESP_LOGI(__FUNCTION__, "Failed too many times, taking a longer break");
                waitTime=60000;
            }
            xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
            vTaskDelay(pdMS_TO_TICKS(waitTime));
            xEventGroupClearBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
            esp_wifi_connect();
            break;
        case IP_EVENT_STA_GOT_IP:
            if (event_base == IP_EVENT){
                event = (ip_event_got_ip_t*) event_data;
                if (event != NULL) {
                    ESP_LOGI(__FUNCTION__, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
                } else {
                    ESP_LOGI(__FUNCTION__, "got no ip");
                }
                s_retry_num = 0;
                xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
                xEventGroupSetBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
            }
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            event2 = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(__FUNCTION__,"station "MACSTR" leave, AID=%d",
            MAC2STR(event2->mac), event2->aid);
            break;
        case WIFI_EVENT_AP_START:
            ESP_LOGD(__FUNCTION__,"WIFI_EVENT_AP_START");
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGD(__FUNCTION__,"WIFI_EVENT_AP_STOP");
            break;
    }
}

esp_err_t initWifi(){
    ESP_LOGD(__FUNCTION__,"%s Initializing netif",__func__);

    ESP_ERROR_CHECK(esp_netif_init());

    wifi_config_t wifi_config;
    memset(&wifi_config,0,sizeof(wifi_config_t));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.event_handler=(system_event_handler_t)&event_handler;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //esp_event_loop_args_t loop_args = {
    //    .queue_size = CONFIG_ESP_SYSTEM_EVENT_QUEUE_SIZE,
    //    .task_name = "sys_evt",
    //    .task_priority = ESP_TASKD_EVENT_PRIO,
    //    .task_stack_size = ESP_TASKD_EVENT_STACK,
    //    .task_core_id = tskNO_AFFINITY
    //};
    //ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &loop_handle));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));

    if (config->wifi_mode == WIFI_MODE_AP) {
        ESP_LOGD(__FUNCTION__,"%s Configured in AP Mode %s",__func__,config->wname);
        strcpy((char*)&wifi_config.ap.ssid[0],&config->wname[0]);
        strcpy((char*)&wifi_config.ap.password[0],&config->wpdw[0]);
        wifi_config.ap.max_connection=4;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    } else {
        ESP_LOGD(__FUNCTION__,"%s Configured in Station Mode %s",__func__,config->wname);
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
        strcpy((char*)&wifi_config.sta.ssid[0],&config->wname[0]);
        strcpy((char*)&wifi_config.sta.password[0],&config->wpdw[0]);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(config->wifi_mode) );
    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
    if (config->wifi_mode == WIFI_MODE_AP) {
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    }
    ESP_ERROR_CHECK(esp_wifi_start() );

    xEventGroupSetBits(config->s_wifi_eg,WIFI_UP_BIT);
    ESP_LOGD(__FUNCTION__, "esp_wifi_start finished.");

    if (config->wifi_mode == WIFI_MODE_AP) {
       // vTaskDelay(pdMS_TO_TICKS(2000));

        s_retry_num = 0;
        xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);

    }
    return ESP_OK;
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

void wifiScan() {
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    wifi_scan_config_t scan_config = {
	.ssid = 0,
	.bssid = 0,
	.channel = 0,
    .show_hidden = true,
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time = {
                    .active = {
                        .min = 100,
                        .max = 1000
                    },
                    .passive = 500
                 }
    };

    TickType_t xLastWakeTime=xTaskGetTickCount();
    while(xEventGroupWaitBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT,pdFALSE,pdFALSE,portMAX_DELAY)){
        ESP_LOGD(__FUNCTION__, "Scanning APs");
        xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
        xEventGroupSetBits(config->s_wifi_eg,WIFI_SCANING_BIT);
        if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGD(__FUNCTION__, "Getting Scanned AP");
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGD(__FUNCTION__, "Total APs scanned = %u", ap_count);
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
            ESP_LOGD(__FUNCTION__, "SSID \t\t%s", ap_info[i].ssid);
            ESP_LOGD(__FUNCTION__, "RSSI \t\t%d", ap_info[i].rssi);
            print_auth_mode(ap_info[i].authmode);
            if (ap_info[i].authmode != WIFI_AUTH_WEP) {
                print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
            }
            ESP_LOGD(__FUNCTION__, "Channel \t\t%d\n", ap_info[i].primary);
        }

        ESP_ERROR_CHECK(esp_wifi_scan_stop());
        xEventGroupSetBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
        xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
        ESP_LOGD(__FUNCTION__,"%d >= %d",xTaskGetTickCount()-xLastWakeTime , pdMS_TO_TICKS(config->scanPeriod ));
        if ( (xTaskGetTickCount()-xLastWakeTime) >= pdMS_TO_TICKS(config->scanPeriod )) {
            ESP_LOGD(__FUNCTION__, "Done Scanned AP");
            break;
        }
    }
}


void wifiSallyForth(void *pvParameter) {
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
    }
    config->s_wifi_eg = xEventGroupCreate();

    ESP_LOGD(__FUNCTION__,"%s initing wifi",__func__);
    if (!nvs_flash_init()){
        if (initWifi()) {
            ESP_LOGE(__FUNCTION__, "%s wifi init failed", __func__);
            vTaskDelete( NULL );
        }
        xTaskCreate(restSallyForth, "restSallyForth", 4096, config , tskIDLE_PRIORITY, NULL);
    } else {
        ESP_LOGE(__FUNCTION__, "Cannot init nvs");
        vTaskDelete( NULL );
    }
    wifiActive=true;
    //wifiScan(pvParameter);
    TickType_t xLastWakeTime;
    while(wifiActive){
        xLastWakeTime= xTaskGetTickCount();
	    //wifiScan();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS( config->workPeriod*1000 ));
    }

    vTaskDelete( NULL );
}
