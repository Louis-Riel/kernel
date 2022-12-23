#include "bt.h"
#include "cJSON.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "../../main/utils.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"
#include <cstring>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define BD_ADDR_LEN     (6)                     /* Device address length */
#define ENDIAN_CHANGE_U16(x) ((((x) &0xFF00) >> 8) + (((x) &0xFF) << 8))

const char* Bt::BT_BASE="Bt";
static Bt* instance = NULL;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

Bt::Bt()
    :ManagedDevice(BT_BASE,BT_BASE,NULL,&ProcessCommand)
{
    ESP_LOGV(__FUNCTION__,"Bt start");
    
    if (instance == NULL) {
        instance = this;
    }

    AppConfig* apin = new AppConfig(status,AppConfig::GetAppStatus());
    apin->SetStringProperty("name",name);
    apin->SetStringProperty("state","idle");
    btStatus = apin->GetPropertyHolder("state");
    devices = cJSON_AddArrayToObject(status, "devices");

    cJSON* methods = cJSON_AddArrayToObject(status,"commands");
    cJSON* flush = cJSON_CreateObject();
    cJSON_AddItemToArray(methods,flush);
    cJSON_AddStringToObject(flush,"command","SetBtState");
    cJSON_AddStringToObject(flush,"className",BT_BASE);
    cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
    cJSON_AddStringToObject(flush,"param1","ON");
    cJSON_AddStringToObject(flush,"caption","On");
    flush = cJSON_CreateObject();
    cJSON_AddItemToArray(methods,flush);
    cJSON_AddStringToObject(flush,"command","SetBtState");
    cJSON_AddStringToObject(flush,"className",BT_BASE);
    cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
    cJSON_AddStringToObject(flush,"param1","OFF");
    cJSON_AddStringToObject(flush,"caption","Off");
    flush = cJSON_CreateObject();
    cJSON_AddItemToArray(methods,flush);
    cJSON_AddStringToObject(flush,"command","ScanForDevices");
    cJSON_AddStringToObject(flush,"caption","Scan Bluetooth");
    cJSON_AddStringToObject(flush,"className",BT_BASE);
    cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
    delete apin;
    m_dev_info = (app_gap_cb_t*)dmalloc(sizeof(app_gap_cb_t));
    memset(m_dev_info, 0, sizeof(app_gap_cb_t));
}

EventHandlerDescriptor* Bt::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"Bt BuildHandlerDescriptors");
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(eventIds::OFF,"OFF",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::ON,"ON",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::SCANED,"SCANNED",event_data_type_tp::JSON);
  return handler;
}

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

static char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size)
{
    if (uuid == NULL || str == NULL) {
        return NULL;
    }

    if (uuid->len == 2 && size >= 5) {
        sprintf(str, "%04x", uuid->uuid.uuid16);
    } else if (uuid->len == 4 && size >= 9) {
        sprintf(str, "%08x", uuid->uuid.uuid32);
    } else if (uuid->len == 16 && size >= 37) {
        uint8_t *p = uuid->uuid.uuid128;
        sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8],
                p[7], p[6], p[5], p[4], p[3], p[2], p[1], p[0]);
    } else {
        return NULL;
    }

    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

