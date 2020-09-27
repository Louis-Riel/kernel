#ifndef __utils_h
#define __utils_h

#include "../build/config/sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <stdlib.h>
#include "TinyGPS++.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "station.h"
#include "rest.h"

#define SS TF_CS
#define PIN_NUM_MISO (gpio_num_t)19
#define PIN_NUM_MOSI (gpio_num_t)23
#define PIN_NUM_CLK (gpio_num_t)18
#define PIN_NUM_CS (gpio_num_t)4

bool moveFile(char* src, char* dest);
const char* getErrorMsg(FRESULT errCode);
bool initSPISDCard();
sdmmc_host_t* getSDHost();
#endif