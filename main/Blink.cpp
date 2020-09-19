//#define CONFIG_TCPIP_LWIP 1 copy to sdkconfig
#include "../build/config/sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "TinyGPS++.h"
#include <time.h>
#include <sys/time.h>
#include <esp_pm.h>
#include "esp_event.h"
#include "esp_app_trace.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <stdlib.h>

#define BLINK_GPIO GPIO_NUM_5
#define GPS_EN_PIN GPIO_NUM_13

#define TRIP_BLOCK_SIZE 255

#ifndef Pins_Arduino_h
#define Pins_Arduino_h
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static const uint8_t LED_BUILTIN = 5;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
static const uint8_t _VBAT = 35; // battery voltage

#define SS TF_CS
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 4
#endif /* Pins_Arduino_h */

char* getErrorMsg(FRESULT errCode){
  switch (errCode)
  {
    case FR_OK: return "(0) Succeeded"; break;
    case FR_DISK_ERR: return "(1) A hard error occurred in the low level disk I/O layer"; break;
    case FR_INT_ERR: return "(2) Assertion failed"; break;
    case FR_NOT_READY: return "(3) The physical drive cannot work"; break;
    case FR_NO_FILE: return "(4) Could not find the file"; break;
    case FR_NO_PATH: return "(5) Could not find the path"; break;
    case FR_INVALID_NAME: return "(6) The path name format is invalid"; break;
    case FR_DENIED: return "(7) Access denied due to prohibited access or directory full"; break;
    case FR_EXIST: return "(8) Access denied due to prohibited access"; break;
    case FR_INVALID_OBJECT: return "(9) The file/directory object is invalid"; break;
    case FR_WRITE_PROTECTED: return "(10) The physical drive is write protected"; break;
    case FR_INVALID_DRIVE: return "(11) The logical drive number is invalid"; break;
    case FR_NOT_ENABLED: return "(12) The volume has no work area"; break;
    case FR_NO_FILESYSTEM: return "(13) There is no valid FAT volume"; break;
    case FR_MKFS_ABORTED: return "(14) The f_mkfs() aborted due to any problem"; break;
    case FR_TIMEOUT: return "(15) Could not get a grant to access the volume within defined period"; break;
    case FR_LOCKED: return "(16) The operation is rejected according to the file sharing policy"; break;
    case FR_NOT_ENOUGH_CORE: return "(17) LFN working buffer could not be allocated"; break;
    case FR_TOO_MANY_OPEN_FILES: return "(18) Number of open files > FF_FS_LOCK"; break;
    case FR_INVALID_PARAMETER: return "(19) Given parameter is invalid"; break;
    default:
      break;
  }
  return "Invalid error code";
}

static xQueueHandle gpio_evt_queue = NULL;
time_t lastMovement = 0;
RTC_DATA_ATTR uint32_t bumpCnt = 0;
RTC_DATA_ATTR bool isStopped = true;
RTC_DATA_ATTR uint16_t lastLatDeg;
RTC_DATA_ATTR uint32_t lastLatBil;
RTC_DATA_ATTR bool lastLatNeg;
RTC_DATA_ATTR uint16_t lastLngDeg;
RTC_DATA_ATTR uint32_t lastLngBil;
RTC_DATA_ATTR bool lastLngNeg;
RTC_DATA_ATTR uint32_t lastCourse;
RTC_DATA_ATTR uint32_t lastSpeed;
RTC_DATA_ATTR uint32_t lastAltitude;
const uint8_t numWakePins = 6;
const gpio_num_t wakePins[] = {GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_34};
RTC_DATA_ATTR uint8_t wakePinState = 0;
RTC_DATA_ATTR uint8_t curRate = 0;
RTC_DATA_ATTR uint8_t lastRate = 0;
RTC_DATA_ATTR char tripFName[35];
RTC_DATA_ATTR bool hibernate = false;
RTC_DATA_ATTR time_t lastDpTs;
time_t lastLocTs = 0;
bool gpsto = false;
bool buto = false;
bool sito = false;
bool boto = false;

static const char* pmpt1 = "    <Placemark>\n\
      <styleUrl>#SpeedPlacemark</styleUrl>\n\
      <Point>\n\
        <coordinates>";
static const char* pmpt2 = "</coordinates>\n\
      </Point>\n\
      <description>Speed ";
