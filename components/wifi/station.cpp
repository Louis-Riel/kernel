
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
#include "bootloader_random.h"

#include "esp_wifi.h"
#include "../rest/rest.h"
#include "../../main/utils.h"
#include <esp_pm.h>
#include <lwip/sockets.h>
#include "../eventmgr/eventmgr.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include <time.h>
#include <sys/time.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

wifi_config_t wifi_config;
static the_wifi_config *config;
static Aper *clients[MAX_NUM_CLIENTS];
static tcpip_adapter_ip_info_t ipInfo;

uint8_t s_retry_num = 0;
esp_netif_t *sta_netif = NULL;
esp_netif_t *ap_netif = NULL;
wifi_event_ap_staconnected_t *station = (wifi_event_ap_staconnected_t *)dmalloc(sizeof(wifi_event_ap_staconnected_t));
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
TaskHandle_t restHandle = NULL;
TaskHandle_t timeHandle = NULL;

Aper **GetClients()
{
    return clients;
}

tcpip_adapter_ip_info_t *GetIpInfo()
{
    return &ipInfo;
}

the_wifi_config *getWifiConfig()
{
    assert(config);
    return config;
}

void InferPassword(const char *sid, char *pwd)
{
    AppConfig *cfg = GetAppConfig();
    cJSON *stations = cfg->GetJSONConfig("/stations");
    if ((stations != NULL) && (cJSON_IsArray(stations)))
    {
        cJSON *ap = NULL;
        cJSON_ArrayForEach(ap, stations)
        {
            cJSON *jsid = cJSON_GetObjectItem(ap, "ssid");
            if (strcmp(cJSON_GetObjectItem(jsid, "value")->valuestring, sid) == 0)
            {
                cJSON *jpwd = cJSON_GetObjectItem(ap, "password");
                strcpy(pwd, cJSON_GetObjectItem(jpwd, "value")->valuestring);
                return;
            }
        }
    }

    sprintf(pwd, "8008");
    uint8_t sidLen = strlen(sid);
    for (int idx = sidLen - 1; idx >= 0; idx--)
    {
        pwd[4 + idx] = sid[idx];
    }
    pwd[4 + sidLen] = 0;
}

bool isSidManaged(const char *sid)
{
    AppConfig *cfg = GetAppConfig();
    cJSON *stations = cfg->GetJSONConfig("/stations");
    if ((stations != NULL) && (cJSON_IsArray(stations)))
    {
        cJSON *ap = NULL;
        cJSON_ArrayForEach(ap, stations)
        {
            cJSON *jsid = cJSON_GetObjectItem(ap, "ssid");
            if (strcmp(cJSON_GetObjectItem(jsid, "value")->valuestring, sid) == 0)
            {
                return true;
            }
        }
    }

    return ((startsWith(sid, "VIDEOETRON ") != 0) ||
            (startsWith(sid, "BELL ") != 0) ||
            (startsWith(sid, "Cogeco ") != 0) ||
            (startsWith(sid, "Linksys    ") != 0));
}

void generateSidConfig(wifi_config_t *wc, bool hasGps)
{
    ESP_LOGD(__FUNCTION__, "Generating SID %s", hasGps ? "with gps" : "without gps");
    if (hasGps)
    {
        bootloader_random_enable();
    }
    switch (esp_random() % 4)
    {
    case 0:
        sprintf((char *)wc->ap.ssid, "VIDEOETRON 2%04d", rand() % 999);
        break;
    case 1:
        sprintf((char *)wc->ap.ssid, "BELL 1%04d", rand() % 999);
        break;
    case 2:
        sprintf((char *)wc->ap.ssid, "Cogeco 3%04d.", rand() % 999);
        break;
    case 3:
        sprintf((char *)wc->ap.ssid, "Linksys    4%04d", rand() % 999);
        break;
    }
    if (hasGps)
    {
        bootloader_random_disable();
    }
    wc->ap.ssid_len = strlen((char *)wc->ap.ssid);
    wc->ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wc->ap.max_connection = 4;

    InferPassword((char *)wc->ap.ssid, (char *)wc->ap.password);
}