void Bt::update_device_info(esp_bt_gap_cb_param_t *param)
{
    char bda_str[18];
    uint32_t cod = 0;
    int32_t rssi = -129; /* invalid value */
    esp_bt_gap_dev_prop_t *p;
    uint8_t len=0;
    struct tm timeinfo;
    time_t now = 0;
    time(&now);
    localtime_r(&now, &timeinfo);

    bda2str(param->disc_res.bda, bda_str, 18);
    cJSON* dev = instance->GetDevice(bda_str);
    if (!dev) {
        ESP_LOGD(__FUNCTION__, "Device found: %s", bda_str);
        dev=instance->AddDevice(bda_str);
        strftime(cJSON_GetObjectItem(dev,"firstSeen")->valuestring, 30, "%c", &timeinfo);
    }
    strftime(cJSON_GetObjectItem(dev,"lastSeen")->valuestring, 30, "%c", &timeinfo);
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            cJSON_SetNumberValue((cJSON_HasObjectItem(dev,"cod") ? cJSON_GetObjectItem(dev,"cod") : cJSON_AddNumberToObject(dev,"cod",cod)),cod);
            ESP_LOGD(__FUNCTION__, "--Class of Device: 0x%x", cod);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            cJSON_SetNumberValue((cJSON_HasObjectItem(dev,"rssi") ? cJSON_GetObjectItem(dev,"rssi") : cJSON_AddNumberToObject(dev,"rssi",rssi)),rssi);
            ESP_LOGD(__FUNCTION__, "--RSSI: %d", rssi);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
            len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN : (uint8_t)p->len;
            if (len) {
                ESP_LOGD(__FUNCTION__, "name: %s", (char *)(p->val));
                cJSON_SetValuestring((cJSON_HasObjectItem(dev,"rssi") ? cJSON_GetObjectItem(dev,"rssi") : cJSON_AddNumberToObject(dev,"rssi",rssi)),(char *)p->val);
            }
        default:
            break;
        }
        AppConfig::SignalStateChange(state_change_t::MAIN);
    }

    /* search for device with MAJOR service class as "rendering" in COD */
    app_gap_cb_t *p_dev = instance->m_dev_info;
    if (p_dev->dev_found && 0 != memcmp(param->disc_res.bda, p_dev->bda, ESP_BD_ADDR_LEN)) {
        return;
    }

    if (!esp_bt_gap_is_valid_cod(cod) ||
	    (!(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PHONE) &&
             !(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_AV))) {
        return;
    }

    memcpy(p_dev->bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
    p_dev->dev_found = true;
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            p_dev->cod = *(uint32_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            p_dev->rssi = *(int8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME: {
            uint8_t len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN :
                          (uint8_t)p->len;
            memcpy(p_dev->bdname, (uint8_t *)(p->val), len);
            p_dev->bdname[len] = '\0';
            p_dev->bdname_len = len;
            break;
        }
        case ESP_BT_GAP_DEV_PROP_EIR: {
            memcpy(p_dev->eir, (uint8_t *)(p->val), p->len);
            p_dev->eir_len = p->len;
            break;
        }
        default:
            break;
        }
    }

    if (p_dev->eir && p_dev->bdname_len == 0) {
        if (get_name_from_eir(p_dev->eir, p_dev->bdname, &p_dev->bdname_len)) {
            cJSON_SetValuestring((cJSON_HasObjectItem(dev,"name") ? cJSON_GetObjectItem(dev,"name") :  cJSON_AddStringToObject(dev,"name",(char*)p_dev->bdname)),(char*)p_dev->bdname);
            ESP_LOGD(__FUNCTION__, "Found a target device, address %s, name %s", bda_str, p_dev->bdname);
        } else {
            ESP_LOGD(__FUNCTION__, "Found a target device, address %s, can't get name", bda_str);
        }
        p_dev->state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
    }
}