static const char* pmpt3 = "km/h\n\
RAM:";
static const char* pmpt4 = "\nBattery:";
static const char* pmpt5 = "</description>\n\
    </Placemark>\n\
";
extern const uint8_t trip_kml_start[] asm("_binary_trip_kml_start");
extern const uint8_t trip_kml_end[] asm("_binary_trip_kml_end");

TinyGPSPlus *gps;
esp_adc_cal_characteristics_t characteristics;
uint16_t timeout = 300;
uint32_t timeoutMicro = 300 * 1000000;

esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card = NULL;
const char mount_point[] = "/sdcard";

extern "C"
{
  void app_main(void);
}

struct dataPoint
{
  uint32_t ts;
  double lng;
  double lat;
  uint16_t speed;
  uint16_t course;
  uint16_t bat;
  uint32_t freeRam;
  uint32_t bumpCnt;
  uint32_t altitude;
};

struct trip
{
  uint32_t numNodes;
  uint32_t nodesAllocated;
  dataPoint **nodes;
  char fname[33];
  uint32_t lastExportedTs = 0;
};

uint32_t getBatteryVoltage()
{
  //uint32_t voltage;
  //esp_adc_cal_get_voltage(ADC_CHANNEL_7, &characteristics, &voltage);
  //return voltage * 1.5;
  return adc1_get_raw(ADC1_CHANNEL_7) / 4096.0 * 7445;
}

trip curTrip;

dataPoint *getCurDataPoint()
{
  ESP_LOGD(__FUNCTION__, "getCurDataPoint");
  time(&lastDpTs);
  return new dataPoint{
    ts : (uint32_t)lastDpTs,
    lng : gps->location.lng(),
    lat : gps->location.lat(),
    speed : (uint16_t)gps->speed.kmph(),
    course : (uint16_t)gps->course.deg(),
    bat : (uint16_t)getBatteryVoltage(),
    freeRam : heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
    bumpCnt : bumpCnt,
    altitude : (uint32_t)gps->altitude.meters()
  };
}

void addTripBlock()
{
  ESP_LOGD(__FUNCTION__, "addTripBlock");
  curTrip.nodes[curTrip.nodesAllocated] = (dataPoint *)malloc(sizeof(dataPoint) * TRIP_BLOCK_SIZE);
  curTrip.nodesAllocated += TRIP_BLOCK_SIZE;
}

void createTrip()
{
  ESP_LOGD(__FUNCTION__, "createTrip");
  curTrip.numNodes = 0;
  curTrip.nodesAllocated = 0;
  curTrip.nodes = (dataPoint **)malloc(sizeof(dataPoint *) * 10240);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)
    curTrip.fname[0] = 0;
  else
    strcpy(curTrip.fname, tripFName);
  curTrip.lastExportedTs = 0;
  addTripBlock();
}

bool initSDCard()
{
  ESP_LOGD(__FUNCTION__, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_miso = (gpio_num_t)PIN_NUM_MISO;
  slot_config.gpio_mosi = (gpio_num_t)PIN_NUM_MOSI;
  slot_config.gpio_sck = (gpio_num_t)PIN_NUM_CLK;
  slot_config.gpio_cs = (gpio_num_t)PIN_NUM_CS;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};
  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(__FUNCTION__, "Failed to mount filesystem. "
                             "If you want the card to be formatted, set format_if_mount_failed = true.");
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Failed to initialize the card (%s). "
                             "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return false;
  }

  ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
  if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
    sdmmc_card_print_info(stdout, card);

  f_mkdir("/converted");
  f_mkdir("/kml");
  f_mkdir("/sent");

  return true;
}

bool commitTrip(trip *trip)
{
  if ((trip == NULL) || (trip->numNodes <= 0))
  {
    return false;
  }
  struct tm timeinfo;
  bool printHeader = false;
  if (strlen(trip->fname) == 0)
  {
    printHeader = true;
    localtime_r((time_t *)&trip->nodes[0]->ts, &timeinfo);
    strftime(trip->fname, sizeof(trip->fname), "/sdcard/%Y-%m-%d_%H-%M-%S.csv", &timeinfo);
  }
  ESP_LOGI(__FUNCTION__, "Saving trip with %d nodes to %s", trip->numNodes, trip->fname);
  FILE *f = fopen(trip->fname, "a");
  if (f == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Failed to open %s for writing", trip->fname);
    return false;
  }
  dataPoint *dp;
  if (printHeader)
  {
    fprintf(f, "timestamp,longitude,latitude,speed,altitude,course,RAM,Battery,Bumps\n");
  }
  char dt[21];
  uint32_t cnt = 0;
  for (int idx = 0; idx < trip->numNodes; idx++)
  {
    dp = trip->nodes[idx];
    if (dp->ts > trip->lastExportedTs)
    {
      localtime_r((time_t *)&dp->ts, &timeinfo);
      strftime(dt, 21, "%Y/%m/%d %H:%M:%S", &timeinfo);
      fprintf(f, "%s,%3.7f,%3.7f,%d,%d,%d,%d,%d,%d\n", dt, dp->lng, dp->lat, dp->speed, dp->altitude, dp->course, dp->freeRam, dp->bat, bumpCnt);
      trip->lastExportedTs = dp->ts;
      cnt++;
    }
  }
  fclose(f);
  ESP_LOGD(__FUNCTION__, "%d nodes written", cnt);
  return true;
}

