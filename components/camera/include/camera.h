#ifndef __camera_h
#define __camera_h

#include <cstdint>
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
#include "../../rest/route.h"
#include "freertos/semphr.h"

class Camera;
typedef struct {
    Camera* camera;
    WebsocketManager::ws_client_t* client;
} camera_ws_stream_t;

class Camera:ManagedDevice {
public:
    Camera(AppConfig* config);
    ~Camera();
    static cJSON* BuildConfigTemplate();
    esp_err_t streamVideo(httpd_req_t * req);
    static void streamVideoToWs(void * wsStreanmer);
    void sendFrame();
    static void sentFrame(void* param);
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
    uint8_t ledc_timer;
    uint8_t ledc_channel;
    uint8_t pixel_format;
    uint8_t frame_size;
    uint8_t fb_count;
    int jpeg_quality;
    const char* cameraType;

    bool InitDevice();
    EventHandlerDescriptor* BuildHandlerDescriptors();

    static bool ProcessCommand(ManagedDevice*, cJSON *);

    cJSON* powered;
    cJSON* initialized;
    cJSON* streaming;
private:
    WebsocketManager::ws_msg_t* clients[5];
    void* inTransit[5];
    int64_t frameTs[5];
    SemaphoreHandle_t frameLock;

    static void Janitor(void*);
};

#endif