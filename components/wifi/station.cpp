
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
#include "rest.h"
#include "../../main/utils.h"
#include <esp_pm.h>
#include <lwip/sockets.h>
#include "eventmgr.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include <time.h>
#include <sys/time.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
const char* TheWifi::WIFI_BASE="TheWifi";

uint8_t s_retry_num = 0;
esp_netif_t *sta_netif = NULL;
esp_netif_t *ap_netif = NULL;
wifi_event_ap_staconnected_t *station = (wifi_event_ap_staconnected_t *)dmalloc(sizeof(wifi_event_ap_staconnected_t));
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
TaskHandle_t restHandle = NULL;
TaskHandle_t timeHandle = NULL;

static TheWifi *theInstance = NULL;

tcpip_adapter_ip_info_t *TheWifi::GetApIp()
{
    return &staIp;
}

tcpip_adapter_ip_info_t *TheWifi::GetStaIp()
{
    return &apIp;
}

TheWifi *TheWifi::GetInstance()
{
    return theInstance;
}

EventGroupHandle_t TheWifi::GetEventGroup()
{
    if (theInstance == NULL)
        return NULL;
    return theInstance->eventGroup;
}

TheWifi::~TheWifi()
{
    ESP_LOGI(__FUNCTION__, "Stop it");
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, wifiEvtHandler));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ipEvtHandler));
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_deinit();
    if (sta_netif)
        esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
    if (ap_netif)
        esp_wifi_clear_default_wifi_driver_and_handlers(ap_netif);
    vEventGroupDelete(eventGroup);
    EventManager::EventManager::UnRegisterEventHandler(handlerDescriptors);
    xEventGroupClearBits(s_app_eg, app_bits_t::WIFI_ON);
    xEventGroupSetBits(s_app_eg, app_bits_t::WIFI_OFF);
    theInstance = NULL;
    delete stationStat;
    delete apStat;
    ESP_LOGI(__FUNCTION__, "Stopd");
}

TheWifi::TheWifi(AppConfig *appcfg)
    : ManagedDevice(WIFI_BASE)
    , eventGroup(xEventGroupCreate())
    , cfg(appcfg)
    , s_app_eg(getAppEG())
{
    theInstance = this;
    stationStat = AppConfig::GetAppStatus()->GetConfig("/wifi/station");
    apStat = AppConfig::GetAppStatus()->GetConfig("/wifi/ap");

    if (handlerDescriptors == NULL)
        EventManager::RegisterEventHandler((handlerDescriptors = BuildHandlerDescriptors()));

    esp_pm_config_esp32_t pm_config;
    pm_config.max_freq_mhz = 240;
    pm_config.min_freq_mhz = 240;
    pm_config.light_sleep_enable = false;
    memset(&apIp, 0, sizeof(apIp));
    memset(&staIp, 0, sizeof(staIp));
    memset(clients, 0, sizeof(void *) * MAX_NUM_CLIENTS);
    memset(&wifi_config, 0, sizeof(wifi_config));

    esp_err_t ret;
    if ((ret = esp_pm_configure(&pm_config)) != ESP_OK)
    {
        ESP_LOGE(__FUNCTION__, "pm config error %s\n",
                 ret == ESP_ERR_INVALID_ARG ? "ESP_ERR_INVALID_ARG" : "ESP_ERR_NOT_SUPPORTED");
    }

    xEventGroupClearBits(eventGroup, 0xff);

    ESP_LOGI(__FUNCTION__, "Wifi mode isap:%d issta:%d %s", appcfg->IsAp(), appcfg->IsSta(), appcfg->GetStringProperty("wifitype"));

    ESP_LOGV(__FUNCTION__, "Initializing netif");
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGV(__FUNCTION__, "Initialized netif");

    if ((ret = esp_wifi_init(&cfg)) == ESP_OK)
    {
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &network_event, this, &wifiEvtHandler));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event, this, &ipEvtHandler));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(appcfg->IsAp() ? appcfg->IsSta() ? WIFI_MODE_APSTA : WIFI_MODE_AP : WIFI_MODE_STA));
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot start wifi:%s", esp_err_to_name(ret));
    }

    ESP_LOGI(__FUNCTION__, "***********%s", appcfg->GetStringProperty("wifitype"));
    if (appcfg->IsSta())
    {
        memset(&wifi_config.sta, 0, sizeof(wifi_sta_config_t));
        if (sta_netif == NULL)
        {
            sta_netif = esp_netif_create_default_wifi_sta();
            esp_netif_set_hostname(sta_netif, appcfg->GetStringProperty("devName"));
            ESP_LOGI(__FUNCTION__, "created station netif....");
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &wifi_config));
        }
        cJSON* methods = cJSON_AddArrayToObject(status,"commands");
        cJSON* flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","scanaps");
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"caption","Scan Access Points");
    }

    if (appcfg->IsAp())
    {
        if (ap_netif == NULL)
        {
            ap_netif = esp_netif_create_default_wifi_ap();
            esp_netif_set_hostname(ap_netif, appcfg->GetStringProperty("devName"));
            ESP_LOGI(__FUNCTION__, "Created netif %li", (long int)ap_netif);
            ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
            generateSidConfig(&wifi_config, false);
            //wifi_config.ap.channel = 5;
            ESP_LOGI(__FUNCTION__, "Configured in AP Mode %s/%s", wifi_config.ap.ssid, wifi_config.ap.password);
            ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_AP, &wifi_config));
        }
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupSetBits(s_app_eg, app_bits_t::WIFI_ON);
    xEventGroupClearBits(s_app_eg, app_bits_t::WIFI_OFF);

    ESP_LOGI(__FUNCTION__, "esp_wifi_start finished.");
}

