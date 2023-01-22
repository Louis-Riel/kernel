#include "include/camera.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_camera.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "eventmgr.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

const char* Camera::CAMERA_BASE="Camera";

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

Camera::Camera(AppConfig* config)
    :ManagedDevice(CAMERA_BASE,config->GetStringProperty("type"),nullptr,&ProcessCommand),
    pin_pwdn(config->GetPinNoProperty("CAM_PIN_PWDN")),
    pin_reset(config->GetPinNoProperty("CAM_PIN_RESET")),
    pin_xclk(config->GetPinNoProperty("CAM_PIN_XCLK")),
    pin_sccb_sda(config->GetPinNoProperty("CAM_PIN_SIOD")),
    pin_sccb_scl(config->GetPinNoProperty("CAM_PIN_SIOC")),
    pin_d7(config->GetPinNoProperty("CAM_PIN_D7")),
    pin_d6(config->GetPinNoProperty("CAM_PIN_D6")),
    pin_d5(config->GetPinNoProperty("CAM_PIN_D5")),
    pin_d4(config->GetPinNoProperty("CAM_PIN_D4")),
    pin_d3(config->GetPinNoProperty("CAM_PIN_D3")),
    pin_d2(config->GetPinNoProperty("CAM_PIN_D2")),
    pin_d1(config->GetPinNoProperty("CAM_PIN_D1")),
    pin_d0(config->GetPinNoProperty("CAM_PIN_D0")),
    pin_vsync(config->GetPinNoProperty("CAM_PIN_VSYNC")),
    pin_href(config->GetPinNoProperty("CAM_PIN_HREF")),
    pin_pclk(config->GetPinNoProperty("CAM_PIN_PCLK")),
    ledc_timer(config->GetIntProperty("LEDC_TIMER")),
    ledc_channel(config->GetIntProperty("LEDC_CHANNEL")),
    pixel_format(config->GetIntProperty("PIXEL_FORMAT")),
    frame_size(config->GetIntProperty("FRAME_SIZE")),
    fb_count(config->GetIntProperty("FB_COUNT")),
    jpeg_quality(config->GetIntProperty("JPEG_QUALITY")),
    cameraType(config->GetStringProperty("type")),
    frameLock(xSemaphoreCreateBinary())
{
    xSemaphoreGive(frameLock);
    auto* appstate = new AppConfig(status,AppConfig::GetAppStatus());
    appstate->SetStringProperty("name",config->GetStringProperty("type"));
    appstate->SetIntProperty("initialized",0);
    appstate->SetIntProperty("streaming",0);
    appstate->SetIntProperty("powered",0);
    initialized = appstate->GetPropertyHolder("initialized");
    streaming = appstate->GetPropertyHolder("streaming");
    powered = appstate->GetPropertyHolder("powered");
    std::memset(clients,0,5*sizeof(void*));
    std::memset(inTransit,0,5*sizeof(void*));
    std::memset(frameTs,0,sizeof(int64_t)*5);
    delete appstate;
}

