
#include "station.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "dhcpserver/dhcpserver.h"

#include "esp_wifi.h"
#include "../rest/rest.h"
#include "../../main/utils.h"
#include <esp_pm.h>
#include <lwip/sockets.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

esp_event_loop_handle_t event_handle;
wifi_config_t wifi_config;
static the_wifi_config* config;

uint8_t s_retry_num=0;
esp_netif_t *sta_netif = NULL;
wifi_event_ap_staconnected_t* station = (wifi_event_ap_staconnected_t*)malloc(sizeof(wifi_event_ap_staconnected_t));
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
TaskHandle_t restHandle=NULL;
TaskHandle_t tripUploadHandle=NULL;

the_wifi_config*  getWifiConfig(){
    assert(config);
    return config;
}

void inferPassword(const char* sid, char* pwd) {
    sprintf(pwd,"8008");
    uint8_t sidLen=strlen(sid);
    for (int idx=sidLen-1; idx>=0; idx--) {
        pwd[4+idx]=sid[idx];
    }
    pwd[4+sidLen]=0;
}

bool startsWith(const char* str,const char* key){
    uint32_t sle =strlen(str);
    uint32_t kle =strlen(key);

    if ((sle == 0) || (kle==0) || (sle<kle)) {
        ESP_LOGV(__FUNCTION__,"%s in %s rejected because of bad len",key,str);
        return false;
    }

    for (int idx=0; idx < kle; idx++){
        if (str[idx] != key[idx]){
            ESP_LOGV(__FUNCTION__,"%s in %s rejected at idx %d",key,str,idx);
            return false;
        }
    }
    return true;
}

bool isSidManaged (const char* sid) {
    return ((startsWith(sid,"VIDEOETRON ")!=0) ||
            (startsWith(sid,"BELL ")!=0) ||
            (startsWith(sid,"Cogeco ")!=0) ||
            (startsWith(sid,"Linksys    ")!=0));
}

void generateSidConfig(wifi_config_t* wc) {
    switch (rand()%4) {
        case 0:
            sprintf((char*)wc->ap.ssid,"VIDEOETRON 2%04d",rand()%999);
            break;
        case 1:
            sprintf((char*)wc->ap.ssid,"BELL 1%04d",rand()%999);
            break;
        case 2:
            sprintf((char*)wc->ap.ssid,"Cogeco 3%04d.",rand()%999);
            break;
        case 3:
            sprintf((char*)wc->ap.ssid,"Linksys    4%04d",rand()%999);
            break;
    }
    wc->ap.ssid_len=strlen((char*)wc->ap.ssid);
    wc->ap.authmode=WIFI_AUTH_WPA_WPA2_PSK;
    wc->ap.max_connection=4;

    inferPassword((char*)wc->ap.ssid,(char*)wc->ap.password);
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
                        .max = 500
                    },
                    .passive = 500
                 }
    };

    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
    if (esp_wifi_scan_start(&scan_config, false) != ESP_OK) {
        ESP_LOGW(__FUNCTION__,"Cannot scan");
        return false;
    }
    xEventGroupSetBits(config->s_wifi_eg,WIFI_SCANING_BIT);
    ESP_LOGD(__FUNCTION__,"Scanning APs");
    return true;
}