static void print_auth_mode(cJSON* json, int authmode)
{
    switch (authmode)
    {
    case WIFI_AUTH_OPEN:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_OPEN");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_WEP");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_WPA_PSK");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_WPA2_PSK");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_WPA_WPA2_PSKK");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_WPA2_ENTERPRISE");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    default:
        cJSON_AddStringToObject(json,"Authmode","WIFI_AUTH_UNKNOWN");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(cJSON* json, int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher)
    {
    case WIFI_CIPHER_TYPE_NONE:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_NONE");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_WEP40");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_WEP104");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_TKIP");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_CCMP");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_TKIP_CCMP");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        cJSON_AddStringToObject(json,"PairwiseCipher","WIFI_CIPHER_TYPE_UNKNOWN");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher)
    {
    case WIFI_CIPHER_TYPE_NONE:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_NONE");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_WEP40");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_WEP104");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_TKIP");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_CCMP");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_TKIP_CCMP");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        cJSON_AddStringToObject(json,"GroupCipher","WIFI_CIPHER_TYPE_UNKNOWN");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

bool wifiScan()
{
    if (xEventGroupGetBits(config->s_wifi_eg) & WIFI_SCAN_READY_BIT)
    {
        wifi_scan_config_t scan_config = {
            .ssid = 0,
            .bssid = 0,
            .channel = 0,
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time = {
                .active = {
                    .min = 100,
                    .max = 500},
                .passive = 500}};

        xEventGroupClearBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
        if (esp_wifi_scan_start(&scan_config, false) != ESP_OK)
        {
            ESP_LOGW(__FUNCTION__, "Cannot scan");
            return false;
        }
        xEventGroupSetBits(config->s_wifi_eg, WIFI_SCANING_BIT);
        ESP_LOGD(__FUNCTION__, "Scanning APs");
        return true;
    } else {
        ESP_LOGW(__FUNCTION__,"Not ready to scan the wifi");
        return false;
    }
}

