#ifndef __camera_h
#define __camera_h

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/mcpwm.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../../main/logs.h"
#include "../../../main/utils.h"
#include "cJSON.h"
#include "eventmgr.h"
#include "esp_http_server.h"

class Camera:ManagedDevice {
public:
    Camera(AppConfig* config);
    ~Camera();
    static cJSON* BuildConfigTemplate();
    esp_err_t streamVideo(httpd_req_t * req);
    const char *GetName();

protected:
    static const char* CAMERA_BASE;
    gpio_num_t pin_pwdn;
    gpio_num_t pin_reset;
    gpio_num_t pin_xclk;
    gpio_num_t pin_sccb_sda;
    gpio_num_t pin_sccb_scl;

    gpio_num_t pin_d7;
    gpio_num_t pin_d6;
    gpio_num_t pin_d5;
    gpio_num_t pin_d4;
    gpio_num_t pin_d3;
    gpio_num_t pin_d2;
    gpio_num_t pin_d1;
    gpio_num_t pin_d0;
    gpio_num_t pin_vsync;
    gpio_num_t pin_href;
    gpio_num_t pin_pclk;
    const char* cameraType;

    void InitDevice();
    EventHandlerDescriptor* BuildHandlerDescriptors();

    static bool ProcessCommand(ManagedDevice*, cJSON *);

    cJSON* initialized;
    cJSON* streaming;
};

#endif