void ProcessScannedAPs(){
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    esp_err_t ret;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_LOGD(__FUNCTION__, "Getting Scanned AP");
    if ((ret=esp_wifi_scan_get_ap_records(&number, ap_info))==ESP_OK){
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGD(__FUNCTION__, "Total APs scanned = %u", ap_count);
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
            if (LOG_LOCAL_LEVEL == ESP_LOG_DEBUG){
                ESP_LOGD(__FUNCTION__, "SSID \t\t%s", ap_info[i].ssid);
                ESP_LOGD(__FUNCTION__, "RSSI \t\t%d", ap_info[i].rssi);
                print_auth_mode(ap_info[i].authmode);
                if (ap_info[i].authmode != WIFI_AUTH_WEP) {
                    print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
                }
                ESP_LOGD(__FUNCTION__, "Channel \t\t%d\n", ap_info[i].primary);
            }
            if (isSidManaged((const char*)ap_info[i].ssid)){

                if (esp_wifi_scan_stop() != ESP_OK) {
                    ESP_LOGW(__FUNCTION__,"Cannot stop scan");
                }

                strcpy((char*)wifi_config.sta.ssid,(char*)ap_info[i].ssid);
                inferPassword((char*)wifi_config.sta.ssid,(char*)wifi_config.sta.password);
                memcpy(wifi_config.sta.bssid,ap_info[i].bssid,sizeof(uint8_t)*6);
                wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                ESP_LOGD(__FUNCTION__,"Connecting to %s/",ap_info[i].ssid);
                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

                if (esp_wifi_connect() == ESP_OK){
                    ESP_LOGD(__FUNCTION__,"Configured in Station Mode %s/%s",wifi_config.sta.ssid,wifi_config.sta.password);
                    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
                    return;
                }
                break;
            }
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Cannot get scan result: %s",esp_err_to_name(ret));
    }
    wifiScan();
}

Aper* GetAper(uint8_t* mac){
    uint8_t freeSlot=254;
    uint8_t emptySlot=254;
    for (int idx = 0; idx < MAX_NUM_CLIENTS; idx++) {
        if ((freeSlot == 254) && (clients[idx] == NULL)) {
            freeSlot=idx;
            ESP_LOGV(__FUNCTION__,"Free slot: %d", freeSlot);
            break;
        }
        if ((emptySlot == 254) && (clients[idx] != NULL) && (!clients[idx]->isConnected())) {
            emptySlot=idx;
            ESP_LOGV(__FUNCTION__,"Empty slot: %d",emptySlot);
        }
        if (memcmp(mac,&clients[idx]->mac,6)==0){
            return clients[idx];
        }
    }
    if (freeSlot != 254) {
        return clients[freeSlot] = new Aper(mac);
    }
    if (emptySlot != 254) {
        clients[emptySlot]->Update(mac);
        return clients[emptySlot];
    }
    return NULL;
}