bool bakeKml(char *cvsFileName, char *kmlFileName)
{
  if (strlen(cvsFileName) > 1)
  {
    ESP_LOGD(__FUNCTION__, "Getting Facts from %s for %s", cvsFileName, kmlFileName);
    FILE *trp = fopen(cvsFileName, "r");
    if (trp == NULL) {
      ESP_LOGE(__FUNCTION__,"Cannot open source");
      return false;
    }
    int fc = 0;
    int bc = 0;
    uint32_t lineCount = 0;
    uint32_t firstLinePos = 0;
    uint32_t pos = 0;
    while ((fc = fgetc(trp)) != EOF)
    {
      if ((lineCount == 1) &&
          (firstLinePos == 0) &&
          (fc != 10) && (fc != 13))
      {
        firstLinePos = pos;
      }
      if ((fc == 10) || (fc == 13))
      {
        if ((bc != 10) && (bc != 13))
        {
          lineCount++;
        }
      }
      bc = fc;
      pos++;
    }
    lineCount--;

    ESP_LOGD(__FUNCTION__, "Baking KML from %d lines %d chars", lineCount, pos);
    FILE *kml = fopen(kmlFileName, "w");
    if (kml == NULL)
    {
      ESP_LOGE(__FUNCTION__, "Cannot open %s", kmlFileName);
      return false;
    }
    uint8_t chr = 0;
    uint8_t lnBuff[515];
    uint8_t *bkeyPos = NULL;
    uint8_t *bvalPos = NULL;
    uint8_t *evalPos = NULL;
    uint8_t prevChr = 0;
    char keyName[100];
    uint8_t *cchr = (uint8_t *)trip_kml_start;
    while (cchr < trip_kml_end)
    {
      chr = *cchr++;
      if ((bkeyPos == NULL) && (chr == '%'))
      {
        bkeyPos = cchr - 1;
        ESP_LOGV(__FUNCTION__, "bkey:%ul", (uint32_t)bkeyPos);
      }
      else
      {
        if (bkeyPos != NULL)
        {
          if ((prevChr == '^') ^ (chr == '^'))
          {
            if (chr == '%')
            {
              ESP_LOGV(__FUNCTION__, "ekey:NULL");
              bkeyPos = NULL;
              if ((bvalPos == NULL) && (evalPos == NULL))
              {
                bvalPos = cchr;
                ESP_LOGV(__FUNCTION__, "bval:%ul", (uint32_t)bvalPos);
              }
              else if ((bvalPos != NULL) && (evalPos == NULL))
              {
                evalPos = cchr - 4;
                ESP_LOGV(__FUNCTION__, "eval:%ul", (uint32_t)evalPos);
                if (evalPos > bvalPos)
                {
                  memcpy(keyName, bvalPos, evalPos - bvalPos + 1);
                  keyName[evalPos - bvalPos + 1] = 0;
                  ESP_LOGV(__FUNCTION__, "key:%s", keyName);
                  if (strcmp(keyName, "tripDate") == 0)
                  {
                    fprintf(kml, "%s", &cvsFileName[8]);
                    ESP_LOGV(__FUNCTION__, "val:%s", &cvsFileName[8]);
                  }
                  else if (strcmp(keyName, "tripNumNodes") == 0)
                  {
                    fprintf(kml, "%d", lineCount);
                    ESP_LOGV(__FUNCTION__, "val:%d", lineCount);
                  }
                  else if (strcmp(keyName, "tripPlacemarks") == 0)
                  {
                    lineCount = 0;
                    pos = 0;
                    uint8_t fldNo = 1;
                    fseek(trp, firstLinePos, SEEK_SET);
                    uint8_t sidx=0;
                    while ((fc = fgetc(trp)) != EOF)
                    {
                      if ((fc == 10) || (fc == 13))
                      {
                        if ((bc != 10) && (bc != 13))
                        {
                          lineCount++;
                          fldNo = 1;
                          sidx=0;
                        }
                      }
                      if ((fldNo >= 2) && (fldNo <=8))
                      {
                        if ((fldNo <= 3) || ((fldNo == 4) && (fc != ',')))
                        {
                          fputc(fc, kml);
                        }
                        if ((fldNo >= 7) && (fldNo <=8) && (fc != ','))
                        {
                          fputc(fc, kml);
                        }
                      }
                      if (fc == ',')
                      {
                        if (fldNo == 1){
                          fprintf(kml,pmpt1);
                        }
                        if (fldNo == 3){
                          fprintf(kml,pmpt2);
                        }
                        if (fldNo == 4){
                          fprintf(kml,pmpt3);
                        }
                        if (fldNo == 7){
                          fprintf(kml,pmpt4);
                        }
                        if (fldNo == 8){
                          fprintf(kml,pmpt5);
                        }
                        fldNo++;
                      }
                      bc = fc;
                      pos++;
                    }
                  }
                  else if (strcmp(keyName, "tripPoints") == 0)
                  {
                    lineCount = 0;
                    pos = 0;
                    uint8_t fldNo = 1;
                    fseek(trp, firstLinePos, SEEK_SET);
                    while ((fc = fgetc(trp)) != EOF)
                    {
                      if ((fc == 10) || (fc == 13))
                      {
                        if ((bc != 10) && (bc != 13))
                        {
                          lineCount++;
                          fldNo = 1;
                          fputc(fc, kml);
                        }
                      }
                      if ((fldNo >= 2) && (fldNo <= 4))
                      {
                        if ((fldNo < 4) || (fc != ','))
                        {
                          fputc(fc, kml);
                        }
                      }
                      if (fc == ',')
                      {
                        fldNo++;
                      }
                      bc = fc;
                      pos++;
                    }
                  }
                }
                else
                {
                  ESP_LOGV(__FUNCTION__, "bad eval pos bval:%ul eval:%ul", (uint32_t)bvalPos, (uint32_t)evalPos);
                }
                bvalPos = NULL;
                evalPos = NULL;
                bkeyPos = NULL;
              }
              else
              {
                ESP_LOGV(__FUNCTION__, "closing key bval:%ul eval:%ul", (uint32_t)bvalPos, (uint32_t)evalPos);
                bvalPos = NULL;
                evalPos = NULL;
                bkeyPos = NULL;
              }
            }
            else
            {
              bkeyPos++;
            }
          }
          else
          {
            bkeyPos = NULL;
            bvalPos = NULL;
            evalPos = NULL;
          }
        }
        else if (bvalPos == NULL)
        {
          if (chr > 0)
          {
            fputc(chr, kml);
          }
        }
      }
      prevChr = chr;
    }
    fclose(kml);
    fclose(trp);
    ESP_LOGD(__FUNCTION__, "Done baking KML");
    return true;
  }
  return false;
}

