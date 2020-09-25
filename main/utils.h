#ifndef __station_h
#define __station_h

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
#include "station.h"
#include "rest.h"

const char* getErrorMsg(FRESULT errCode);

#endif