void static network_event(void* handler_arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    Aper* client = NULL;
    if (base == IP_EVENT) {
        ESP_LOGD(__FUNCTION__,"ip event %d",event_id);
        ip_event_got_ip_t* event;
        system_event_info_t* evt;
        switch(event_id) {
            case IP_EVENT_STA_GOT_IP:
                event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(__FUNCTION__, "got ip::" IPSTR, IP2STR(&event->ip_info.ip));
                memcpy(&ipInfo,&event->ip_info,sizeof(ipInfo));
                xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
                if (restHandle == NULL) {
                    xTaskCreate(restSallyForth, "restSallyForth", 4096, getWifiConfig() , tskIDLE_PRIORITY, &restHandle);
                }
                break;
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(__FUNCTION__, "lost got ip");
                xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
                break;
            case IP_EVENT_AP_STAIPASSIGNED:
                if (station != NULL){
                    evt = (system_event_info_t*) event_data;
                    client = GetAper(station->mac);
                    if ((evt != NULL) && (tripUploadHandle==NULL)){
                        if (client != NULL) {
                            client->Update((ip4_addr_t*)&evt->ap_staipassigned.ip);
                        }
                        ESP_LOGI(__FUNCTION__, "Served ip::" IPSTR, IP2STR(&client->ip));
                        vTaskDelay(500/portTICK_RATE_MS);
                        xTaskCreate(pullStation, "pullStation", 4096, &client->ip , tskIDLE_PRIORITY, &tripUploadHandle);
                    } else {
                        ESP_LOGD(__FUNCTION__,"No ip, reconnected, re-trigger on all");
                        if ((client != NULL) && (((uint32_t)client->ip.addr) != 0)) {
                            xTaskCreate(pullStation, "pullStation", 4096, &client->ip , tskIDLE_PRIORITY, &tripUploadHandle);
                        } else {
                            wifi_sta_list_t wifi_sta_list;
                            tcpip_adapter_sta_list_t tcp_sta_list;
                            esp_wifi_ap_get_sta_list(&wifi_sta_list);

                            if(tcpip_adapter_get_sta_list(&wifi_sta_list, &tcp_sta_list) == ESP_OK)
                            {
                                for(uint8_t i = 0; i<wifi_sta_list.num ; i++)
                                {
                                    ESP_LOGD(__FUNCTION__,"Mac : %d , STA IP : %d\n",tcp_sta_list.sta[i].mac[0] ,tcp_sta_list.sta[i].ip.addr);
                                    ESP_LOGD(__FUNCTION__,"Num: %d , Mac : %d\n",wifi_sta_list.num,wifi_sta_list.sta[i].mac[0]);
                                    xTaskCreate(pullStation, "pullStation", 4096, &tcp_sta_list.sta[i].ip , tskIDLE_PRIORITY, &tripUploadHandle);
                                    vTaskDelay(500/portTICK_RATE_MS);
                                }
                            }
                            else
                            {
                                ESP_LOGE(__FUNCTION__,"Cant get sta list");
                            }
                        }
                    }
                } else {
                    ESP_LOGE(__FUNCTION__,"No Station to pull on");
                }
                break;
            default:
                ESP_LOGD(__FUNCTION__, "Unknown IP Event %d",event_id);
                break;
        }
    }
    if (base == WIFI_EVENT) {
        ESP_LOGD(__FUNCTION__,"wifi event %d",event_id);
        switch(event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(__FUNCTION__, "Wifi is up");
                xEventGroupSetBits(config->s_wifi_eg,WIFI_UP_BIT);
                if (xEventGroupGetBits(config->s_wifi_eg)&WIFI_SCAN_READY_BIT) {
                    wifiScan();
                } else if (esp_wifi_connect() == ESP_OK) {
                    ESP_LOGD(__FUNCTION__,"Configured in Station Mode");
                    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
                    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
                }
                break;
            case WIFI_EVENT_STA_STOP:
                xEventGroupSetBits(config->s_wifi_eg,WIFI_DOWN_BIT);
                xEventGroupClearBits(config->s_wifi_eg,WIFI_UP_BIT);
                xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
                xEventGroupClearBits(config->s_wifi_eg,WIFI_CONNECTED_BIT);
                xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
                break;
            case WIFI_EVENT_SCAN_DONE:
                xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
                ProcessScannedAPs();
                break;
            case WIFI_EVENT_AP_START:
                xEventGroupSetBits(config->s_wifi_eg,WIFI_UP_BIT);
                xEventGroupClearBits(config->s_wifi_eg,WIFI_DOWN_BIT);
                ESP_LOGD(__FUNCTION__,"AP STARTED");
                xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
                tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo);
                ESP_LOGI(__FUNCTION__, "AP ip::" IPSTR, IP2STR(&ipInfo.ip));
                if (restHandle == NULL) {
                    xTaskCreate(restSallyForth, "restSallyForth", 4096, getWifiConfig() , tskIDLE_PRIORITY, &restHandle);
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                memcpy((void*)station,(void*)event_data,sizeof(wifi_event_ap_staconnected_t));
                ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d",
                    MAC2STR(station->mac), station->aid);
                client = GetAper(station->mac);
                if (client != NULL) {
                    client->Associate();
                }
                initSPISDCard();
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Disconnected, AID=%d",
                MAC2STR(station->mac), station->aid);
                client = GetAper(station->mac);
                if (client != NULL) {
                    client->Dissassociate();
                }
                deinitSPISDCard();
                if (tripUploadHandle != NULL) {
                    //vTaskDelete(tripUploadHandle);
                    tripUploadHandle=NULL;
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                wifi_ap_record_t ap_info;
                esp_wifi_sta_get_ap_info(&ap_info);
                initSPISDCard();
                ESP_LOGI(__FUNCTION__,"Wifi Connected to %s",ap_info.ssid);
                //esp_netif_dhcpc_start(sta_netif);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(__FUNCTION__, "Got disconnected from AP");
                xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
                deinitSPISDCard();
                if (!(xEventGroupGetBits(config->s_wifi_eg)&WIFI_CLIENT_DONE)){
                    esp_wifi_connect();
                }
                break;
            default:
                ESP_LOGD(__FUNCTION__, "Unknown Wifi Event %d",event_id);
                break;
        }
    }
}