void ProcessScannedAPs()
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    esp_err_t ret;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_LOGD(__FUNCTION__, "Getting Scanned AP");
    if ((ret = esp_wifi_scan_get_ap_records(&number, ap_info)) == ESP_OK)
    {
        AppConfig* wifi = AppConfig::GetAppStatus()->GetConfig("/wifi");
        cJSON* json = wifi->GetJSONConfig(NULL);
        cJSON* APs = wifi->GetJSONConfig("/APs");
        free(wifi);
        if (APs != NULL) {
            cJSON_DeleteItemFromObject(json,"APs");
        }
        APs = cJSON_AddArrayToObject(json,"APs");

        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGD(__FUNCTION__, "Total APs scanned = %u", ap_count);
        int lastWinner=-1;
        int8_t lastRssi=-124;
        uint8_t numWinners=0;
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
        {
            cJSON* AP = cJSON_CreateObject();
            cJSON_AddItemToArray(APs,AP);
            cJSON_AddStringToObject(AP,"SSID",(char*)ap_info[i].ssid);
            cJSON_AddNumberToObject(AP,"RSSI",ap_info[i].rssi);
            cJSON_AddNumberToObject(AP,"Channel",ap_info[i].primary);
            print_auth_mode(AP, ap_info[i].authmode);
            if (ap_info[i].authmode != WIFI_AUTH_WEP)
            {
                print_cipher_type(AP,ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
            }
            if (isSidManaged((const char *)ap_info[i].ssid))
            {
                if(ap_info[i].rssi > lastRssi) {
                    lastRssi = ap_info[1].rssi;
                    lastWinner = i;
                }
            }
        }

        if (lastWinner >= 0) {
            if (esp_wifi_scan_stop() != ESP_OK)
            {
                ESP_LOGW(__FUNCTION__, "Cannot stop scan");
            }

            strcpy((char *)wifi_config.sta.ssid, (char *)ap_info[lastWinner].ssid);
            InferPassword((char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
            memcpy(wifi_config.sta.bssid, ap_info[lastWinner].bssid, sizeof(uint8_t) * 6);
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            ESP_LOGD(__FUNCTION__, "Connecting to %s/", ap_info[lastWinner].ssid);
            ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &wifi_config));

            if (esp_wifi_connect() == ESP_OK)
            {
                ESP_LOGD(__FUNCTION__, "Configured in Station Mode %s/%s", wifi_config.sta.ssid, wifi_config.sta.password);
                xEventGroupClearBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
                xEventGroupSetBits(config->s_wifi_eg, WIFI_STA_CONFIGURED);
                return;
            }
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot get scan result: %s", esp_err_to_name(ret));
    }
    wifiScan();
}

Aper *GetAper(uint8_t *mac)
{
    uint8_t freeSlot = 254;
    uint8_t emptySlot = 254;
    for (int idx = 0; idx < MAX_NUM_CLIENTS; idx++)
    {
        if ((freeSlot == 254) && (clients[idx] == NULL))
        {
            freeSlot = idx;
            ESP_LOGV(__FUNCTION__, "Free slot: %d", freeSlot);
            break;
        }
        if ((emptySlot == 254) && (clients[idx] != NULL) && (!clients[idx]->isConnected()))
        {
            emptySlot = idx;
            ESP_LOGV(__FUNCTION__, "Empty slot: %d", emptySlot);
        }
        if (memcmp(mac, &clients[idx]->mac, 6) == 0)
        {
            return clients[idx];
        }
    }
    if (freeSlot != 254)
    {
        return clients[freeSlot] = new Aper(mac);
    }
    if (emptySlot != 254)
    {
        clients[emptySlot]->Update(mac);
        return clients[emptySlot];
    }
    return NULL;
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGD(__FUNCTION__, "Notification of a time synchronization event");
    settimeofday(tv, NULL);
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}

static void updateTime(void *param)
{
    ESP_LOGV(__FUNCTION__, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGV(__FUNCTION__, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    vTaskDelete(NULL);
}

void static ParseStateBits(AppConfig* state) {
    EventBits_t bits = xEventGroupGetBits(config->s_wifi_eg);
    state->SetBoolProperty("Connected",bits & WIFI_CONNECTED_BIT);
    state->SetBoolProperty("Scanning",bits & WIFI_SCANING_BIT);
    state->SetBoolProperty("Up",bits & WIFI_UP_BIT);
    state->SetBoolProperty("Down",bits & WIFI_DOWN_BIT);
    state->SetBoolProperty("Synching",!(bits & WIFI_CLIENT_DONE));
}

void static RefreshApMembers(AppConfig* state) {
    Aper **clients = GetClients();
    cJSON* json = state->GetJSONConfig(NULL);
    cJSON_DeleteItemFromObject(json,"clients");
    cJSON* jclients = cJSON_AddArrayToObject(json,"clients");

    int idx = 0;
    for (Aper *client = clients[idx++]; idx < MAX_NUM_CLIENTS; client = clients[idx++])
    {
        if (client)
        {
            cJSON_AddItemToArray(jclients, client->toJson());
        }
    }
    AppConfig::SignalStateChange();
}

void static network_event(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    Aper *client = NULL;
    AppConfig *cfg = GetAppConfig();
    AppConfig* state = AppConfig::GetAppStatus();
    AppConfig* stationStat = state->GetConfig("/wifi/station");
    AppConfig* apStat = state->GetConfig("/wifi/ap");
    if (base == IP_EVENT)
    {
        ESP_LOGV(__FUNCTION__, "ip event %d", event_id);
        ip_event_got_ip_t *event;
        system_event_info_t *evt;
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            event = (ip_event_got_ip_t *)event_data;
            if (event->ip_info.ip.addr == 0) {
                ESP_LOGW(__FUNCTION__,"Got empty IP, no go");
                break;
            }
            char ipaddr[16];
            sprintf(ipaddr,IPSTR, IP2STR(&ipInfo.ip));
            ESP_LOGI(__FUNCTION__, "got ip::%s", ipaddr);
            memcpy(&ipInfo, &event->ip_info, sizeof(ipInfo));
            xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            ParseStateBits(stationStat);
            initSPISDCard();

            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            // Is time set? If not, tm_year will be (1970 - 1900).
            if (timeinfo.tm_year < (2016 - 1900) && (timeHandle == NULL))
            {
                ESP_LOGD(__FUNCTION__, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
                xTaskCreate(updateTime, "updateTime", 4096, NULL, tskIDLE_PRIORITY, &timeHandle);
            }

            if (restHandle == NULL)
            {
                xTaskCreate(restSallyForth, "restSallyForth", 4096, getWifiConfig(), tskIDLE_PRIORITY, &restHandle);
            }
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(__FUNCTION__, "lost ip");
            xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            ParseStateBits(stationStat);
            break;
        case IP_EVENT_AP_STAIPASSIGNED:
            if (station != NULL)
            {
                evt = (system_event_info_t *)event_data;
                client = GetAper(station->mac);
                if (evt != NULL)
                {
                    if (client != NULL)
                    {
                        client->Update((ip4_addr_t *)&evt->ap_staipassigned.ip);
                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x count not be created, AID=%d",
                                 MAC2STR(station->mac), station->aid);
                    }
                    ESP_LOGD(__FUNCTION__, "Served ip::" IPSTR, IP2STR(&client->ip));
                }
                else
                {
                    ESP_LOGD(__FUNCTION__, "No ip, reconnected, re-trigger on all");
                    if ((client != NULL) && (((uint32_t)client->ip.addr) != 0))
                    {
                        //xTaskCreate(pullStation, "pullStation", 4096, &client->ip , tskIDLE_PRIORITY+5, NULL);
                    }
                    else
                    {
                        wifi_sta_list_t wifi_sta_list;
                        tcpip_adapter_sta_list_t tcp_sta_list;
                        esp_wifi_ap_get_sta_list(&wifi_sta_list);

                        if (tcpip_adapter_get_sta_list(&wifi_sta_list, &tcp_sta_list) == ESP_OK)
                        {
                            for (uint8_t i = 0; i < wifi_sta_list.num; i++)
                            {
                                ESP_LOGD(__FUNCTION__, "Mac : %d , STA IP : %d\n", tcp_sta_list.sta[i].mac[0], tcp_sta_list.sta[i].ip.addr);
                                ESP_LOGD(__FUNCTION__, "Num: %d , Mac : %d\n", wifi_sta_list.num, wifi_sta_list.sta[i].mac[0]);
                            }
                        }
                        else
                        {
                            ESP_LOGE(__FUNCTION__, "Cant get sta list");
                        }
                    }
                }
                RefreshApMembers(apStat);
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "No Station to pull on");
            }
            break;
        default:
            ESP_LOGD(__FUNCTION__, "Unknown IP Event %d", event_id);
            break;
        }
    }
    if (base == WIFI_EVENT)
    {
        ESP_LOGV(__FUNCTION__, "wifi event %d", event_id);
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGD(__FUNCTION__, "Wifi Station is up");
            if (config->s_wifi_eg)
            {
                xEventGroupSetBits(config->s_wifi_eg, WIFI_UP_BIT);
                if (xEventGroupGetBits(config->s_wifi_eg) & WIFI_SCAN_READY_BIT)
                {
                    wifiScan();
                }
                else if ((xEventGroupGetBits(config->s_wifi_eg) & WIFI_STA_CONFIGURED) && esp_wifi_connect() == ESP_OK)
                {
                    xEventGroupClearBits(config->s_wifi_eg, WIFI_SCANING_BIT);
                    ESP_LOGD(__FUNCTION__, "Configured in Station Mode");
                }
            }
            else
            {
                ESP_LOGW(__FUNCTION__, "Wifi does not have an event handler, weirdness is afoot");
            }
            ParseStateBits(stationStat);
            break;
        case WIFI_EVENT_STA_STOP:
            xEventGroupSetBits(config->s_wifi_eg, WIFI_DOWN_BIT);
            xEventGroupClearBits(config->s_wifi_eg, WIFI_UP_BIT);
            xEventGroupClearBits(config->s_wifi_eg, WIFI_SCANING_BIT);
            xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
            ParseStateBits(stationStat);
            break;
        case WIFI_EVENT_SCAN_DONE:
            xEventGroupClearBits(config->s_wifi_eg, WIFI_SCANING_BIT);
            ParseStateBits(stationStat);
            ProcessScannedAPs();
            break;
        case WIFI_EVENT_AP_START:
            xEventGroupSetBits(config->s_wifi_eg, WIFI_UP_BIT);
            xEventGroupClearBits(config->s_wifi_eg, WIFI_DOWN_BIT);
            ESP_LOGD(__FUNCTION__, "AP STARTED");
            xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo);
            char ipaddt[16];
            sprintf(ipaddt,IPSTR, IP2STR(&ipInfo.ip));
            ESP_LOGD(__FUNCTION__, "AP ip::%s", ipaddt);
            ParseStateBits(apStat);
            apStat->SetStringProperty("Ip", ipaddt);
            if (restHandle == NULL)
            {
                xTaskCreate(restSallyForth, "restSallyForth", 4096, getWifiConfig(), tskIDLE_PRIORITY, &restHandle);
            }
            break;
        case WIFI_EVENT_AP_STOP:
            xEventGroupClearBits(config->s_wifi_eg, WIFI_UP_BIT);
            xEventGroupSetBits(config->s_wifi_eg, WIFI_DOWN_BIT);
            ESP_LOGD(__FUNCTION__, "AP STOPPED");
            xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(eventGroup, HTTP_SERVING);
            ParseStateBits(apStat);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            memcpy((void *)station, (void *)event_data, sizeof(wifi_event_ap_staconnected_t));
            ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d",
                     MAC2STR(station->mac), station->aid);
            client = GetAper(station->mac);
            if (client != NULL)
            {
                client->Associate();
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x count not be created, AID=%d",
                         MAC2STR(station->mac), station->aid);
            }
            RefreshApMembers(apStat);
            initSPISDCard();
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Disconnected, AID=%d",
                     MAC2STR(station->mac), station->aid);
            client = GetAper(station->mac);
            if (client != NULL)
            {
                client->Dissassociate();
            }
            RefreshApMembers(apStat);
            deinitSPISDCard();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            wifi_ap_record_t ap_info;
            esp_wifi_sta_get_ap_info(&ap_info);
            ESP_LOGI(__FUNCTION__, "Wifi Connected to %s", ap_info.ssid);
            xEventGroupSetBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
            ParseStateBits(stationStat);
            //esp_netif_dhcpc_start(sta_netif);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (xEventGroupGetBits(config->s_wifi_eg) & WIFI_CONNECTED_BIT)
            {
                ESP_LOGD(__FUNCTION__, "Got disconnected from AP");
                deinitSPISDCard();
                xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
                ParseStateBits(stationStat);
                ESP_LOGI(__FUNCTION__, "Trying to reconnect");
            }
            if (!(xEventGroupGetBits(config->s_wifi_eg) & WIFI_CLIENT_DONE) || (cfg->IsAp() && cfg->IsSta()))
            {
                ESP_LOGV(__FUNCTION__, "Reconnecting");
                esp_wifi_connect();
            }
            break;
        default:
            ESP_LOGD(__FUNCTION__, "Unknown Wifi Event %d", event_id);
            break;
        }
    }
    free(stationStat);
    free(apStat);
    state->SignalStateChange();
}