EventHandlerDescriptor *TheWifi::BuildHandlerDescriptors()
{
    ESP_LOGV(__FUNCTION__, "TheWifi(%s) BuildHandlerDescriptors", eventBase);
    EventHandlerDescriptor *handler = new EventHandlerDescriptor(eventBase, (char *)eventBase);
    handler->AddEventDescriptor(IP_EVENT_STA_GOT_IP, "IP_EVENT_STA_GOT_IP");
    handler->AddEventDescriptor(IP_EVENT_STA_LOST_IP, "IP_EVENT_STA_LOST_IP");
    handler->AddEventDescriptor(IP_EVENT_AP_STAIPASSIGNED, "IP_EVENT_AP_STAIPASSIGNED");
    handler->AddEventDescriptor(IP_EVENT_GOT_IP6, "IP_EVENT_GOT_IP6");
    handler->AddEventDescriptor(IP_EVENT_ETH_GOT_IP, "IP_EVENT_ETH_GOT_IP");
    handler->AddEventDescriptor(IP_EVENT_PPP_GOT_IP, "IP_EVENT_PPP_GOT_IP");
    handler->AddEventDescriptor(IP_EVENT_PPP_LOST_IP, "IP_EVENT_PPP_LOST_IP");

    handler->AddEventDescriptor(20 + WIFI_EVENT_WIFI_READY, "WIFI_EVENT_WIFI_READY");
    handler->AddEventDescriptor(20 + WIFI_EVENT_SCAN_DONE, "WIFI_EVENT_SCAN_DONE");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_START, "WIFI_EVENT_STA_START");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_STOP, "WIFI_EVENT_STA_STOP");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_CONNECTED, "WIFI_EVENT_STA_CONNECTED");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_DISCONNECTED, "WIFI_EVENT_STA_DISCONNECTED");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_AUTHMODE_CHANGE, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_WPS_ER_SUCCESS, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_WPS_ER_FAILED, "WIFI_EVENT_STA_WPS_ER_FAILED");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_WPS_ER_TIMEOUT, "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_WPS_ER_PIN, "WIFI_EVENT_STA_WPS_ER_PIN");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP, "WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
    handler->AddEventDescriptor(20 + WIFI_EVENT_AP_START, "WIFI_EVENT_AP_START");
    handler->AddEventDescriptor(20 + WIFI_EVENT_AP_STOP, "WIFI_EVENT_AP_STOP");
    handler->AddEventDescriptor(20 + WIFI_EVENT_AP_STACONNECTED, "WIFI_EVENT_AP_STACONNECTED");
    handler->AddEventDescriptor(20 + WIFI_EVENT_AP_STADISCONNECTED, "WIFI_EVENT_AP_STADISCONNECTED");
    handler->AddEventDescriptor(20 + WIFI_EVENT_AP_PROBEREQRECVED, "WIFI_EVENT_AP_PROBEREQRECVED");
    handler->AddEventDescriptor(20 + WIFI_EVENT_FTM_REPORT, "WIFI_EVENT_FTM_REPORT");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_BSS_RSSI_LOW, "WIFI_EVENT_STA_BSS_RSSI_LOW");
    handler->AddEventDescriptor(20 + WIFI_EVENT_ACTION_TX_STATUS, "WIFI_EVENT_ACTION_TX_STATUS");
    handler->AddEventDescriptor(20 + WIFI_EVENT_ROC_DONE, "WIFI_EVENT_ROC_DONE");
    handler->AddEventDescriptor(20 + WIFI_EVENT_STA_BEACON_TIMEOUT, "WIFI_EVENT_STA_BEACON_TIMEOUT");
    handler->AddEventDescriptor(20 + WIFI_EVENT_MAX, "WIFI_EVENT_MAX");
    ESP_LOGV(__FUNCTION__, "TheWifi(%s) Done BuildHandlerDescriptors", eventBase);

    return handler;
}