cJSON* Camera::BuildConfigTemplate() {
    cJSON* commandTemplate = ManagedDevice::BuildConfigTemplate();
    cJSON_SetValuestring(cJSON_GetObjectItem(commandTemplate,"class"),"Camera");
    cJSON_AddTrueToObject(commandTemplate, "isArray");
    cJSON_AddStringToObject(commandTemplate,"collectionName","Cameras");
    cJSON_AddStringToObject(commandTemplate,"type","OV2640");

    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_PWDN",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_RESET",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_XCLK",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_SIOD",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_SIOC",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D7",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D6",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D5",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D4",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D3",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D2",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D1",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_D0",-1);
    cJSON_AddNumberToObject(commandTemplate,"LEDC_TIMER",0);
    cJSON_AddNumberToObject(commandTemplate,"LEDC_CHANNEL",0);
    cJSON_AddNumberToObject(commandTemplate,"JPEG_QUALITY",12);
    cJSON_AddNumberToObject(commandTemplate,"PIXEL_FORMAT",4);
    cJSON_AddNumberToObject(commandTemplate,"FRAME_SIZE",5);
    cJSON_AddNumberToObject(commandTemplate,"FB_COUNT",1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_VSYNC",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_HREF",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_PCLK",-1);

    return commandTemplate;
}

EventHandlerDescriptor* Camera::BuildHandlerDescriptors(){
  ESP_LOGV(__PRETTY_FUNCTION__,"%s BuildHandlerDescriptors",name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(1,"setTargetAngle",event_data_type_tp::JSON);
  return handler;
}

bool Camera::InitDevice(){
    if (this->initialized->valueint == 0){
        ESP_LOGI(__PRETTY_FUNCTION__,"Initializing %s camera",this->cameraType);

        static camera_config_t camera_config = {
            .pin_pwdn  = this->pin_pwdn,
            .pin_reset = this->pin_reset,
            .pin_xclk = this->pin_xclk,
            .pin_sccb_sda = this->pin_sccb_sda,
            .pin_sccb_scl = this->pin_sccb_scl,
            .pin_d7 = this->pin_d7,
            .pin_d6 = this->pin_d6,
            .pin_d5 = this->pin_d5,
            .pin_d4 = this->pin_d4,
            .pin_d3 = this->pin_d3,
            .pin_d2 = this->pin_d2,
            .pin_d1 = this->pin_d1,
            .pin_d0 = this->pin_d0,
            .pin_vsync = this->pin_vsync,
            .pin_href = this->pin_href,
            .pin_pclk = this->pin_pclk,

            .xclk_freq_hz = 10000000,//EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
            .ledc_timer = (ledc_timer_t)this->ledc_timer,
            .ledc_channel = (ledc_channel_t)this->ledc_channel,

            .pixel_format = (pixformat_t)this->pixel_format,//PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
            .frame_size = (framesize_t)this->frame_size, //FRAMESIZE_QVGA,//QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

            .jpeg_quality = this->jpeg_quality, //0-63, for OV series camera sensors, lower number means higher quality
            .fb_count = this->fb_count, //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
            .fb_location = CAMERA_FB_IN_PSRAM,
            .grab_mode = CAMERA_GRAB_LATEST,//CAMERA_GRAB_LATEST. Sets when buffers should be filled
            .sccb_i2c_port = -1
        };

        //initialize the camera
        esp_err_t ret;
        ESP_LOGI(__PRETTY_FUNCTION__,"Initializing camera with ");
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_pwdn=%d",camera_config.pin_pwdn);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_reset=%d",camera_config.pin_reset);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_xclk=%d",camera_config.pin_xclk);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_sccb_sda=%d",camera_config.pin_sccb_sda);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_sccb_scl=%d",camera_config.pin_sccb_scl);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d7=%d",camera_config.pin_d7);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d6=%d",camera_config.pin_d6);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d5=%d",camera_config.pin_d5);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d4=%d",camera_config.pin_d4);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d3=%d",camera_config.pin_d3);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d2=%d",camera_config.pin_d2);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d1=%d",camera_config.pin_d1);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_d0=%d",camera_config.pin_d0);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_vsync=%d",camera_config.pin_vsync);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_href=%d",camera_config.pin_href);
        ESP_LOGI(__PRETTY_FUNCTION__,"pin_pclk=%d",camera_config.pin_pclk);
        ESP_LOGI(__PRETTY_FUNCTION__,"xclk_freq_hz=%d",camera_config.xclk_freq_hz);
        ESP_LOGI(__PRETTY_FUNCTION__,"ledc_timer=%d",camera_config.ledc_timer);
        ESP_LOGI(__PRETTY_FUNCTION__,"ledc_channel=%d",camera_config.ledc_channel);
        ESP_LOGI(__PRETTY_FUNCTION__,"pixel_format=%d",camera_config.pixel_format);
        ESP_LOGI(__PRETTY_FUNCTION__,"frame_size=%d",camera_config.frame_size);
        ESP_LOGI(__PRETTY_FUNCTION__,"jpeg_quality=%d",camera_config.jpeg_quality);
        ESP_LOGI(__PRETTY_FUNCTION__,"fb_count=%zu",camera_config.fb_count);
        ESP_LOGI(__PRETTY_FUNCTION__,"fb_location=%d",camera_config.fb_location);
        ESP_LOGI(__PRETTY_FUNCTION__,"grab_mode=%d",camera_config.grab_mode);
        ESP_LOGI(__PRETTY_FUNCTION__,"sccb_i2c_port=%d",camera_config.sccb_i2c_port);


        //power up the camera if PWDN pin is defined
        if(this->pin_pwdn != -1){
            ESP_ERROR_CHECK(gpio_set_direction(this->pin_pwdn,gpio_mode_t::GPIO_MODE_OUTPUT));
            ESP_ERROR_CHECK(gpio_set_level(this->pin_pwdn,0));
            cJSON_SetIntValue(powered,1);
            ESP_LOGI(__PRETTY_FUNCTION__, "Camera powered up");
        } else {
            ESP_LOGI(__PRETTY_FUNCTION__, "Camera does not have pwdn");
        }

        cJSON_SetIntValue(initialized,(ret=esp_camera_init(&camera_config)) == ESP_OK ? 1: 0);
        if (initialized->valueint == 1) {
            ESP_LOGI(__PRETTY_FUNCTION__, "Camera Initialized");
        } else {
            ESP_LOGE(__PRETTY_FUNCTION__, "Camera Init Failed:(%#03x)%s",ret,esp_err_to_name(ret));
        }

        CreateBackgroundTask(Camera::Janitor, "FrameJanitor", 4096, this, tskIDLE_PRIORITY, NULL);
    }

    return initialized->valueint == 1;
}