void wifiStop(void *pvParameter)
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_deinit();
}

void wifiSallyForth(void *pvParameter)
{
    esp_pm_config_esp32_t pm_config;
    pm_config.max_freq_mhz = 240;
    pm_config.min_freq_mhz = 240;
    pm_config.light_sleep_enable = false;
    memset(&ipInfo, 0, sizeof(ipInfo));
    AppConfig *appcfg = AppConfig::GetAppConfig();

    esp_err_t ret;
    if ((ret = esp_pm_configure(&pm_config)) != ESP_OK)
    {
        printf("pm config error %s\n",
               ret == ESP_ERR_INVALID_ARG ? "ESP_ERR_INVALID_ARG" : "ESP_ERR_NOT_SUPPORTED");
    }

    if (config == NULL)
    {
        config = (the_wifi_config *)dmalloc(sizeof(the_wifi_config));
        memset(config, 0, sizeof(the_wifi_config));
        config->s_wifi_eg = xEventGroupCreate();
        config->disconnectWaitTime = 2000;
        config->poolWaitTime = 3000;
        xEventGroupSetBits(config->s_wifi_eg, WIFI_DOWN_BIT);
        xEventGroupClearBits(config->s_wifi_eg, WIFI_UP_BIT);
        xEventGroupClearBits(config->s_wifi_eg, WIFI_SCANING_BIT);
        xEventGroupClearBits(config->s_wifi_eg, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);

        ESP_LOGD(__FUNCTION__, "Wifi mode isap:%d issta:%d %s", appcfg->IsAp(), appcfg->IsSta(), appcfg->GetStringProperty("type"));
        config->wifi_mode = appcfg->IsAp() ? appcfg->IsSta() ? WIFI_MODE_APSTA : WIFI_MODE_AP : WIFI_MODE_STA;
        memset(clients, 0, sizeof(void *) * MAX_NUM_CLIENTS);
    }

    ESP_LOGD(__FUNCTION__, "Initializing netif");
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGD(__FUNCTION__, "Initialized netif");
    nvs_flash_init();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(config->wifi_mode));

    if (appcfg->IsAp())
    {
        if (ap_netif == NULL)
        {
            ap_netif = esp_netif_create_default_wifi_ap();
            ESP_LOGD(__FUNCTION__, "Created netif %li", (long int)ap_netif);
            ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
            generateSidConfig(&wifi_config, pvParameter != NULL);
            ESP_LOGD(__FUNCTION__, "Configured in AP Mode %s/%s", wifi_config.ap.ssid, wifi_config.ap.password);
            ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_AP, &wifi_config));
        }
    }

    if (appcfg->IsSta())
    {
        memset(&wifi_config.sta, 0, sizeof(wifi_sta_config_t));
        xEventGroupSetBits(config->s_wifi_eg, WIFI_SCAN_READY_BIT);
        if (sta_netif == NULL)
        {
            sta_netif = esp_netif_create_default_wifi_sta();
            ESP_LOGD(__FUNCTION__, "created station netif....");
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &wifi_config));
        }
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    EventHandlerDescriptor *handler = new EventHandlerDescriptor(IP_EVENT, "IP_EVENT");
    handler->SetEventName(IP_EVENT_AP_STAIPASSIGNED, "IP_EVENT_AP_STAIPASSIGNED");
    handler->SetEventName(IP_EVENT_STA_GOT_IP, "IP_EVENT_STA_GOT_IP");
    EventManager::RegisterEventHandler(handler);

    s_retry_num = 0;
    ESP_LOGD(__FUNCTION__, "esp_wifi_start finished.");

    vTaskDelete(NULL);
}

