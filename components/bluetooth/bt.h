#ifndef __bt_h
#define __bt_h

#include <stdio.h>
#include <string.h>
// #include "cJSON.h"
#include "eventmgr.h"
// #include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"

typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;
    uint16_t beacon_type;
}__attribute__((packed)) esp_ble_ibeacon_head_t;

typedef struct {
    uint8_t proximity_uuid[16];
    uint16_t major;
    uint16_t minor;
    int8_t measured_power;
}__attribute__((packed)) esp_ble_ibeacon_vendor_t;

typedef struct {
    esp_ble_ibeacon_head_t ibeacon_head;
    esp_ble_ibeacon_vendor_t ibeacon_vendor;
}__attribute__((packed)) esp_ble_ibeacon_t;

class Bt:ManagedDevice {
public:
    Bt();
    static bool ProcessCommand(ManagedDevice* dev, cJSON * parms);
    enum eventIds {
        OFF,ON,SCANED
    };
protected:
    static void update_device_info(esp_bt_gap_cb_param_t *param);
    static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
    static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
    static QueueHandle_t eventQueue;
    void InitDevice();
    void DeInitDevice();
    static const char* BT_BASE;
    void RefrestState();
    EventHandlerDescriptor* BuildHandlerDescriptors();
    cJSON* btStatus;
    cJSON* devices;
    cJSON* GetDevice(const char* uuid);
    cJSON* AddDevice(const char* uuid);
    cJSON* GetService(const char* uuid, esp_bt_uuid_t* svc);
    cJSON* AddService(const char* uuid, esp_bt_uuid_t* svc);
    void RemoveDevice(const char* uuid);
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