void Bt::esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    char bda_str[18];
    cJSON* dev = NULL;
    uint16_t major = 0;
    uint16_t minor = 0;
    struct tm timeinfo;
    time_t now = 0;
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        ESP_LOGI(__FUNCTION__, "Scan parm set");
        esp_ble_gap_start_scanning(0);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(__FUNCTION__, "Scan start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(__FUNCTION__, "Adv start failed");
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            /* Search for BLE iBeacon Packet */
            if (scan_result->scan_rst.ble_adv != NULL){
                esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
                bda2str(scan_result->scan_rst.bda, bda_str, 18);
                dev = instance->GetDevice(bda_str);

                major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
                minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
                time(&now);
                localtime_r(&now, &timeinfo);

                if (!dev) {
                    ESP_LOGD(__FUNCTION__, "Device found: %s", bda_str);
                    dev=instance->AddDevice(bda_str);
                    cJSON_AddNumberToObject(dev,"major",major);
                    cJSON_AddNumberToObject(dev,"minor",minor);
                    cJSON_AddNumberToObject(dev,"power",ibeacon_data->ibeacon_vendor.measured_power);
                    cJSON_AddNumberToObject(dev,"rssi",scan_result->scan_rst.rssi);
                    strftime(cJSON_GetObjectItem(dev,"firstSeen")->valuestring, 30, "%c", &timeinfo);
                }
                cJSON_SetNumberValue(cJSON_GetObjectItem(dev, "power"), ibeacon_data->ibeacon_vendor.measured_power);
                cJSON_SetNumberValue(cJSON_GetObjectItem(dev, "rssi"), scan_result->scan_rst.rssi);
                strftime(cJSON_GetObjectItem(dev,"lastSeen")->valuestring, 30, "%c", &timeinfo);

                //esp_log_buffer_hex(__FUNCTION__, scan_result->scan_rst.bda, 6);
                ESP_LOGV(__FUNCTION__, "Searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                ESP_LOGV(__FUNCTION__, "Searched Device Name Len %d", adv_name_len);
                //esp_log_buffer_char(__FUNCTION__, adv_name, adv_name_len);
                ESP_LOGV(__FUNCTION__, "\n");
                if (adv_name_len) {
                    cJSON_SetValuestring((cJSON_HasObjectItem(dev,"name") ? cJSON_GetObjectItem(dev,"name") :  cJSON_AddStringToObject(dev,"name",(char*)adv_name)),(char*)adv_name);
                }
                
                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &adv_name_len);
                ESP_LOGV(__FUNCTION__, "Searched Device short Name Len %d", adv_name_len);
                //esp_log_buffer_char(__FUNCTION__, adv_name, adv_name_len);
                ESP_LOGV(__FUNCTION__, "\n");
                if (adv_name_len) {
                    cJSON_SetValuestring((cJSON_HasObjectItem(dev,"shortname") ? cJSON_GetObjectItem(dev,"shortname") :  cJSON_AddStringToObject(dev,"shortname",(char*)adv_name)),(char*)adv_name);
                }

                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_DEV_CLASS, &adv_name_len);
                ESP_LOGV(__FUNCTION__, "Searched class Len %d", adv_name_len);
                //esp_log_buffer_char(__FUNCTION__, adv_name, adv_name_len);
                ESP_LOGV(__FUNCTION__, "\n");
                if (adv_name_len) {
                    cJSON_SetValuestring((cJSON_HasObjectItem(dev,"class") ? cJSON_GetObjectItem(dev,"class") :  cJSON_AddStringToObject(dev,"class",(char*)adv_name)),(char*)adv_name);
                }

                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_APPEARANCE, &adv_name_len);
                ESP_LOGV(__FUNCTION__, "Searched Device apprearance Len %d", adv_name_len);
                //esp_log_buffer_char(__FUNCTION__, adv_name, adv_name_len);
                ESP_LOGV(__FUNCTION__, "\n");
                if (adv_name_len) {
                    cJSON_SetValuestring((cJSON_HasObjectItem(dev,"apprearance") ? cJSON_GetObjectItem(dev,"apprearance") :  cJSON_AddStringToObject(dev,"apprearance",(char*)adv_name)),(char*)adv_name);
                }
            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(__FUNCTION__, "Scan stop failed");
        }
        else {
            ESP_LOGI(__FUNCTION__, "Stop scan successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(__FUNCTION__, "Adv stop failed");
        }
        else {
            ESP_LOGI(__FUNCTION__, "Stop adv successfully");
        }
        break;

    default:
        break;
    }
}

