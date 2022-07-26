#ifndef __bt_h
#define __bt_h

#include <stdio.h>
#include <string.h>
// #include "cJSON.h"
#include "eventmgr.h"
// #include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

class Bt:ManagedDevice {
public:
    Bt();
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    enum eventIds {
        OFF,ON,SCANED
    };
protected:
    static void update_device_info(esp_bt_gap_cb_param_t *param);
    static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
    static QueueHandle_t eventQueue;
    void InitDevice();
    static const char* BT_BASE;
    void RefrestState();
    EventHandlerDescriptor* BuildHandlerDescriptors();
    cJSON* btStatus;
    struct app_gap_cb_t {
        bool dev_found;
        uint8_t bdname_len;
        uint8_t eir_len;
        uint8_t rssi;
        uint32_t cod;
        uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
        uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
        esp_bd_addr_t bda;
        app_gap_state_t state;
    } *m_dev_info;
};

#endif