static bool moveFile(char* src, char* dest){
  FRESULT res;
  FILE* srcF = fopen(src,"r");
  if (srcF != 0) {
    FILE* destF = fopen(dest,"w");
    if (destF != NULL) {
      int ch=0;
      while ((ch=fgetc(srcF))!=EOF){
        fputc(ch,destF);
      }
      fclose(destF);
      fclose(srcF);
      if ((res=f_unlink(&src[8]))==0){
        ESP_LOGD(__FUNCTION__,"moved %s to %s",src,dest);
        return true;
      } else {
        ESP_LOGE(__FUNCTION__,"failed in deleting %s %s",src,getErrorMsg(res));
      }
    } else {
      ESP_LOGE(__FUNCTION__,"Cannot open dest %s",dest);
    }
  } else {
    ESP_LOGE(__FUNCTION__,"Cannot open source %s",src);
  }
  return false;
}

static void commitTripToDisk(void *kml)
{
  if (initSDCard())
  {
    char *kFName = (char *)malloc(350);
    char *cFName = (char *)malloc(350);
    char *csvs = (char *)malloc(300 * 10);

    if (commitTrip(&curTrip))
    {
      if (kml)
      {
        sprintf(kFName, "/sdcard/kml/%s.kml", &curTrip.fname[8]);
        if (bakeKml(curTrip.fname, kFName))
        {
          sprintf(kFName, "/sdcard/converted/%s", &curTrip.fname[8]);
          if (moveFile(curTrip.fname, kFName))
          {
            ESP_LOGD(__FUNCTION__, "Moved %s to %s.", cFName, kFName);
          } else
          {
            ESP_LOGE(__FUNCTION__, "Failed moving %s to %s", cFName, kFName);
          }
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Failed baking %s from %s", cFName, cFName);
        }
      }
    }
    if (kml && (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED))
    {
      ESP_LOGV(__FUNCTION__, "Opening sdcard folder");
      FF_DIR theFolder;
      uint8_t numCsvs = 0;
      do
      {
        numCsvs = 0;
        if (f_opendir(&theFolder, "/") == FR_OK)
        {
          ESP_LOGV(__FUNCTION__, "reading sdcard files");
          FILINFO fi;

          char *curCsv = csvs;
          while (f_readdir(&theFolder, &fi) == FR_OK)
          {
            if (strlen(fi.fname) == 0)
            {
              break;
            }
            ESP_LOGV(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);
            if (((fi.fattrib & AM_DIR) == 0) &&
                (strstr(fi.fname, ".~lock") == NULL) &&
                strstr(fi.fname, ".csv") &&
                (strstr(fi.fname, ".kml") == NULL))
            {
              ESP_LOGV(__FUNCTION__, "Stacking %s", fi.fname);
              strcpy(curCsv, fi.fname);
              curCsv += strlen(fi.fname) + 1;
              if (++numCsvs >= 10)
              {
                break;
              }
            }
          }
          f_closedir(&theFolder);
          if (numCsvs > 0)
          {
            curCsv = csvs;
            numCsvs = numCsvs > 10 ? 10 : numCsvs;
            ESP_LOGD(__FUNCTION__, "Parsing %d files", numCsvs);
            for (uint8_t idx = 0; idx < numCsvs; idx++)
            {
              sprintf(cFName, "/sdcard/%s", curCsv);
              sprintf(kFName, "/sdcard/kml/%s.kml", curCsv);
              if (bakeKml(cFName, kFName))
              {
                sprintf(kFName, "/sdcard/converted/%s", curCsv);
                if (strcmp(curTrip.fname,cFName) < 0){
                  if (moveFile(cFName, kFName))
                  {
                    ESP_LOGD(__FUNCTION__, "Moved %s to %s", cFName, kFName);
                  }
                  else
                  {
                    ESP_LOGE(__FUNCTION__, "Failed moving %s to %s", cFName, kFName);
                  }
                }
              } else {
                ESP_LOGD(__FUNCTION__,"Skipping active trip %s",cFName);
              }
              curCsv += strlen(curCsv) + 1;
            }
          }
        }
      }while (numCsvs > 0);
    }
    if (esp_vfs_fat_sdmmc_unmount() == ESP_OK)
    {
      ESP_LOGD(__FUNCTION__, "Unmounted SD Card");
    } else {
      ESP_LOGE(__FUNCTION__, "Failed to unmount SD Card");
    }
    free(kFName);
    free(cFName);
    free(csvs);
  }
}