void Bt::bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    app_gap_cb_t *p_dev = instance->m_dev_info;
    char bda_str[18];
    char uuid_str[37];
    cJSON* dev = NULL;
    bda2str(param->disc_res.bda, bda_str, 18);

    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        update_device_info(param);
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            ESP_LOGD(__FUNCTION__, "Device discovery stopped.");
            cJSON_SetValuestring(instance->btStatus, "On");
            if ( (p_dev->state == APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE ||
                    p_dev->state == APP_GAP_STATE_DEVICE_DISCOVERING)
                    && p_dev->dev_found) {
                p_dev->state = APP_GAP_STATE_SERVICE_DISCOVERING;
                ESP_LOGD(__FUNCTION__, "Discover services ...");
                esp_bt_gap_get_remote_services(p_dev->bda);
            }
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            cJSON_SetValuestring(instance->btStatus, "Scanning");
            ESP_LOGD(__FUNCTION__, "Discovery started.");
        }
        AppConfig::SignalStateChange(state_change_t::MAIN);
        break;
    }
    case ESP_BT_GAP_RMT_SRVCS_EVT: {
        if (memcmp(param->rmt_srvcs.bda, p_dev->bda, ESP_BD_ADDR_LEN) == 0 &&
                p_dev->state == APP_GAP_STATE_SERVICE_DISCOVERING) {
            p_dev->state = APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE;
            if (param->rmt_srvcs.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGD(__FUNCTION__, "Services for device %s found",  bda2str(p_dev->bda, bda_str, 18));
                for (int i = 0; i < param->rmt_srvcs.num_uuids; i++) {
                    esp_bt_uuid_t *u = param->rmt_srvcs.uuid_list + i;
                    ESP_LOGD(__FUNCTION__, "--%s", uuid2str(u, uuid_str, 37));
                    esp_bt_gap_get_remote_service_record(p_dev->bda, u);
                    dev=instance->GetService(bda_str,u);
                    if (!dev) {
                        dev=instance->AddService(bda_str,u);
                        AppConfig::SignalStateChange(state_change_t::MAIN);
                    }
                    // ESP_LOGD(__FUNCTION__, "--%d", u->len);
                }
            } else {
                ESP_LOGD(__FUNCTION__, "Services for device %s not found",  bda2str(p_dev->bda, bda_str, 18));
            }
        }
        break;
    }
    default: {
        ESP_LOGD(__FUNCTION__, "event:%d", event);
        break;
    }
    }
    return;
}

void Bt::InitDevice(){
    ESP_LOGI(__FUNCTION__,"Initializing bt");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = ESP_OK;

    if (strcmp(btStatus->valuestring,"idle") == 0){
        if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
            ESP_LOGE(__FUNCTION__, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }
        ESP_LOGD(__FUNCTION__,"bt controler init");
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
    }
    ESP_LOGD(__FUNCTION__,"bt controler enabled");

    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
    }
    ESP_LOGD(__FUNCTION__,"Bt initted");

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGD(__FUNCTION__,"Bt enabled");

    cJSON_SetValuestring(btStatus, "On");
    AppConfig::SignalStateChange(state_change_t::MAIN);

    const char *dev_name = "THE_FUCK_YOU_LOOKIN_AT";
    esp_bt_dev_set_device_name(dev_name);

    /* register GAP callback function */
    esp_bt_gap_register_callback(bt_app_gap_cb);
    if ((ret = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "gap register error, error code = %x", ret);
    }
    /* set discoverable and connectable mode, wait to be connected */
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

    /* inititialize device information and status */
    app_gap_cb_t *p_dev = m_dev_info;
    memset(p_dev, 0, sizeof(app_gap_cb_t));

    /* start to discover nearby Bt devices */
    p_dev->state = APP_GAP_STATE_DEVICE_DISCOVERING;

    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    esp_ble_gap_set_scan_params(&ble_scan_params);
    ESP_LOGD(__FUNCTION__,"Bt running");
}

