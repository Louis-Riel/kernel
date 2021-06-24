#ifndef __rest_h
#define __rest_h
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp32/rom/md5_hash.h"
#include "../wifi/station.h"
#include "../microtar/src/microtar.h"
#include "../../main/utils.h"
#include "freertos/ringbuf.h"
#include "esp_http_client.h"
#include "cJSON.h"

#define MESSAGE_BUFFER_SIZE 4096
#define NUM_IMG_BUFFERS     512
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192
#define HTTP_BUF_SIZE 8192
#define HTTP_CHUNK_SIZE 8192
#define HTTP_RECEIVE_BUFFER_SIZE 8192
#define JSON_BUFFER_SIZE 8192
#define KML_BUFFER_SIZE 204600

static EventGroupHandle_t eventGroup=xEventGroupCreate();

cJSON* status_json();
cJSON* tasks_json();
cJSON* getMemoryStats();
void restSallyForth(void *pvParameter);
void pullStation(void *pvParameter);
bool moveFolder(char* folderName, char* toFolderName);
char* getPostField(const char* pname, const char* postData,char* dest);
esp_err_t filedownload_event_handler(esp_http_client_event_t *evt);
void extractClientTar(char* tarFName);
typedef enum{
    TAR_BUFFER_FILLED = BIT0,
    TAR_BUFFER_SENT = BIT1,
    TAR_BUILD_DONE = BIT2,
    TAR_SEND_DONE = BIT3,
    HTTP_SERVING = BIT4,
    GETTING_TRIP_LIST = BIT5,
    GETTING_TRIPS = BIT6,
    DOWNLOAD_STARTED = BIT7,
    DOWNLOAD_FINISHED = BIT8
} restServerState_t;
#endif