const char* Camera::GetName() {
    return ManagedDevice::GetName();
}

struct camera_frame_request_t {
    Camera* camera;
    WebsocketManager::ws_msg_t* wsMsg;
};

void Camera::sentFrame(void* param) {
    void** params = (void**)param;
    Camera* camera = (Camera*) params[0];
    void* pos = params[1];
    void* fb = camera->inTransit[(uint64_t)pos];
    camera->inTransit[(uint64_t)pos] = nullptr;
    camera->frameTs[(uint64_t)pos] = 0;
    uint8_t activeClients = 0;
    ESP_LOGV(__PRETTY_FUNCTION__,"frame returned by %d",(int)(int64_t)pos);

    for (uint8_t idx = 0; idx < 5; idx++) {
        if (camera->inTransit[idx]) {
            ESP_LOGV(__PRETTY_FUNCTION__,"client %d is still active",idx);
            activeClients++;
        }
    }

    if ((activeClients == 0) && fb) {
        if (params[2]) {
            ESP_LOGV(__PRETTY_FUNCTION__,"client %d freed compressed jpg",(int)(int64_t)pos);
            free(fb);
        } else {
            ESP_LOGV(__PRETTY_FUNCTION__,"client %d returned frame", (int)(int64_t)pos);
            esp_camera_fb_return((camera_fb_t*)fb);
        }
        camera->sendFrame();
    }
}

