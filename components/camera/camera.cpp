#include "include/camera.h"
#include "esp_sleep.h"
#include "esp_camera.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_http_server.h"

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
    cameraType(config->GetStringProperty("type"))
{
    auto* appstate = new AppConfig(status,AppConfig::GetAppStatus());
    appstate->SetStringProperty("name",config->GetStringProperty("type"));
    appstate->SetIntProperty("initialized",0);
    appstate->SetIntProperty("streaming",0);
    initialized = appstate->GetPropertyHolder("initialized");
    streaming = appstate->GetPropertyHolder("streaming");

    InitDevice();
    delete appstate;
}

cJSON* Camera::BuildConfigTemplate() {
    cJSON* commandTemplate = ManagedDevice::BuildConfigTemplate();
    cJSON_SetValuestring(cJSON_GetObjectItem(commandTemplate,"class"),"Camera");
    cJSON_AddTrueToObject(commandTemplate, "isArray");
    cJSON_AddStringToObject(commandTemplate,"collectionName","Cameras");
    cJSON_AddStringToObject(commandTemplate,"type","OV2640");

    cJSON_AddNumberToObject(commandTemplate, "CAM_PIN_PWDN",-1);
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
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_VSYNC",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_HREF",-1);
    cJSON_AddNumberToObject(commandTemplate,"CAM_PIN_PCLK",-1);

    return commandTemplate;
}

EventHandlerDescriptor* Camera::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"%s BuildHandlerDescriptors",name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(1,"setTargetAngle",event_data_type_tp::JSON);
  return handler;
}

void Camera::InitDevice(){
    ESP_LOGI(__FUNCTION__,"Initialising %s camera",this->cameraType);

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

        .xclk_freq_hz = 20000000,//EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_UXGA,//QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

        .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
        .fb_count = 1, //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,//CAMERA_GRAB_LATEST. Sets when buffers should be filled
    };

    //power up the camera if PWDN pin is defined
    if(this->pin_pwdn != -1){
        // pinMode(this->pin_pwdn, OUTPUT);
        gpio_set_level(this->pin_pwdn,0);
    }

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    cJSON_SetBoolValue(initialized,err == ESP_OK);
    if (err != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "Camera Init Failed %d",cJSON_IsFalse(initialized));
    } else {
        ESP_LOGI(__FUNCTION__, "Camera Initialized");
    }
}

const char* Camera::GetName() {
    return ManagedDevice::GetName();
}


esp_err_t Camera::streamVideo(httpd_req_t * req) {
    if (cJSON_IsFalse(initialized)) {
        ESP_LOGE(__FUNCTION__,"Camera not initialized");
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
        ESP_LOGE(__FUNCTION__,"Cannot set resp type");
    } else {
        ESP_LOGI(__FUNCTION__,"Straming %s", GetName());
        cJSON_SetBoolValue(streaming,cJSON_True);
        while(true){
            fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(__FUNCTION__, "Camera capture failed");
                res = ESP_FAIL;
                httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Camera capture failed");
                break;
            }
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                if(!jpeg_converted){
                    ESP_LOGE(__FUNCTION__, "JPEG compression failed");
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
            ESP_LOGI(__FUNCTION__, "MJPG: %uKB %ums (%.1ffps)",
                (uint32_t)(_jpg_buf_len/1024),
                (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
        }
    }
    return res;
}


bool Camera::ProcessCommand(ManagedDevice* camera, cJSON * parms) {
    if (strcmp(camera->GetName(), cJSON_GetObjectItem(parms,"name")->valuestring) == 0) {
        Camera* s = (Camera*) camera;
        return true;
    }
    return false;
}