void addDataPoint()
{
  dataPoint *curNode = getCurDataPoint();
  ESP_LOGD(__FUNCTION__, "addDataPoint %d", curTrip.numNodes);
  curTrip.nodes[curTrip.numNodes++] = curNode;
  ESP_LOGD(__FUNCTION__, "lng:%3.7f,lat:%3.7f,speed:%d, course:%d,bat:%d,freeRam:%d,bumps:%d,alt:%d",
           curNode->lng,
           curNode->lat,
           curNode->speed,
           curNode->course,
           curNode->bat,
           curNode->freeRam,
           bumpCnt,
           curNode->altitude);

  if (curTrip.numNodes == curTrip.nodesAllocated)
  {
    addTripBlock();
  }
}

static void flash(void *pvParameters)
{
  uint8_t curIdx = gps->numRunners++;
  gps->runners[curIdx] = (BaseType_t *)xTaskGetCurrentTaskHandle();
  uint32_t flashes = (uint32_t)pvParameters;
  ESP_LOGV(__FUNCTION__, "Runners %d is flash on handle %p with %d flashes", gps->numRunners, gps->runners[curIdx], flashes);

  for (uint8_t idx = 0; idx < flashes; idx++)
  {
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  gps->runners[curIdx] = NULL;
  xSemaphoreGive(gps->runnerSemaphore);
  vTaskDelete(NULL);
}

void Hibernate()
{
  commitTripToDisk((void *)true);
  ESP_LOGI(__FUNCTION__, "Deep Sleeping %d = %d", bumpCnt, gps->numRunners);
  uint64_t ext_wakeup_pin_mask = 0;
  int curLvl = 0;
  for (int idx = 0; idx < numWakePins; idx++)
  {
    curLvl = gpio_get_level(wakePins[idx]);
    ESP_LOGD(__FUNCTION__, "Pin %d is %d", wakePins[idx], curLvl);
    if (curLvl == 0)
    {
      ESP_ERROR_CHECK(rtc_gpio_pulldown_en(wakePins[idx]));
      ext_wakeup_pin_mask |= (1ULL << wakePins[idx]);
    }
  }
  if (ext_wakeup_pin_mask != 0)
  {
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_mask, ESP_EXT1_WAKEUP_ANY_HIGH));
  }
  else
  {
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(wakePins[numWakePins - 1], curLvl == 0 ? 1 : 0));
  }
  gpio_set_level(gps->enpin, 0);
  gpio_hold_en(gps->enpin);

  //gpio_deep_sleep_hold_en();
  lastSpeed = 0;
  lastCourse = 0;
  lastAltitude = 0;
  lastLatDeg = 0;
  lastLatBil = 0;
  lastLngDeg = 0;
  lastLngBil = 0;
  lastDpTs = 0;
  bumpCnt = 0;
  lastRate = 0;
  curTrip.fname[0] = 0;
  curTrip.lastExportedTs = 0;
  curTrip.nodesAllocated = 0;
  curTrip.numNodes = 0;
  hibernate = true;
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  esp_deep_sleep_start();
}