Aper::Aper(uint8_t station[6])
{
    Update(station);
    memset(&ip, 0, sizeof(esp_ip4_addr_t));
}

Aper::~Aper()
{
}

void Aper::Update(uint8_t station[6])
{
    assert(station);
    memcpy(mac, station, sizeof(uint8_t) * 6);
    connectionTime = 0;
    connectTime = 0;
    disconnectTime = 0;
}

void Aper::Update(ip4_addr_t *station)
{
    if (station == NULL)
    {
        memset(&ip, 0, sizeof(esp_ip4_addr_t));
    }
    else
    {
        memcpy(&ip, station, sizeof(esp_ip4_addr_t));
    }
}

void Aper::Dissassociate()
{
    time(&disconnectTime);
    connectionTime += (disconnectTime - connectTime);
    connectTime = 0;
}

void Aper::Associate()
{
    time(&connectTime);
    disconnectTime = 0;
}

bool Aper::isConnected()
{
    return ((connectTime != 0) && (disconnectTime == 0));
}

bool Aper::isSameDevice(Aper *other)
{
    assert(other);
    for (int idx = 0; idx < 6; idx++)
    {
        if (other->mac[idx] != mac[idx])
        {
            return false;
        }
    }
    return true;
}

cJSON *Aper::toJson()
{
    char *tbuf = (char *)dmalloc(255);
    cJSON *j = cJSON_CreateObject();
    sprintf(tbuf, "%02x:%02x:%02x:%02x:%02x:%02x",
            MAC2STR(mac));
    cJSON_AddStringToObject(j, "mac", tbuf);
    sprintf(tbuf, IPSTR, IP2STR(&ip));
    cJSON_AddStringToObject(j, "ip", tbuf);
    if (connectTime != 0)
    {
        cJSON_AddNumberToObject(j, "connectTime_sec", connectTime);
    }
    if (disconnectTime != 0)
    {
        cJSON_AddNumberToObject(j, "disconnectTime_sec", disconnectTime);
    }
    if (connectionTime)
    {
        cJSON_AddNumberToObject(j, "connectionTime_sec", connectionTime);
    }
    if (isConnected())
        cJSON_AddTrueToObject(j, "isConnected");
    else
        cJSON_AddFalseToObject(j, "isConnected");
    ldfree(tbuf);
    return j;
}