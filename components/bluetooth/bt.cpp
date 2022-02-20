#include "bt.h"
#include "cJSON.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "../../../main/utils.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include <cstring>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

const char* Bt::BT_BASE="Bt";
static Bt* instance = NULL;

Bt::Bt()
    :ManagedDevice(BT_BASE,BT_BASE,BuildStatus)
{
    ESP_LOGV(__FUNCTION__,"Bt start");
    
    if (instance == NULL) {
        instance = this;
    }

    InitDevice();
    cJSON* jcfg;
    AppConfig* apin = new AppConfig((jcfg=BuildStatus(this)),AppConfig::GetAppStatus());
    apin->SetStringProperty("name",name);
    apin->SetStringProperty("state","Off");
    btStatus = apin->GetPropertyHolder("state");

    cJSON* methods = cJSON_AddArrayToObject(jcfg,"commands");
    cJSON* flush = cJSON_CreateObject();
    cJSON_AddItemToArray(methods,flush);
    cJSON_AddStringToObject(flush,"command","SetBtState");
    cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
    cJSON_AddStringToObject(flush,"param1","ON");
    cJSON_AddStringToObject(flush,"caption","On");
    flush = cJSON_CreateObject();
    cJSON_AddItemToArray(methods,flush);
    cJSON_AddStringToObject(flush,"command","SetBtState");
    cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
    cJSON_AddStringToObject(flush,"param1","OFF");
    cJSON_AddStringToObject(flush,"caption","Off");
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

    ESP_LOGI(__FUNCTION__, "Device found: %s", bda2str(param->disc_res.bda, bda_str, 18));
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            ESP_LOGI(__FUNCTION__, "--Class of Device: 0x%x", cod);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            ESP_LOGI(__FUNCTION__, "--RSSI: %d", rssi);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
        default:
            break;
        }
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
        get_name_from_eir(p_dev->eir, p_dev->bdname, &p_dev->bdname_len);
        ESP_LOGI(__FUNCTION__, "Found a target device, address %s, name %s", bda_str, p_dev->bdname);
        p_dev->state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
        ESP_LOGI(__FUNCTION__, "Cancel device discovery ...");
        esp_bt_gap_cancel_discovery();
    }
}

void Bt::bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    app_gap_cb_t *p_dev = instance->m_dev_info;
    char bda_str[18];
    char uuid_str[37];

    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        update_device_info(param);
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            ESP_LOGI(__FUNCTION__, "Device discovery stopped.");
            if ( (p_dev->state == APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE ||
                    p_dev->state == APP_GAP_STATE_DEVICE_DISCOVERING)
                    && p_dev->dev_found) {
                p_dev->state = APP_GAP_STATE_SERVICE_DISCOVERING;
                ESP_LOGI(__FUNCTION__, "Discover services ...");
                esp_bt_gap_get_remote_services(p_dev->bda);
            }
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            ESP_LOGI(__FUNCTION__, "Discovery started.");
        }
        break;
    }
    case ESP_BT_GAP_RMT_SRVCS_EVT: {
        if (memcmp(param->rmt_srvcs.bda, p_dev->bda, ESP_BD_ADDR_LEN) == 0 &&
                p_dev->state == APP_GAP_STATE_SERVICE_DISCOVERING) {
            p_dev->state = APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE;
            if (param->rmt_srvcs.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(__FUNCTION__, "Services for device %s found",  bda2str(p_dev->bda, bda_str, 18));
                for (int i = 0; i < param->rmt_srvcs.num_uuids; i++) {
                    esp_bt_uuid_t *u = param->rmt_srvcs.uuid_list + i;
                    ESP_LOGI(__FUNCTION__, "--%s", uuid2str(u, uuid_str, 37));
                    // ESP_LOGI(__FUNCTION__, "--%d", u->len);
                }
            } else {
                ESP_LOGI(__FUNCTION__, "Services for device %s not found",  bda2str(p_dev->bda, bda_str, 18));
            }
        }
        break;
    }
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
    default: {
        ESP_LOGI(__FUNCTION__, "event: %d", event);
        break;
    }
    }
    return;
}

void Bt::InitDevice(){
    ESP_LOGD(__FUNCTION__,"Initializing bt");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = ESP_OK;

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    cJSON_SetValuestring(btStatus, "On");

    char *dev_name = "THE_FUCK_YOU_LOOKIN_AT";
    esp_bt_dev_set_device_name(dev_name);

    /* register GAP callback function */
    esp_bt_gap_register_callback(bt_app_gap_cb);

    /* set discoverable and connectable mode, wait to be connected */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    /* inititialize device information and status */
    app_gap_cb_t *p_dev = m_dev_info;
    memset(p_dev, 0, sizeof(app_gap_cb_t));

    /* start to discover nearby Bt devices */
    p_dev->state = APP_GAP_STATE_DEVICE_DISCOVERING;

    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}

void Bt::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    if (strcmp(base,BT_BASE)==0){
        Bt* pin = NULL;
        ESP_LOGV(__FUNCTION__,"Event %s-%d - %" PRIXPTR " %" PRIXPTR, base,id,(uintptr_t)event_data,(uintptr_t)*(cJSON**)event_data);
        cJSON* params=*(cJSON**)event_data;
        if (cJSON_IsInvalid(params)) {
            if (*(char*)event_data == '{') {
                params = cJSON_Parse((char*)event_data);
                ESP_LOGV(__FUNCTION__,"evt:%s",(char*)event_data);
            } else {
                ESP_LOGW(__FUNCTION__,"Unknown param type");
            }
        }

        if (!cJSON_IsInvalid(params)) {
            ESP_LOGV(__FUNCTION__,"Params(0x%" PRIXPTR " 0x%" PRIXPTR ")",(uintptr_t)event_data,(uintptr_t)params);
            if (params != NULL) {
                if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
                    char* stmp = cJSON_Print(params);
                    ESP_LOGV(__FUNCTION__,"req: %s", stmp);
                    ldfree(stmp);
                }
            } else {
                ESP_LOGE(__FUNCTION__,"Missing params");
            }
        } else {
            ESP_LOGW(__FUNCTION__,"Cannot parse param");
        }
    }
}