void Bt::DeInitDevice(){
    ESP_LOGI(__FUNCTION__,"DeInitializing bt");
    esp_err_t ret = ESP_OK;

    esp_bt_gap_cancel_discovery();

    if ((ret = esp_bluedroid_disable()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s disable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
    }
    ESP_LOGD(__FUNCTION__,"Bt disabled");

    if ((ret = esp_bluedroid_deinit()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
    }
    ESP_LOGD(__FUNCTION__,"Bt deinit");

    if ((ret = esp_bt_controller_disable()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s disable controller failed: %s\n", __func__, esp_err_to_name(ret));
    }

    ESP_LOGD(__FUNCTION__,"Bt controler disable");
    if ((ret = esp_bt_controller_deinit()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s deinitialize controller failed: %s\n", __func__, esp_err_to_name(ret));
    }
    ESP_LOGD(__FUNCTION__,"Bt controler deinit");

    if ((ret = esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s deinitialize controller failed: %s\n", __func__, esp_err_to_name(ret));
    }

    cJSON_SetValuestring(btStatus, "Off");
    AppConfig::SignalStateChange(state_change_t::MAIN);
}

bool Bt::ProcessCommand(ManagedDevice* dev, cJSON * parms) {
    Bt* leds = (Bt*)dev;
    if (cJSON_HasObjectItem(parms,"command")) {
        char* command = cJSON_GetObjectItem(parms,"command")->valuestring;
        if (strcmp("SetBtState", command) == 0 && cJSON_HasObjectItem(parms,"param1")) {
            cJSON* param = cJSON_GetObjectItem(parms, "param1");
            if (strcmp(param->valuestring,"ON") == 0) {
                leds->InitDevice();
            } else if (strcmp(param->valuestring,"OFF") == 0) {
                leds->DeInitDevice();
            } else {
                ESP_LOGW(__FUNCTION__,"Invalid param1 value");
                return false;
            }
        } else if (strcmp("ScanForDevices", command) == 0) {
            esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
        } else {
            ESP_LOGV(__FUNCTION__,"Invalid Command %s", command);
            return false;
        }
        return true;
    }
    return false;
}

cJSON* Bt::GetDevice(const char* uuid){
    cJSON* dev = NULL;
    cJSON_ArrayForEach(dev,devices) {
        if (cJSON_HasObjectItem(dev,"uuid") && strcmp(cJSON_GetObjectItem(dev,"uuid")->valuestring,uuid) == 0) {
            return dev;
        }
    }
    return NULL;
}

cJSON* Bt::AddDevice(const char* uuid){
    cJSON* dev = cJSON_CreateObject();
    cJSON_AddItemToArray(devices,dev);

    cJSON_AddStringToObject(dev,"uuid",uuid);
    cJSON_AddStringToObject(dev,"firstSeen","                             ");
    cJSON_AddStringToObject(dev,"lastSeen","                             ");

    return dev;
}

cJSON* Bt::GetService(const char* uuid, esp_bt_uuid_t* svc){
    cJSON* dev = GetDevice(uuid);
    if (dev) {
        cJSON* services = cJSON_GetObjectItem(dev,"services");
        if (!services) {
            services = cJSON_CreateArray();
            cJSON_AddItemToObject(dev,"services", services);
        }
        char uuid_str[37];
        ESP_LOGD(__FUNCTION__, "--%s", uuid2str(svc, uuid_str, 37));
        cJSON* service = NULL;
        cJSON_ArrayForEach(service,services) {
            if (cJSON_HasObjectItem(service,"uuid") && strcmp(cJSON_GetObjectItem(service,"uuid")->valuestring,uuid_str) == 0) {
                return service;
            }
        }
    }
    return NULL;
}

cJSON* Bt::AddService(const char* uuid, esp_bt_uuid_t* svc){
    cJSON* dev = GetDevice(uuid);
    if (dev) {
        cJSON* services = cJSON_GetObjectItem(dev,"services");
        if (!services) {
            services = cJSON_CreateArray();
            cJSON_AddItemToObject(dev,"services", services);
        }
        char uuid_str[37];
        ESP_LOGD(__FUNCTION__, "--%s", uuid2str(svc, uuid_str, 37));
        cJSON* svc = cJSON_CreateObject();
        cJSON_AddStringToObject(svc,"uuid",uuid_str);
        cJSON_AddItemToArray(services,svc);
        return svc;        
    }
    return NULL;
}

void Bt::RemoveDevice(const char* uuid){

}