void Camera::sendFrame() {
    ESP_LOGV(__PRETTY_FUNCTION__,"frame start");

    uint8_t activeClients = 0;

    camera_fb_t* fb = nullptr;

    ESP_LOGV(__PRETTY_FUNCTION__,"taking");
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    void* isCompressed = nullptr;
    if (xSemaphoreTake(frameLock, portMAX_DELAY)) {
        ESP_LOGV(__PRETTY_FUNCTION__,"took");
        fb = esp_camera_fb_get();

        if(fb && fb->format != PIXFORMAT_JPEG){
            ESP_LOGV(__PRETTY_FUNCTION__,"uncompressed image %zu",fb->len);
            if(!frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len)){
                xSemaphoreGive(frameLock);
                esp_camera_fb_return(fb);
                ESP_LOGE(__PRETTY_FUNCTION__, "JPEG compression failed");
            } else {
                xSemaphoreGive(frameLock);
                isCompressed = _jpg_buf;
                esp_camera_fb_return(fb);
                ESP_LOGV(__PRETTY_FUNCTION__,"compressed jpg %zu",_jpg_buf_len);
            }
        } else if (fb) {
            xSemaphoreGive(frameLock);
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
            ESP_LOGV(__PRETTY_FUNCTION__,"native jpg %zu",_jpg_buf_len);
        }
    }

    if (fb) {
        for (uint8_t idx = 0; idx < 5; idx++) {
            WebsocketManager::ws_msg_t* wsMsg = clients[idx];
            if (wsMsg) {
                if (!wsMsg->client->jIsLive->valueint) {
                    clients[idx] = nullptr;
                    ESP_LOGW(__PRETTY_FUNCTION__,"Client %d had disconnected",idx);
                    ldfree((wsMsg->postHookParam));
                    ldfree(wsMsg);
                } else if (!inTransit[idx]) {
                    wsMsg->buf = _jpg_buf;
                    wsMsg->len = _jpg_buf_len;
                    esp_err_t ret = httpd_queue_work(wsMsg->client->hd,WebsocketManager::PostToClient,wsMsg);

                    if (ret != ESP_OK) {
                        cJSON_SetIntValue(wsMsg->client->jIsLive,false);
                        httpd_sess_trigger_close(wsMsg->client->hd, wsMsg->client->fd);
                        wsMsg->client->fd=0;
                        wsMsg->client->hd=nullptr;
                        AppConfig::SignalStateChange(state_change_t::MAIN);
                        clients[idx] = nullptr;
                        ESP_LOGW(__PRETTY_FUNCTION__,"Client %d disconnected: %s",idx, esp_err_to_name(ret));
                        ldfree((wsMsg->postHookParam));
                        ldfree(wsMsg);
                    } else {
                        inTransit[idx] = _jpg_buf;
                        frameTs[idx] = esp_timer_get_time();
                        ((void**)wsMsg->postHookParam)[2] = isCompressed;
                        activeClients++;
                    }
                }
            }
        }

        if (!fb || !activeClients) {
            ESP_LOGE(__PRETTY_FUNCTION__,"Stopping. Got a frame:%d. Frame sent:%d.",fb != nullptr, activeClients);
            for (uint8_t idx = 0; idx < 5; idx++) {
                clients[idx] = nullptr;
                inTransit[idx] = nullptr;
            }

            if (fb) {
                esp_camera_fb_return(fb);
            }

            ESP_LOGI(__PRETTY_FUNCTION__,"Streaming %s Done", GetName());

            if(pin_pwdn != -1){
                ESP_ERROR_CHECK(gpio_set_direction(pin_pwdn,gpio_mode_t::GPIO_MODE_OUTPUT));
                ESP_ERROR_CHECK(gpio_set_level(pin_pwdn,1));
                cJSON_SetIntValue(powered,0);
                ESP_LOGI(__PRETTY_FUNCTION__, "Camera powered down");
            } else {
                ESP_LOGI(__PRETTY_FUNCTION__, "Camera does not have pwdn");
            }
            esp_camera_deinit();
            cJSON_SetIntValue(initialized,0);
            cJSON_SetBoolValue(streaming,cJSON_False);
        }
    }
}