esp_err_t initWifi(){
    ESP_LOGD(__FUNCTION__,"Initializing netif");
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGD(__FUNCTION__,"Initialized netif");

    if (config->wifi_mode == WIFI_MODE_AP) {
        if (sta_netif==NULL)
            sta_netif=esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL));
        ESP_ERROR_CHECK(esp_netif_dhcps_start(sta_netif));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
    } else {
        if (sta_netif==NULL){
            ESP_LOGD(__FUNCTION__,"creating netif");
            sta_netif=esp_netif_create_default_wifi_sta();
            ESP_LOGD(__FUNCTION__,"created netif");
        }
        ESP_LOGD(__FUNCTION__,"Creating netif %li",(long int)sta_netif);
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL));
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
    }

    s_retry_num = 0;
    ESP_LOGD(__FUNCTION__, "esp_wifi_start finished.");

    return ESP_OK;
}

void wifiStop(void* pvParameter) {
    //if (xEventGroupGetBits(config->s_wifi_eg) & WIFI_CONNECTED_BIT)
        esp_wifi_disconnect();
    //if (xEventGroupGetBits(config->s_wifi_eg) & WIFI_UP_BIT){
        esp_wifi_stop();
        esp_wifi_deinit();
    //}
    esp_netif_deinit();
    sta_netif=NULL;
    //vTaskDelete(NULL);
}

void wifiStart(void *pvParameter) {
    ESP_LOGD(__FUNCTION__,"Initing wifi");
    //if (!nvs_flash_init()){
        if (initWifi()) {
            ESP_LOGE(__FUNCTION__, "Wifi init failed");
        }
    //} else {
    //    ESP_LOGE(__FUNCTION__, "Cannot init nvs");
    //}
    //vTaskDelete( NULL );
}

void wifiSallyForth(void *pvParameter) {
    esp_pm_config_esp32_t pm_config;
    pm_config.max_freq_mhz=240;
    pm_config.min_freq_mhz=240;
    pm_config.light_sleep_enable=false;
    memset(&ipInfo,0,sizeof(ipInfo));

    esp_err_t ret;
    if((ret = esp_pm_configure(&pm_config)) != ESP_OK) {
        printf("pm config error %s\n",
                ret == ESP_ERR_INVALID_ARG ?
                "ESP_ERR_INVALID_ARG":"ESP_ERR_NOT_SUPPORTED");
    }

    config = (the_wifi_config*)malloc(sizeof(the_wifi_config));
    memset(config,0,sizeof(the_wifi_config));
    config->s_wifi_eg = xEventGroupCreate();
    config->disconnectWaitTime = 2000;
    config->poolWaitTime = 3000;
    xEventGroupSetBits(config->s_wifi_eg,WIFI_DOWN_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_UP_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCANING_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_CONNECTED_BIT);
    xEventGroupClearBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);

    if (pvParameter != NULL){
        ESP_LOGD(__FUNCTION__,"Station");
        config->wifi_mode = WIFI_MODE_STA;
        wifi_config.sta.pmf_cfg.capable=true;
        wifi_config.sta.pmf_cfg.required=false;
        memset(clients,0,sizeof(void*)*MAX_NUM_CLIENTS);
        xEventGroupSetBits(config->s_wifi_eg,WIFI_SCAN_READY_BIT);
    } else {
        ESP_LOGD(__FUNCTION__,"Access Point");
        config->wifi_mode = WIFI_MODE_AP;
        generateSidConfig(&wifi_config);
        ESP_LOGD(__FUNCTION__,"AP %s/%s",wifi_config.ap.ssid,wifi_config.ap.password);
    }
    wifiStart(NULL);
    vTaskDelete( NULL );
}