static void gpsEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  time_t now;
  switch (id)
  {
  case TinyGPSPlus::gpsEvent::locationChanged:
    ESP_LOGI(__FUNCTION__, "Location: %3.6f, %3.6f, %3.6f, %4.2f", gps->location.lat(), gps->location.lng(), gps->speed.kmph(), gps->altitude.meters());
    time(&now);
    lastLocTs = now;
    if (gps->gpspingTask == NULL)
    {
      bool allDone = false;
      //xTaskCreate(flash, "flashy", 2048, (void *)curRate, tskIDLE_PRIORITY+5, NULL);
      uint8_t newRunners = gps->numRunners;
      esp_event_loop_run(gps->loop_handle, 50 / portTICK_PERIOD_MS);
      ESP_LOGV(__FUNCTION__, "Waiting for runners %d", gps->numRunners);
      while ((gps->numRunners > 0) && !allDone && xSemaphoreTake(gps->runnerSemaphore, 1000 / portTICK_PERIOD_MS))
      {
        newRunners = 0;
        for (uint8_t runner = 0; runner < gps->numRunners; runner++)
        {
          if (gps->runners[runner] != NULL)
          {
            ESP_LOGV(__FUNCTION__, "Runners %d is still going", gps->numRunners);
            newRunners++;
          }
        }
        allDone = (newRunners == 0);
      }
      gps->numRunners = 0;
      ESP_LOGV(__FUNCTION__, "Done for runners %d", gps->numRunners);

      if (curRate >= 10)
      {
        esp_sleep_enable_timer_wakeup(gps->getSleepTime() * 1000000);
        lastSpeed = gps->speed.value();
        lastAltitude = gps->altitude.value();
        lastCourse = gps->course.value();
        lastLatDeg = gps->location.rawLat().deg;
        lastLatBil = gps->location.rawLat().billionths;
        lastLatNeg = gps->location.rawLat().negative;
        lastLngDeg = gps->location.rawLng().deg;
        lastLngBil = gps->location.rawLng().billionths;
        lastLngNeg = gps->location.rawLng().negative;
        lastRate = gps->curFreqIdx;
        timeval tv;
        gettimeofday(&tv, NULL);
        if ((curTrip.numNodes > 0) &&
            (curTrip.nodes[curTrip.numNodes - 1] != NULL) &&
            (curTrip.lastExportedTs < curTrip.nodes[curTrip.numNodes - 1]->ts))
          commitTripToDisk((void *)false);
        strcpy(tripFName, curTrip.fname);
        gps->gpsPause();
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
        esp_deep_sleep_start();
      }
      else
      {
        ESP_LOGV(__FUNCTION__, "Mr sandman, give me a dream of %d seconds", (int)curRate);
        xTaskCreate(gps->gotoSleep, "gotosleep", 2048, gps, tskIDLE_PRIORITY, &gps->gpspingTask);
        //gps->gotoSleep(gps);
      }
    }
    else
    {
      ESP_LOGV(__FUNCTION__, "Already Sleeping %d", (int)gps->gpspingTask);
    }
    break;
  case TinyGPSPlus::gpsEvent::systimeChanged:
    char strftime_buf[64];
    struct tm timeinfo;
    time(&now);

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(__FUNCTION__, "System Time: %s", strftime_buf);
    break;
  case TinyGPSPlus::gpsEvent::significantDistanceChange:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Distance Diff: %f", *((double *)event_data));
    xTaskCreate(flash, "flashy", 2048, (void *)1, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::significantSpeedChange:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Speed Diff: %f %f", gps->speed.kmph(), *((double *)event_data));
    xTaskCreate(flash, "flashy", 2048, (void *)2, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::significantCourseChange:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Course Diff: %f %f", gps->course.deg(), *((double *)event_data));
    xTaskCreate(flash, "flashy", 2048, (void *)3, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::rateChanged:
    ESP_LOGI(__FUNCTION__, "Rate Change: %d", *((uint8_t *)event_data));
    curRate = *((uint8_t *)event_data);
    //xTaskCreate(flash, "flashy", 2048, (void *)curRate, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::msg:
    time(&now);
    boto = (esp_timer_get_time() > timeoutMicro);
    if ((lastLocTs > 0) && (now - lastLocTs > timeout))
    {
      if (!gpsto)
      {
        gpsto = true;
        ESP_LOGI(__FUNCTION__, "Timeout on GPS Location");
      }
    }
    else
    {
      gpsto = false;
    }
    if (((lastMovement > 0) && (now - lastMovement > timeout)) || ((lastMovement == 0) && boto))
    {
      if (!buto)
      {
        buto = true;
        ESP_LOGI(__FUNCTION__, "Timeout on bumps");
      }
    }
    else
    {
      if (buto)
      {
        buto = false;
        ESP_LOGI(__FUNCTION__, "Resumed bumps");
      }
    }
    if (boto && (lastLocTs == 0))
    {
      if (!sito)
      {
        sito = true;
        ESP_LOGI(__FUNCTION__, "Timeout on signal");
      }
    }
    else
    {
      sito = false;
    }

    if ((gpsto || sito) && buto)
    {
      ESP_LOGI(__FUNCTION__, "Lost GPS Signal and no bumps gps:%d sig:%d", gpsto, sito);
      Hibernate();
    }

    if ((lastDpTs > 0) && ((now - lastDpTs) > timeout))
    {
      ESP_LOGI(__FUNCTION__, "Timeout on data %ld", lastDpTs);
      Hibernate();
    }

    ESP_LOGV(__FUNCTION__, "msg: %s", (char *)event_data);
    break;
  case TinyGPSPlus::gpsEvent::sleeping:
    ESP_LOGD(__FUNCTION__, "Sleeping");
    break;
  case TinyGPSPlus::gpsEvent::go:
    ESP_LOGI(__FUNCTION__, "Go");
    isStopped = false;
    break;
  case TinyGPSPlus::gpsEvent::stop:
    ESP_LOGI(__FUNCTION__, "Stop");
    isStopped = true;
    break;
  case TinyGPSPlus::gpsEvent::wakingup:
    ESP_LOGD(__FUNCTION__, "Waking");
    break;
  }
}

static void gpio_isr_handler(void *arg)
{
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void pollWakePins(void *arg)
{
  uint32_t io_num;
  for (;;)
  {
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
    {
      uint8_t curLvl = gpio_get_level((gpio_num_t)io_num);
      for (uint8_t idx = 0; idx < numWakePins; idx++)
      {
        if (wakePins[idx] == io_num)
        {
          uint8_t lastLvl = (wakePinState >> (idx + 1)) & 1U;
          if (lastLvl != curLvl)
          {
            ESP_LOGV(__FUNCTION__, "GPIO[%d] intr, val: %d\n", io_num, curLvl);
            bumpCnt++;
            time(&lastMovement);
            wakePinState = (wakePinState & ~(1U << (idx + 1))) | (curLvl << (idx + 1));
            break;
          }
        }
      }
    }
  }
}

void configureMotionDetector()
{
  ESP_LOGV(__FUNCTION__, "Configuring Motion Detection");
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  io_conf.pin_bit_mask = ((1ULL << GPIO_NUM_27) |
                          (1ULL << GPIO_NUM_26) |
                          (1ULL << GPIO_NUM_25) |
                          (1ULL << GPIO_NUM_33) |
                          (1ULL << GPIO_NUM_32) |
                          (1ULL << GPIO_NUM_34));
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
  gpio_config(&io_conf);
  ESP_LOGV(__FUNCTION__, "Pins configured");
  xTaskCreate(pollWakePins, "pollWakePins", 2048, NULL, tskIDLE_PRIORITY, NULL);
  gpio_install_isr_service(0);
  ESP_LOGV(__FUNCTION__, "ISR Service Started");

  for (int idx = 0; idx < numWakePins; idx++)
  {
    ESP_ERROR_CHECK(gpio_isr_handler_add(wakePins[idx], gpio_isr_handler, (void *)wakePins[idx]));
    ESP_LOGV(__FUNCTION__, "Pin %d: RTC:%d", wakePins[idx], rtc_gpio_is_valid_gpio(wakePins[idx]));
    ESP_LOGV(__FUNCTION__, "Pin %d: Level:%d", wakePins[idx], gpio_get_level(wakePins[idx]));
  }
  ESP_LOGV(__FUNCTION__, "Pin %d: RTC:%d", GPIO_NUM_13, rtc_gpio_is_valid_gpio(GPIO_NUM_13));
  ESP_LOGV(__FUNCTION__, "Pin %d: Level:%d", GPIO_NUM_13, gpio_get_level(GPIO_NUM_13));
  ESP_LOGD(__FUNCTION__, "Motion Detection Ready");
}

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    ESP_LOGI(__FUNCTION__, "Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    ESP_LOGI(__FUNCTION__, "Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    ESP_LOGI(__FUNCTION__, "Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    ESP_LOGI(__FUNCTION__, "Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    ESP_LOGI(__FUNCTION__, "Wakeup caused by ULP program");
    break;
  default:
    ESP_LOGI(__FUNCTION__, "Wakeup was not caused by deep sleep");
    break;
  }
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT1:
  {
    uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_pin_mask != 0)
    {
      int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
      ESP_LOGD(__FUNCTION__, "Wake up from GPIO %d\n", pin);
      ESP_LOGD(__FUNCTION__, "level %d\n", gpio_get_level((gpio_num_t)pin));
    }
    else
    {
      ESP_LOGD(__FUNCTION__, "Wake up from GPIO\n");
    }
    break;
  }
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
  {
    ESP_LOGD(__FUNCTION__, "Wake up from touch on pad %d\n", esp_sleep_get_touchpad_wakeup_status());
    break;
  }
  case ESP_SLEEP_WAKEUP_UNDEFINED:
  default:
    break;
  }
}

void app_main(void)
{
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)
  {
    lastLatDeg = 0;
    lastLatBil = 0;
    lastLngDeg = 0;
    lastLngBil = 0;
    lastAltitude = 0;
    lastCourse = 0;
    lastSpeed = 0;
    lastRate = 0;
    bumpCnt = 0;
    commitTripToDisk((void *)1);
  } else {
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
  }
  print_wakeup_reason();
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
  {
    if (hibernate)
    {
      ESP_LOGD(__FUNCTION__, "re-sleeping");
      Hibernate();
    }
  }
  ESP_LOGD(__FUNCTION__, "Starting bumps:%d, lastMovement:%d", bumpCnt, CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);

  createTrip();
  RawDegrees lat;
  RawDegrees lng;
  lat.deg = lastLatDeg;
  lat.billionths = lastLatBil;
  lat.negative = lastLatNeg;
  lng.deg = lastLngDeg;
  lng.billionths = lastLngBil;
  lng.negative = lastLngNeg;
  gps = new TinyGPSPlus(GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_13, lastRate, lat, lng, lastCourse, lastSpeed, lastAltitude);

  gpio_reset_pin(BLINK_GPIO);
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(BLINK_GPIO, 1);
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1163, &characteristics);

  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::msg, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::go, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::stop, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::sleeping, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::wakingup, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::rateChanged, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::systimeChanged, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::locationChanged, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::significantCourseChange, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::significantDistanceChange, gpsEvent, &gps));
  ESP_ERROR_CHECK(esp_event_handler_register_with(gps->loop_handle, gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::significantSpeedChange, gpsEvent, &gps));
  configureMotionDetector();
  //ESP_ERROR_CHECK(xTaskCreate(commitTripToDisk,"commitTripToDisk",4096,(void*)1,tskIDLE_PRIORITY,NULL));
}