void Camera::Janitor(void* param) {
    ESP_LOGI(__PRETTY_FUNCTION__,"Starting Janitor");
    Camera* camera = (Camera*) param;
    int64_t curTs = 0;
    while (camera->initialized->valueint) {
        curTs = esp_timer_get_time();
        for (int idx =0; idx<5; idx++) {
            if (camera->frameTs[idx]) {
                ESP_LOGV(__PRETTY_FUNCTION__,"Client %d:%lld",idx,(long long int)curTs - camera->frameTs[idx]);
            }
            if (camera->clients[idx] && !camera->clients[idx]->client->jIsLive->valueint) {
                ESP_LOGW(__PRETTY_FUNCTION__,"Client %d disconnected",idx);
                camera->sendFrame();
                continue;
            }
            if (camera->frameTs[idx] && ((curTs - camera->frameTs[idx]) > 500000)) {
                ESP_LOGW(__PRETTY_FUNCTION__,"Stale frame for client %d",idx);
                camera->sentFrame(camera->clients[idx]->postHookParam);
            }
        }
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
    ESP_LOGI(__PRETTY_FUNCTION__,"Janitor done");
}

void Camera::streamVideoToWs(void * params){
    camera_ws_stream_t* request = (camera_ws_stream_t*)params;
    if (!request->camera->InitDevice()) {
        ESP_LOGE(__PRETTY_FUNCTION__,"Camera not initialized");
        return;
    }

    bool clientRegistered = false;

    for (uint8_t idx = 0; idx < 5; idx++) {
        if (!request->camera->clients[idx]) {
            WebsocketManager::ws_msg_t* wsMsg = (WebsocketManager::ws_msg_t*)dmalloc(sizeof(WebsocketManager::ws_msg_t));
            memset(wsMsg, 0, sizeof(WebsocketManager::ws_msg_t));
            void** params = (void**)dmalloc(3*sizeof(void*));
            params[0] = request->camera;
            params[1] = (void*)(uint64_t)idx;
            params[2] = nullptr;

            wsMsg->msgType = HTTPD_WS_TYPE_BINARY;
            wsMsg->postHook = sentFrame;
            wsMsg->postHookParam = params;
            wsMsg->client = request->client;
            request->camera->clients[idx] = wsMsg;
            clientRegistered = true;
            ESP_LOGI(__PRETTY_FUNCTION__,"Client Registered as %d", idx);
            break;
        }
    }

    if (!clientRegistered) {
        ESP_LOGE(__PRETTY_FUNCTION__,"Cannot register client");
        return;
    }
    cJSON_SetBoolValue(request->camera->streaming,cJSON_True);
    ESP_LOGI(__PRETTY_FUNCTION__,"Sending frame");
    request->camera->sendFrame();
    ESP_LOGI(__PRETTY_FUNCTION__,"Sent frame");
}

esp_err_t Camera::streamVideo(httpd_req_t * req) {
    if (!InitDevice()) {
        ESP_LOGE(__PRETTY_FUNCTION__,"Camera not initialized");
        httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Camera not initialized");
        return ESP_FAIL;
    }

    camera_fb_t * fb = NULL;
    
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        ESP_LOGE(__PRETTY_FUNCTION__,"Cannot set resp type");
    } else {
        ESP_LOGI(__PRETTY_FUNCTION__,"Straming %s", GetName());
        cJSON_SetBoolValue(streaming,cJSON_True);
        while(res == ESP_OK){
            fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Camera capture failed");
                res = ESP_FAIL;
                httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Camera capture failed");
                break;
            }
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                if(!jpeg_converted){
                    ESP_LOGE(__PRETTY_FUNCTION__, "JPEG compression failed");
                    esp_camera_fb_return(fb);
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }

            if(res == ESP_OK){
                res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            }
            if(res == ESP_OK){
                size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if(res == ESP_OK){
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }
            if(fb->format != PIXFORMAT_JPEG){
                free(_jpg_buf);
            }
            esp_camera_fb_return(fb);
            if(res != ESP_OK){
                httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Camera fb return failed");
                break;
            }

            int64_t fr_end = esp_timer_get_time();
            int64_t frame_time = fr_end - last_frame;
            last_frame = fr_end;
            frame_time /= 1000;
            ESP_LOGI(__PRETTY_FUNCTION__, "MJPG: %uKB %ums (%.1ffps)",
                (uint32_t)(_jpg_buf_len/1024),
                (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
        }
        ESP_LOGI(__PRETTY_FUNCTION__,"Straming %s Done", GetName());
        //power down the camera if PWDN pin is defined
        if(this->pin_pwdn != -1){
            ESP_ERROR_CHECK(gpio_set_direction(this->pin_pwdn,gpio_mode_t::GPIO_MODE_OUTPUT));
            ESP_ERROR_CHECK(gpio_set_level(this->pin_pwdn,1));
            cJSON_SetIntValue(powered,0);
            ESP_LOGI(__PRETTY_FUNCTION__, "Camera powered down");
        } else {
            ESP_LOGI(__PRETTY_FUNCTION__, "Camera does not have pwdn");
        }
        esp_camera_deinit();
        cJSON_SetIntValue(initialized,0);
    }
    return res;
}


bool Camera::ProcessCommand(ManagedDevice* camera, cJSON * parms) {
    if (strcmp(camera->GetName(), cJSON_GetObjectItem(parms,"name")->valuestring) == 0) {
        return true;
    }
    return false;
}