Aper::Aper(uint8_t station[6])
{
    Update(station);
    memset(&ip,0,sizeof(esp_ip4_addr_t));
}

Aper::~Aper()
{
}

void Aper::Update(uint8_t station[6]){
    assert(station);
    memcpy(mac,station,sizeof(uint8_t)*6);
    memset(&ip,0,sizeof(esp_ip4_addr_t));
    connectionTime=0;
    connectTime=0;
    disconnectTime=0;
}

void Aper::Update(ip4_addr_t* station){
    if (station == NULL){
        memset(&ip,0,sizeof(esp_ip4_addr_t));
    } else {
        memcpy(&ip,station,sizeof(esp_ip4_addr_t));
    }
}

void Aper::Dissassociate()
{
    time(&disconnectTime);
    connectionTime+=(disconnectTime-connectTime);
    connectTime=0;
}

void Aper::Associate()
{
    time(&connectTime);
    disconnectTime=0;
}

bool Aper::isConnected()
{
    return ((connectTime!=0) && (disconnectTime==0));
}

bool Aper::isSameDevice(Aper* other){
    assert(other);
    for (int idx=0; idx < 6; idx++){
        if (other->mac[idx] != mac[idx]){
            return false;
        }
    }
    return true;
}

cJSON* Aper::toJson(){
    char* tbuf = (char*)malloc(255);
    cJSON* j = cJSON_CreateObject();
    sprintf(tbuf, "%02x:%02x:%02x:%02x:%02x:%02x",
        MAC2STR(mac));
    cJSON_AddStringToObject(j,"mac",tbuf);
    sprintf(tbuf, IPSTR, IP2STR(&ip));
    cJSON_AddStringToObject(j,"ip",tbuf);
    struct tm timeinfo;
    if (connectTime != 0){
        localtime_r(&connectTime, &timeinfo);
        strftime(tbuf, 255, "%c", &timeinfo);
        cJSON_AddStringToObject(j,"connectTime",tbuf);
    }
    if (disconnectTime != 0){
        localtime_r(&disconnectTime, &timeinfo);
        strftime(tbuf, 255, "%c", &timeinfo);
        cJSON_AddStringToObject(j,"disconnectTime",tbuf);
    }
    if (connectionTime) {
        localtime_r(&connectionTime, &timeinfo);
        strftime(tbuf, 255, "%c", &timeinfo);
        cJSON_AddStringToObject(j,"connectionTime",tbuf);
    }
    if (isConnected())
        cJSON_AddTrueToObject(j,"isConnected");
    else 
        cJSON_AddFalseToObject(j,"isConnected");
    free(tbuf);
    return j;
}

uint32_t getClientsJson(char* buf){
    cJSON * j = NULL;
    cJSON * prev = NULL;
    cJSON* js[MAX_NUM_CLIENTS];
    memset(&js,0,MAX_NUM_CLIENTS*sizeof(void*));

    for (int idx=0;idx < MAX_NUM_CLIENTS; idx++){
        Aper* client = clients[idx];
        if (client) {
            js[idx]=client->toJson();
            if (j == NULL) {
                j = cJSON_CreateArray();
                j->child=js[idx];
            } else {
                prev->next=js[idx];
                js[idx]->prev=prev;
            }
            prev=js[idx];
        }
    }
    if (prev == NULL) {
        return sprintf(buf,"%s","[]");
    }
    sprintf(buf,"%s",cJSON_PrintUnformatted(j));
    cJSON_Delete(j);
    return (strlen(buf));
}