void TheWifi::InferPassword(const char *sid, char *pwd)
{
    cJSON *stations = GetAppConfig()->GetJSONConfig("/stations");
    if ((stations != NULL) && (cJSON_IsArray(stations)))
    {
        cJSON *ap = NULL;
        cJSON_ArrayForEach(ap, stations)
        {
            cJSON *jsid = cJSON_GetObjectItem(ap, "ssid");

            ESP_LOGV(__FUNCTION__,"cJSON_HasObjectItem(jsid, value)%d && cJSON_IsString(cJSON_GetObjectItem(jsid, value))%d && strcmp(cJSON_GetObjectItem(jsid, value)->valuestring, sid)%d ", 
            cJSON_HasObjectItem(jsid, "value"),
             cJSON_IsString(cJSON_GetObjectItem(jsid, "value")),
             strcmp(cJSON_GetObjectItem(jsid, "value")->valuestring, sid));
            if (cJSON_HasObjectItem(jsid, "value") &&
                cJSON_IsString(cJSON_GetObjectItem(jsid, "value")) &&
                strcmp(cJSON_GetObjectItem(jsid, "value")->valuestring, sid) == 0)
            {
                cJSON *jpwd = cJSON_GetObjectItem(ap, "password");
                cJSON* jipwd = cJSON_GetObjectItem(jpwd, "value");
                if (jipwd && cJSON_IsString(jipwd)){
                    strcpy(pwd, jipwd->valuestring);
                } else {
                    char ctmp[64];
                    sprintf(pwd, "%d",jipwd->valueint);
                }
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

bool TheWifi::isSidPuller(const char *sid, bool isTracker)
{
    return isTracker &&
           ((startsWith(sid, "VIDEOETRON ") != 0) ||
            (startsWith(sid, "BELL ") != 0) ||
            (startsWith(sid, "Cogeco ") != 0) ||
            (startsWith(sid, "Linksys    ") != 0));
}

bool TheWifi::isSidManaged(const char *sid, bool isTracker)
{
    if ((sid == NULL) || (strlen(sid) == 0))
    {
        return false;
    }
    AppConfig *cfg = GetAppConfig();
    cJSON *stations = cfg->GetJSONConfig("/stations");
    if ((stations != NULL) && (cJSON_IsArray(stations)))
    {
        ESP_LOGV(__FUNCTION__,"We have stations");
        cJSON *ap = NULL;
        cJSON_ArrayForEach(ap, stations)
        {
            cJSON *j1 = cJSON_GetObjectItem(ap, "ssid");
            if (!j1)
            {
                ESP_LOGV(__FUNCTION__,"Stations configured without a ssid");
                continue;
            }
            cJSON *j2 = cJSON_GetObjectItem(j1, "value");
            if (!j2 || !j2->valuestring || !strlen(j2->valuestring))
            {
                ESP_LOGV(__FUNCTION__,"Stations configured without a ssid value");
                continue;
            }
            ESP_LOGV(__FUNCTION__,"%s == %s",j2->valuestring,sid);
            return (strcmp(j2->valuestring, sid) == 0);
        }
    } else {
        ESP_LOGV(__FUNCTION__,"We have no stations");
    }
    return false;
}

void TheWifi::generateSidConfig(wifi_config_t *wc, bool hasGps)
{
    ESP_LOGI(__FUNCTION__, "Generating SID %s", hasGps ? "with gps" : "without gps");
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

static void print_auth_mode(cJSON *json, int authmode)
{
    switch (authmode)
    {
    case WIFI_AUTH_OPEN:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_OPEN");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_WEP");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_WPA_PSK");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_WPA2_PSK");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_WPA_WPA2_PSKK");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_WPA2_ENTERPRISE");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    default:
        cJSON_AddStringToObject(json, "Authmode", "WIFI_AUTH_UNKNOWN");
        ESP_LOGV(__FUNCTION__, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(cJSON *json, int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher)
    {
    case WIFI_CIPHER_TYPE_NONE:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_NONE");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_WEP40");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_WEP104");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_TKIP");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_CCMP");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_TKIP_CCMP");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        cJSON_AddStringToObject(json, "PairwiseCipher", "WIFI_CIPHER_TYPE_UNKNOWN");
        ESP_LOGV(__FUNCTION__, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher)
    {
    case WIFI_CIPHER_TYPE_NONE:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_NONE");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_WEP40");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_WEP104");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_TKIP");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_CCMP");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_TKIP_CCMP");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        cJSON_AddStringToObject(json, "GroupCipher", "WIFI_CIPHER_TYPE_UNKNOWN");
        ESP_LOGV(__FUNCTION__, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

bool TheWifi::wifiScan()
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

    esp_err_t ret = ESP_OK;
    if ((ret = esp_wifi_scan_start(&scan_config, false)) != ESP_OK)
    {
        ESP_LOGW(__FUNCTION__, "%s - Cannot scan", esp_err_to_name(ret));
        return false;
    }
    xEventGroupSetBits(eventGroup, WIFI_SCANING_BIT);
    ESP_LOGV(__FUNCTION__, "Scanning APs");
    return true;
}

void TheWifi::ProcessScannedAPs()
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    esp_err_t ret;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_LOGV(__FUNCTION__, "Getting Scanned AP");
    if ((ret = esp_wifi_scan_get_ap_records(&number, ap_info)) == ESP_OK)
    {
        AppConfig *appStatus = AppConfig::GetAppStatus();
        AppConfig *appCfg = AppConfig::GetAppConfig();
        cJSON *wifiCfg = appStatus->GetJSONConfig("/wifi");
        cJSON *APs = appStatus->GetJSONConfig("/wifi/APs");
        if (APs != NULL)
        {
            cJSON_DeleteItemFromObject(wifiCfg, "APs");
        }
        APs = cJSON_AddArrayToObject(wifiCfg, "APs");

        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        int lastWinner = -1;
        int8_t lastRssi = -124;
        bool isTracker = appCfg->HasProperty("clienttype") && strcasecmp(appCfg->GetStringProperty("clienttype"), "tracker") == 0;
        ESP_LOGV(__FUNCTION__, "Total APs scanned = %u isTracker:%d", ap_count, isTracker);
        bool hasPuller = false;
        for (int i = 0; (i < number); i++)
        {
            cJSON *AP = cJSON_CreateObject();
            cJSON_AddItemToArray(APs, AP);
            cJSON_AddStringToObject(AP, "SSID", (char *)ap_info[i].ssid);
            cJSON_AddNumberToObject(AP, "RSSI", ap_info[i].rssi);
            cJSON_AddNumberToObject(AP, "Channel", ap_info[i].primary);
            ESP_LOGV(__FUNCTION__, "%d %s-%d", i, (char *)ap_info[i].ssid, ap_info[i].primary);
            print_auth_mode(AP, ap_info[i].authmode);
            if (ap_info[i].authmode != WIFI_AUTH_WEP)
            {
                print_cipher_type(AP, ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
            }
            if (!hasPuller && isSidManaged((const char *)ap_info[i].ssid, isTracker))
            {
                if (ap_info[i].rssi > lastRssi)
                {
                    lastRssi = ap_info[1].rssi;
                    lastWinner = i;
                }
            }
            if (isTracker && isSidPuller((const char *)ap_info[i].ssid, true))
            {
                ESP_LOGV(__FUNCTION__,"ssid %s is managed", (char *)ap_info[i].ssid);
                hasPuller = true;
                if (ap_info[i].rssi > lastRssi)
                {
                    lastRssi = ap_info[1].rssi;
                    lastWinner = i;
                }
            }
        }
        AppConfig::SignalStateChange(state_change_t::WIFI);

        if (lastWinner >= 0)
        {
            if (esp_wifi_scan_stop() != ESP_OK)
            {
                ESP_LOGW(__FUNCTION__, "Cannot stop scan");
            }

            strcpy((char *)wifi_config.sta.ssid, (char *)ap_info[lastWinner].ssid);
            InferPassword((char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
            memcpy(wifi_config.sta.bssid, ap_info[lastWinner].bssid, sizeof(uint8_t) * 6);
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            ESP_LOGI(__FUNCTION__, "Connecting to %s/%s", (char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
            ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &wifi_config));

            if ((ret = esp_wifi_connect()) == ESP_OK)
            {
                ESP_LOGI(__FUNCTION__, "Configured in Station Mode %s", wifi_config.sta.ssid);
                xEventGroupSetBits(eventGroup, WIFI_STA_CONFIGURED);
                return;
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Cannot connect to wifi:%s", esp_err_to_name(ret));
            }
        } else if (isTracker) {
            wifiScan();
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot get scan result: %s", esp_err_to_name(ret));
    }
}

Aper *TheWifi::GetAperByIp(esp_ip4_addr_t ip) {
    for (int idx = 0; idx < MAX_NUM_CLIENTS; idx++)
    {
        if (clients[idx] && (ip.addr == clients[idx]->ip.addr))
        {
            return clients[idx];
        }
    }
    return NULL;
}

Aper *TheWifi::GetAperByMac(uint8_t *mac)
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
    ESP_LOGI(__FUNCTION__, "Notification of a time synchronization event");
    settimeofday(tv, NULL);
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}

static void updateTime(void *param)
{
    if (sntp_enabled()) {
        ESP_LOGV(__FUNCTION__,"SNTP Already running");
        return;
    }
    ESP_LOGV(__FUNCTION__, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();

    time_t now = 0;
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    int retry = 0;
    const int retry_count = 10;

    for (xEventGroupWaitBits(TheWifi::GetEventGroup(), WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, 2000 / portTICK_PERIOD_MS);
         (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) && (retry++ < retry_count);
         vTaskDelay(2000 / portTICK_PERIOD_MS))
    {
        ESP_LOGV(__FUNCTION__, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }

    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void TheWifi::ParseStateBits(AppConfig *state)
{
    if (xEventGroupGetBits(s_app_eg) & app_bits_t::WIFI_ON)
    {
        EventBits_t bits = xEventGroupGetBits(eventGroup);
        state->SetBoolProperty("Connected", bits & WIFI_CONNECTED_BIT);
        state->SetBoolProperty("Scanning", bits & WIFI_SCANING_BIT);
        state->SetBoolProperty("Up", bits & WIFI_STA_UP_BIT);
        state->SetBoolProperty("Station", bits & WIFI_STA_CONFIGURED);
    }
}

int TheWifi::RefreshApMembers(AppConfig *state)
{
    Aper **clients = GetClients();
    cJSON *json = state->GetJSONConfig(NULL);
    cJSON_DeleteItemFromObject(json, "clients");
    cJSON *jclients = cJSON_AddArrayToObject(json, "clients");

    int idx = 0;
    int ret = 0;
    for (Aper *client = clients[idx++]; idx < MAX_NUM_CLIENTS; client = clients[idx++])
    {
        if (client)
        {
            if (client->isConnected())
            {
                ret++;
            }
            cJSON_AddItemToArray(jclients, client->toJson());
        }
    }
    AppConfig::SignalStateChange(state_change_t::WIFI);
    return ret;
}

void TheWifi::network_event(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGI(__FUNCTION__, "Base %s event %d", base, event_id);
    TheWifi *theWifi = (TheWifi *)theInstance;
    if (theInstance == NULL)
    {
        ESP_LOGW(__FUNCTION__, "Not processing as wifi is off");
        return;
    }
    Aper *client = NULL;
    EventGroupHandle_t s_app_eg = theWifi->s_app_eg;
    EventGroupHandle_t evtGrp = TheWifi::GetEventGroup();
    if (base == IP_EVENT)
    {
        ESP_ERROR_CHECK(theWifi->PostEvent(event_data, event_data != NULL ? sizeof(void *) : 0, event_id));
        ESP_LOGV(__FUNCTION__, "ip event %d", event_id);
        ip_event_got_ip_t *event;
        system_event_info_t *evt;
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            event = (ip_event_got_ip_t *)event_data;
            if (event->ip_info.ip.addr == 0)
            {
                ESP_LOGW(__FUNCTION__, "Got empty IP, no go");
                break;
            }
            char ipaddr[16];
            sprintf(ipaddr, IPSTR, IP2STR(&event->ip_info.ip));
            theWifi->stationStat->SetStringProperty("Ip", ipaddr);
            ESP_LOGI(__FUNCTION__, "got ip::%s", ipaddr);
            char gipaddr[16];
            sprintf(gipaddr, IPSTR, IP2STR(&event->ip_info.gw));
            if ((gipaddr != NULL) && (strlen(gipaddr) > 0))
            {
                theWifi->stationStat->SetStringProperty("Gw", gipaddr);
                ESP_LOGI(__FUNCTION__, "got gw::%s", gipaddr);
            }
            else
            {
                ipaddr[strlen(ipaddr) - 2] = '1';
                ESP_LOGW(__FUNCTION__, "Did not get a gateway, faking it to %s", ipaddr);
                theWifi->stationStat->SetStringProperty("Gw", ipaddr);
            }
            memcpy(&theWifi->staIp, &event->ip_info, sizeof(theWifi->staIp));
            xEventGroupSetBits(evtGrp, WIFI_CONNECTED_BIT);
            //xEventGroupSetBits(s_app_eg, REST);
            xEventGroupClearBits(evtGrp, WIFI_DISCONNECTED_BIT);
            theWifi->ParseStateBits(theWifi->stationStat);

            if (!theWifi->isSidPuller((const char *)theWifi->wifi_config.sta.ssid, true))
                CreateBackgroundTask(updateTime, "updateTime", 4096, NULL, tskIDLE_PRIORITY, &timeHandle);
            xEventGroupSetBits(s_app_eg, app_bits_t::REST);

            //CreateBackgroundTask(restSallyForth, "restSallyForth", 8196, evtGrp, tskIDLE_PRIORITY, NULL);

            //restSallyForth(evtGrp);
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(__FUNCTION__, "lost ip");

            theWifi->wifiScan();
            theWifi->ParseStateBits(theWifi->stationStat);
            break;
        case IP_EVENT_AP_STAIPASSIGNED:
            if (event_data) {
                evt = (system_event_info_t *)event_data;
                wifi_sta_list_t wifi_sta_list;
                tcpip_adapter_sta_list_t tcp_sta_list;
                esp_wifi_ap_get_sta_list(&wifi_sta_list);

                if (tcpip_adapter_get_sta_list(&wifi_sta_list, &tcp_sta_list) == ESP_OK)
                {
                    for (uint8_t i = 0; i < wifi_sta_list.num; i++)
                    {
                        if ((evt->ap_staipassigned.ip.addr != 0) && 
                            (evt->ap_staipassigned.ip.addr == tcp_sta_list.sta[i].ip.addr)) {
                            client = theWifi->GetAperByMac(tcp_sta_list.sta[i].mac);
                            if (client && (theWifi->GetAperByIp(tcp_sta_list.sta[i].ip) != client)) {
                                Aper* prevOwner = theWifi->GetAperByIp(tcp_sta_list.sta[i].ip);
                                if (prevOwner) {
                                    ESP_LOGI(__FUNCTION__,"Client released ip %d.%d.%d.%d",IP2STR(&tcp_sta_list.sta[i].ip));
                                    prevOwner->ip.addr=0;
                                }
                                ESP_LOGI(__FUNCTION__,"Client reconnected with ip %d.%d.%d.%d",IP2STR(&tcp_sta_list.sta[i].ip));
                                client->Update((ip4_addr_t *)&tcp_sta_list.sta[i].ip);
                                theWifi->RefreshApMembers(theWifi->apStat);
                            } else {
                                if (client) {
                                    ESP_LOGV(__FUNCTION__,"Client reconnected with ip %d.%d.%d.%d",IP2STR(&tcp_sta_list.sta[i].ip));
                                }
                            }
                            break;
                        }
                    }
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Cant get sta list");
                }
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "No event data");
            }
            break;
        default:
            ESP_LOGI(__FUNCTION__, "Unknown IP Event %d", event_id);
            break;
        }
    }
    if (base == WIFI_EVENT)
    {
        ESP_LOGI(__FUNCTION__, "wifi event %d", event_id + 20);
        ESP_ERROR_CHECK(theWifi->PostEvent(event_data, event_data != NULL ? sizeof(void *) : 0, event_id + 20));
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(__FUNCTION__, "Wifi Station is up");
            xEventGroupSetBits(evtGrp, WIFI_STA_UP_BIT);
            theWifi->wifiScan();
            theWifi->ParseStateBits(theWifi->stationStat);
            break;
        case WIFI_EVENT_STA_STOP:
            xEventGroupClearBits(evtGrp, WIFI_STA_UP_BIT);
            theWifi->ParseStateBits(theWifi->stationStat);
            break;
        case WIFI_EVENT_SCAN_DONE:
            xEventGroupClearBits(evtGrp, WIFI_SCANING_BIT);
            theWifi->ParseStateBits(theWifi->stationStat);
            theWifi->ProcessScannedAPs();
            break;
        case WIFI_EVENT_AP_START:
            ESP_LOGI(__FUNCTION__, "AP STARTED");
            xEventGroupSetBits(evtGrp, WIFI_AP_UP_BIT);
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &theWifi->staIp);
            char ipaddt[16];
            sprintf(ipaddt, IPSTR, IP2STR(&theWifi->staIp.ip));
            ESP_LOGI(__FUNCTION__, "AP ip::%s", ipaddt);
            //xEventGroupSetBits(s_app_eg, REST);
            theWifi->ParseStateBits(theWifi->apStat);
            theWifi->apStat->SetStringProperty("Ip", ipaddt);
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(__FUNCTION__, "AP STOPPED");
            xEventGroupClearBits(evtGrp, WIFI_AP_UP_BIT);
            theWifi->ParseStateBits(theWifi->apStat);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            xEventGroupSetBits(evtGrp, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(evtGrp, WIFI_DISCONNECTED_BIT);
            memcpy((void *)station, (void *)event_data, sizeof(wifi_event_ap_staconnected_t));
            client = theWifi->GetAperByMac(station->mac);
            if (client != NULL)
            {
                //CreateBackgroundTask(restSallyForth, "restSallyForth", 8196, evtGrp, tskIDLE_PRIORITY, NULL);
                xEventGroupSetBits(s_app_eg, app_bits_t::REST);
                client->Associate();
                ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d",
                         MAC2STR(station->mac), station->aid);
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x count not be created, AID=%d",
                         MAC2STR(station->mac), station->aid);
            }
            theWifi->RefreshApMembers(theWifi->apStat);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            client = theWifi->GetAperByMac(station->mac);
            if (client != NULL)
            {
                client->Dissassociate();
                ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Disconnected, AID=%d",
                         MAC2STR(station->mac), station->aid);
            }
            else
            {
                ESP_LOGW(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Disconnected but not registerred, AID=%d",
                         MAC2STR(station->mac), station->aid);
            }

            if ((theWifi->RefreshApMembers(theWifi->apStat) == 0) && !(xEventGroupGetBits(evtGrp) & WIFI_AP_UP_BIT))
            {
                xEventGroupSetBits(evtGrp, REST_OFF);
            }

            break;
        case WIFI_EVENT_STA_CONNECTED:
            xEventGroupSetBits(evtGrp, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(evtGrp, WIFI_DISCONNECTED_BIT);
            wifi_ap_record_t ap_info;
            esp_wifi_sta_get_ap_info(&ap_info);
            ESP_LOGI(__FUNCTION__, "Wifi Connected to %s", ap_info.ssid);
            theWifi->ParseStateBits(theWifi->stationStat);
            //esp_netif_dhcpc_start(sta_netif);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (xEventGroupGetBits(evtGrp) & WIFI_CONNECTED_BIT)
            {
                ESP_LOGI(__FUNCTION__, "Got disconnected from AP");
                theWifi->ParseStateBits(theWifi->stationStat);
            }
            if (!(xEventGroupGetBits(s_app_eg) & app_bits_t::TRIPS_SYNCING))
            {
                ESP_LOGI(__FUNCTION__, "Trying to reconnect");
                theWifi->wifiScan();
            }
            if (!(xEventGroupGetBits(evtGrp) & WIFI_AP_UP_BIT))
            {
                xEventGroupSetBits(evtGrp, WIFI_CONNECTED_BIT);
                xEventGroupClearBits(evtGrp, WIFI_DISCONNECTED_BIT);
            }
            break;
        default:
            ESP_LOGI(__FUNCTION__, "Unknown Wifi Event %d", event_id);
            break;
        }
    }
    AppConfig::SignalStateChange(state_change_t::WIFI);
}

void wifiSallyForth(void *pvParameter)
{
    if (TheWifi::GetInstance() == NULL)
    {
        theInstance = new TheWifi(AppConfig::GetAppConfig());
    }
}

Aper **TheWifi::GetClients()
{
    return clients;
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
    ESP_LOGI(__FUNCTION__, "New station %02x:%02x:%02x:%02x:%02x:%02x", MAC2STR(mac));
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
        ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Lost it's IP", MAC2STR(mac));
    }
    else
    {
        memcpy(&ip, station, sizeof(esp_ip4_addr_t));
        ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Connected with IP %d.%d.%d.%d", MAC2STR(mac), IP2STR(station));
    }
}

void Aper::Dissassociate()
{
    time(&disconnectTime);
    connectionTime += (disconnectTime - connectTime);
    connectTime = 0;
    ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Disconnected", MAC2STR(station->mac));
}

void Aper::Associate()
{
    time(&connectTime);
    disconnectTime = 0;
    ESP_LOGI(__FUNCTION__, "station %02x:%02x:%02x:%02x:%02x:%02x Connected", MAC2STR(station->mac));
}

bool Aper::isConnected()
{
    return disconnectTime == 0;
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