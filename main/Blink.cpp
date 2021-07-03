#include <stdio.h>
#include <string.h>
#include "../build/config/sdkconfig.h"
#include "utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
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
#include "TinyGPS++.h"
#include <esp_event.h>
#include "mdctor/ulp.h"
#include "../components/wifi/station.h"
#include "../components/esp_littlefs/include/esp_littlefs.h"
#include "bootloader_random.h"
#include "../components/eventmgr/eventmgr.h"
#include "../components/pins/pins.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define BLINK_GPIO GPIO_NUM_5
#define GPS_EN_PIN GPIO_NUM_13
#define NUM_VOLT_CYCLE 20

#define TRIP_BLOCK_SIZE 255

static xQueueHandle gpio_evt_queue = NULL;
static trip curTrip;
time_t now = 0;
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
const uint8_t numWakePins = 3;
const gpio_num_t wakePins[] = {GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_34};
RTC_DATA_ATTR uint8_t wakePinState = 0;
RTC_DATA_ATTR uint8_t lastRate = 0;
RTC_DATA_ATTR char tripFName[35];
RTC_DATA_ATTR bool hibernate = false;
RTC_DATA_ATTR time_t lastDpTs;
RTC_DATA_ATTR poiState_t lastPoiState;
time_t lastLocTs = 0;
bool gpsto = false;
bool buto = false;
bool sito = false;
bool boto = false;
bool dto = false;
float batLvls[NUM_VOLT_CYCLE];
uint8_t batSmplCnt = NUM_VOLT_CYCLE + 1;
bool balFullSet = false;
uint64_t sleepTime = 0;

static const char *pmpt1 = "    <Placemark>\n\
      <styleUrl>#SpeedPlacemark</styleUrl>\n\
      <Point>\n\
        <coordinates>";
static const char *pmpt2 = "</coordinates>\n\
      </Point>\n\
      <description>";
static const char *pmpt3 = "km/h\n\
RAM:";
static const char *pmpt4 = "\nBattery:";
static const char *pmpt5 = "</description>\n\
    </Placemark>\n\
";
extern const uint8_t trip_kml_start[] asm("_binary_trip_kml_start");
extern const uint8_t trip_kml_end[] asm("_binary_trip_kml_end");

TinyGPSPlus *gps = NULL;
esp_adc_cal_characteristics_t characteristics;
uint16_t timeout = GPS_TIMEOUT;
uint32_t timeoutMicro = GPS_TIMEOUT * 1000000;

TaskHandle_t hybernator = NULL;

extern "C"
{
  void app_main(void);
}

trip *getActiveTrip()
{
  return &curTrip;
}

float getBatteryVoltage();
void sampleBatteryVoltage()
{
  if (batSmplCnt >= NUM_VOLT_CYCLE)
  {
    balFullSet = batSmplCnt == NUM_VOLT_CYCLE;
    batSmplCnt = 0;
    ESP_LOGV(__FUNCTION__, "Voltage:%f", getBatteryVoltage());
  }

  uint32_t voltage = 0;
  uint32_t tmp;
  uint32_t cnt = 0;
  esp_err_t ret;
  adc_power_acquire();
  for (int idx = 0; idx < 10; idx++)
  {
    if ((ret=esp_adc_cal_get_voltage(ADC_CHANNEL_7, &characteristics, &tmp))==ESP_OK){
      voltage += (tmp*2.40584138288);
      cnt++;
    } else {
      ESP_LOGW(__FUNCTION__,"Error getting voltage %s",esp_err_to_name(ret));
    }
  }
  adc_power_release();
  if (cnt > 0)
    batLvls[batSmplCnt++] = (voltage / cnt);
}

float getBatteryVoltage()
{
  if (batSmplCnt >= NUM_VOLT_CYCLE)
  {
    sampleBatteryVoltage();
  }

  float voltage = 0;
  uint8_t uloop = balFullSet ? NUM_VOLT_CYCLE : batSmplCnt;

  for (uint8_t idx = 0; idx < uloop; idx++)
  {
    voltage += batLvls[idx];
  }
  return uloop > 0 ? voltage / uloop : 0;
}

dataPoint *getCurDataPoint()
{
  time(&lastDpTs);
  if (lastDpTs < 10000) {
    return NULL;
  }
  dto = false;
  return new dataPoint{
    ts : (uint32_t)lastDpTs,
    lng : gps->location.lng(),
    lat : gps->location.lat(),
    speed : (uint16_t)gps->speed.kmph(),
    course : (uint16_t)gps->course.deg(),
    bat : getBatteryVoltage(),
    freeRam : heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
    bumpCnt : bumpCnt,
    altitude : (uint32_t)gps->altitude.meters()
  };
}

void addTripBlock()
{
  ESP_LOGD(__FUNCTION__, "addTripBlock");
  curTrip.nodes[curTrip.nodesAllocated] = (dataPoint *)dmalloc(sizeof(dataPoint) * TRIP_BLOCK_SIZE);
  curTrip.nodesAllocated += TRIP_BLOCK_SIZE;
}

void createTrip()
{
  ESP_LOGD(__FUNCTION__, "createTrip");
  curTrip.numNodes = 0;
  curTrip.nodesAllocated = 0;
  curTrip.nodes = (dataPoint **)dmalloc(sizeof(dataPoint *) * 10240);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)
    curTrip.fname[0] = 0;
  else
    strcpy(curTrip.fname, tripFName);
  curTrip.lastExportedTs = 0;
  addTripBlock();
}

bool commitTrip(trip *trip)
{
  if ((trip == NULL) || (trip->numNodes <= 0))
  {
    ESP_LOGW(__FUNCTION__, "commitTrip called without an active trip");
    return false;
  }
  struct tm timeinfo;
  bool printHeader = false;
  if (strlen(trip->fname) == 0)
  {
    printHeader = true;
    localtime_r((time_t *)&trip->nodes[0]->ts, &timeinfo);
    strftime(trip->fname, sizeof(trip->fname), "/lfs/csv/%Y-%m-%d_%H-%M-%S.csv", &timeinfo);
  }
  ESP_LOGD(__FUNCTION__, "Saving trip with %d nodes to %s %s", trip->numNodes, trip->fname, curTrip.fname);
  FILE *f = fOpen(trip->fname, "a");
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
    localtime_r((time_t *)&dp->ts, &timeinfo);
    strftime(dt, 21, "%Y/%m/%d %H:%M:%S", &timeinfo);
    fprintf(f, "%s,%3.7f,%3.7f,%d,%d,%d,%d,%f,%d\n", dt, dp->lng, dp->lat, dp->speed, dp->altitude, dp->course, dp->freeRam, dp->bat, bumpCnt);
    trip->lastExportedTs = dp->ts;
    cnt++;
    free(dp);
    trip->numNodes=0;
  }
  fClose(f);
  ESP_LOGD(__FUNCTION__, "%d nodes written", cnt);
  return true;
}

bool bakeKml(char *cvsFileName, char *kmlFileName)
{
  if (strlen(cvsFileName) > 1)
  {
    ESP_LOGV(__FUNCTION__, "Getting Facts from %s for %s", cvsFileName, kmlFileName);
    FILE *trp = fopen(cvsFileName, "r", true);
    if (trp == NULL)
    {
      ESP_LOGE(__FUNCTION__, "Cannot open source");
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

    if (lineCount <= 0)
    {
      ESP_LOGE(__FUNCTION__, "Nothing in this trip");
      fClose(trp);
      return true;
    }
    ESP_LOGD(__FUNCTION__, "Baking KML from %s %d lines %d chars", kmlFileName, lineCount, pos);
    FILE *kml = fopen(kmlFileName, "w", true);
    if (kml == NULL)
    {
      ESP_LOGE(__FUNCTION__, "Cannot open %s", kmlFileName);
      return false;
    }
    ESP_LOGV(__FUNCTION__, "Opened %s", kmlFileName);
    uint8_t chr = 0;
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
                    char ts[30];
                    char speed[10];
                    memset(speed, 0, 10);
                    memset(ts, 0, 30);
                    speed[0] = 0;
                    uint8_t speedLen = 0;
                    uint8_t tsLen = 0;
                    ts[0] = 0;
                    while ((fc = fgetc(trp)) != EOF)
                    {
                      if ((fc == 10) || (fc == 13))
                      {
                        if ((bc != 10) && (bc != 13))
                        {
                          lineCount++;
                          fldNo = 1;
                          tsLen = 0;
                          ts[0] = 0;
                          speedLen = 0;
                          speed[0] = 0;
                        }
                      }
                      if (fc == ',')
                      //fprintf(f, "timestamp,longitude,latitude,speed,altitude,course,RAM,Battery,Bumps\n");
                      {
                        if (fldNo == 1)
                        {
                          fprintf(kml, pmpt1);
                          ts[tsLen] = 0;
                        }
                        if ((fldNo >= 2) && (fldNo <= 3))
                        {
                          fputc(fc, kml);
                        }

                        if (fldNo == 4)
                        {
                          fprintf(kml, pmpt2);
                          fprintf(kml, ts);
                          fprintf(kml, "\nSpeed:");
                          fprintf(kml, speed);
                          fprintf(kml, pmpt3);
                          speed[speedLen] = 0;
                        }
                        if (fldNo == 7)
                        {
                          fprintf(kml, pmpt4);
                        }
                        if (fldNo == 8)
                        {
                          fprintf(kml, pmpt5);
                        }
                        fldNo++;
                      }
                      else
                      {
                        if (fldNo == 1)
                        {
                          ts[tsLen++] = fc;
                        }
                        if ((fldNo >= 2) && (fldNo <= 4))
                        {
                          fputc(fc, kml);
                        }
                        if ((fldNo >= 7) && (fldNo <= 8))
                        {
                          fputc(fc, kml);
                        }
                        if (fldNo == 4)
                        {
                          speed[speedLen++] = fc;
                        }
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
    fClose(kml);
    fClose(trp);
    ESP_LOGV(__FUNCTION__, "Done baking KML");
    return true;
  }
  return false;
}

void commitTripToDisk(void *param)
{
  uint32_t theBits = (uint32_t)param;
  if (initSPISDCard())
  {
    if (xEventGroupWaitBits(app_eg, app_bits_t::COMMITTING_TRIPS, pdFALSE, pdTRUE, 10 / portTICK_RATE_MS) == ESP_OK)
    {
      ESP_LOGD(__FUNCTION__, "Waiting for other trip baker");
      if (xEventGroupWaitBits(app_eg, app_bits_t::TRIPS_COMMITTED, pdFALSE, pdTRUE, 5000 / portTICK_RATE_MS) != ESP_OK)
      {
        ESP_LOGW(__FUNCTION__, "Timed out, giving up");
        deinitSPISDCard();
        if (theBits & BIT3)
        {
          vTaskDelete(NULL);
        }
        return;
      }
      ESP_LOGD(__FUNCTION__, "Done waiting for other trip baker");
    }
    xEventGroupSetBits(app_eg, app_bits_t::COMMITTING_TRIPS);
    xEventGroupClearBits(app_eg, app_bits_t::TRIPS_COMMITTED);
    char *kFName = (char *)dmalloc(350);
    char *cFName = (char *)dmalloc(350);

    if (commitTrip(&curTrip) && (theBits & BIT1))
    {
      sprintf(kFName, "%s/kml/%s.kml", AppConfig::GetActiveStorage(), &curTrip.fname[8]);
      char *theChar;
      for (theChar = kFName; *theChar != 0; theChar++)
      {
        if (*theChar == '_')
        {
          *theChar = '/';
          break;
        }
        if (*theChar == '-')
        {
          *theChar = '/';
        }
      }
      sprintf(theChar + 1, "%s.kml", &curTrip.fname[8]);

      if (bakeKml(curTrip.fname, kFName))
      {
        sprintf(kFName, "%s/converted/%s", AppConfig::GetActiveStorage(), &curTrip.fname[8]);
        if (moveFile(curTrip.fname, kFName))
        {
          ESP_LOGD(__FUNCTION__, "Moved %s to %s.", curTrip.fname, kFName);
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Failed moving %s to %s", curTrip.fname, kFName);
        }
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "Failed baking %s from %s", curTrip.fname, cFName);
      }
    }
    if (theBits & BIT2)
    {
      ESP_LOGV(__FUNCTION__, "Opening csv folder");
      DIR *theFolder;
      struct dirent *fi;
      if ((theFolder = opendir("/lfs/csv")) != NULL)
      {
        char *kFName = (char *)dmalloc(300);
        char *cFName = (char *)dmalloc(300);
        while ((fi = readdir(theFolder)) != NULL)
        {
          if (strlen(fi->d_name) == 0)
          {
            continue;
          }
          if (fi->d_type == DT_REG)
          {
            sprintf(kFName, "%s/kml/%s.kml", AppConfig::GetActiveStorage(), fi->d_name);
            for (char *theChar = kFName;
                 *theChar != 0;
                 theChar++)
            {
              if (*theChar == '_')
              {
                *theChar = '/';
                break;
              }
              if (*theChar == '-')
              {
                *theChar = '/';
              }
            }
            sprintf(cFName, "/lfs/csv/%s", fi->d_name);
            if (bakeKml(cFName, kFName))
            {
              sprintf(kFName, "%s/converted/%s", AppConfig::GetActiveStorage(), fi->d_name);
              if (strcmp(curTrip.fname, cFName) < 0)
              {
                if (moveFile(cFName, kFName))
                {
                  ESP_LOGD(__FUNCTION__, "Moved %s to %s", cFName, kFName);
                }
                else
                {
                  ESP_LOGE(__FUNCTION__, "Failed moving %s to %s", cFName, kFName);
                }
              }
            }
            else
            {
              ESP_LOGD(__FUNCTION__, "Failed in baking %s", cFName);
            }
          }
        }
        ldfree(kFName);
        ldfree(cFName);
        closedir(theFolder);
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "Failed to open cvss");
      }
    }
    deinitSPISDCard();
  }
  xEventGroupSetBits(app_eg, app_bits_t::TRIPS_COMMITTED);
  xEventGroupClearBits(app_eg, app_bits_t::COMMITTING_TRIPS);
  if (theBits & BIT3)
    vTaskDelete(NULL);
}

void addDataPoint()
{
  dataPoint *curNode = getCurDataPoint();
  if ((curNode == NULL) || (curNode->lng == 0.0)) {
    ESP_LOGW(__FUNCTION__,"Skipping save of no point pr time data");
    return;
  }
  ESP_LOGD(__FUNCTION__, "addDataPoint %d", curTrip.numNodes);
  curTrip.nodes[curTrip.numNodes++] = curNode;
  ESP_LOGD(__FUNCTION__, "lng:%3.7f,lat:%3.7f,speed:%d, course:%d,bat:%f,freeRam:%d,bumps:%d,alt:%d",
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

  if (curTrip.numNodes % 10 == 0)
  {
    if (gps != NULL)
    {
      xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void *)(BIT3), tskIDLE_PRIORITY, NULL);
    }
  }
}

void doHibernate(void *param)
{
  if (gps != NULL)
  {
    addDataPoint();
    ESP_LOGD(__FUNCTION__, "Deep Sleeping %d", bumpCnt);
    if (gps != NULL)
    {
      commitTripToDisk((void *)0);
    }
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
      ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(wakePins[0], 0));
    }
    gpio_set_level(gps->enPin(), 0);
    gpio_hold_en(gps->enPin());

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
    lastPoiState = poiState_t::unknown;
    curTrip.fname[0] = 0;
    curTrip.lastExportedTs = 0;
    curTrip.nodesAllocated = 0;
    curTrip.numNodes = 0;
    hibernate = true;
    ESP_LOGD(__FUNCTION__, "Hybernating");

    //xTaskCreate(flash, "flashy", 2048, (void *)10, tskIDLE_PRIORITY, NULL);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF));
    adc_power_off();
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF));
    esp_deep_sleep_start();
  }
}

void Hibernate()
{
  if ((xEventGroupGetBits(gps->eg) & TinyGPSPlus::gpsEvent::gpsRunning) && 
      (getBatteryVoltage() != 0.0))
  {
    xEventGroupClearBits(gps->eg, TinyGPSPlus::gpsEvent::gpsRunning);
    xEventGroupSetBits(gps->eg, TinyGPSPlus::gpsEvent::gpsStopped);
    xTaskCreate(doHibernate, "doHibernate", 8192, NULL, tskIDLE_PRIORITY, NULL);
  }
}

static void gpsEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  time(&now);
  sampleBatteryVoltage();
  if ((id != TinyGPSPlus::gpsEvent::msg) && (id != TinyGPSPlus::gpsEvent::locationChanged)){
    AppConfig::SignalStateChange(state_change_t::GPS);
    ESP_LOGV(__FUNCTION__,"gps:%d",id);
  }
  switch (id)
  {
  case TinyGPSPlus::gpsEvent::locationChanged:
    ESP_LOGV(__FUNCTION__, "Location: %3.6f, %3.6f, %3.6f, %4.2f", gps->location.lat(), gps->location.lng(), gps->speed.kmph(), gps->altitude.meters());
    lastLocTs = now;
    break;
  case TinyGPSPlus::gpsEvent::systimeChanged:
    char strftime_buf[64];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    time(&lastDpTs);
    ESP_LOGI(__FUNCTION__, "System Time: %s", strftime_buf);
    break;
  case TinyGPSPlus::gpsEvent::significantDistanceChange:
    addDataPoint();
    ESP_LOGD(__FUNCTION__, "Distance Diff: %f", *((double *)event_data));
    break;
  case TinyGPSPlus::gpsEvent::significantSpeedChange:
    //addDataPoint();
    ESP_LOGD(__FUNCTION__, "Speed Diff: %f %f", gps->speed.kmph(), *((double *)event_data));
    break;
  case TinyGPSPlus::gpsEvent::significantCourseChange:
    addDataPoint();
    ESP_LOGD(__FUNCTION__, "Course Diff: %f %f", gps->course.deg(), *((double *)event_data));
    break;
  case TinyGPSPlus::gpsEvent::rateChanged:
    ESP_LOGD(__FUNCTION__, "Rate Changed to %d", *((uint8_t *)event_data));
    break;
  case TinyGPSPlus::gpsEvent::msg:
    ESP_LOGV(__FUNCTION__,"msg:%s",(char*)event_data);
    boto = (esp_timer_get_time() > timeoutMicro);
    if ((lastLocTs > 0) && (now - lastLocTs > timeout))
    {
      if (!gpsto)
      {
        gpsto = true;
        ESP_LOGD(__FUNCTION__, "Timeout on GPS Location");
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
        ESP_LOGD(__FUNCTION__, "Timeout on bumps");
      }
    }
    else
    {
      if (buto)
      {
        buto = false;
        ESP_LOGD(__FUNCTION__, "Resumed bumps");
      }
    }
    if (boto && (lastLocTs == 0))
    {
      if (!sito)
      {
        sito = true;
        ESP_LOGD(__FUNCTION__, "Timeout on signal");
      }
    }
    else
    {
      sito = false;
    }

    if ((gpsto || sito) && buto)
    {
      ESP_LOGV(__FUNCTION__, "Lost GPS Signal and no bumps gps:%d sig:%d", gpsto, sito);
      Hibernate();
    }

    if ((lastDpTs > 0) && ((now - lastDpTs) > timeout) && !dto)
    {
      if ((now - lastDpTs) < 16243936){
        dto = true;
        ESP_LOGD(__FUNCTION__, "Timeout on data lastDpTs:%ld timeout:%d diff:%ld", lastDpTs,timeout,now - lastDpTs);
        Hibernate();
      }
    }
    break;
  case TinyGPSPlus::gpsEvent::wakingup:
    sleepTime += ((uint64_t)event_data) * 1000000;
    ESP_LOGV(__FUNCTION__, "Sleep at %lu", (time_t)getSleepTime());
    break;
  case TinyGPSPlus::gpsEvent::go:
    addDataPoint();
    ESP_LOGD(__FUNCTION__, "Go");
    isStopped = false;
    break;
  case TinyGPSPlus::gpsEvent::stop:
    addDataPoint();
    ESP_LOGD(__FUNCTION__, "Stop");
    isStopped = true;
    break;
  case TinyGPSPlus::gpsEvent::gpsPaused:
    ESP_LOGV(__FUNCTION__, "Battery %f", getBatteryVoltage());
    AppConfig::GetAppStatus()->SetStateProperty("/gps", (item_state_t)(item_state_t::ACTIVE | item_state_t::PAUSED));
    break;
  case TinyGPSPlus::gpsEvent::gpsResumed:
    ESP_LOGV(__FUNCTION__, "Battery %f", getBatteryVoltage());
    AppConfig::GetAppStatus()->SetStateProperty("/gps", item_state_t::ACTIVE);
    break;
  case TinyGPSPlus::gpsEvent::atSyncPoint:
    ESP_LOGD(__FUNCTION__, "atSyncPoint");
    //if ((gps != NULL) && (now > 10000))
    //{
    //  xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void *)(BIT3), tskIDLE_PRIORITY, NULL);
    //}
    //xTaskCreate(wifiSallyForth, "wifiSallyForth", 8192, gps, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::outSyncPoint:
    ESP_LOGD(__FUNCTION__, "Leaving Synching");
    break;
  default:
    break;
  }
}

int64_t getSleepTime()
{
  return sleepTime;
}

int64_t getUpTime()
{
  return (esp_timer_get_time() - sleepTime) / 1000000;
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
            xEventGroupSetBits(app_eg, BUMP_BIT);
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
  io_conf.pin_bit_mask = ((1ULL << GPIO_NUM_33) |
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
    ESP_LOGI(__FUNCTION__, "Wakeup was not caused by deep sleep %d", wakeup_reason);
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

esp_err_t setupLittlefs()
{
  const esp_vfs_littlefs_conf_t conf = {
      .base_path = "/lfs",
      .partition_label = "storage",
      .format_if_mount_failed = true,
      .dont_mount = false};

  esp_err_t ret = esp_vfs_littlefs_register(&conf);
  AppConfig* appState = AppConfig::GetAppStatus();
  ESP_LOGD(__FUNCTION__, "lfs mounted %d", ret);
  AppConfig* spiffState = appState->GetConfig("/spiff");
  if (ret != ESP_OK)
  {
    spiffState->SetStateProperty("state",item_state_t::ERROR);
    ESP_LOGE(__FUNCTION__, "Failed in registering littlefs %s", esp_err_to_name(ret));
    return ret;
  }

  size_t total_bytes;
  size_t used_bytes;
  if ((ret = esp_littlefs_info("storage", &total_bytes, &used_bytes)) != ESP_OK)
  {
    ESP_LOGE(__FUNCTION__, "Failed in getting info %s", esp_err_to_name(ret));
    return ret;
  }
  spiffState->SetStateProperty("state",item_state_t::ACTIVE);
  spiffState->SetIntProperty("total",total_bytes);
  spiffState->SetIntProperty("used",used_bytes);
  spiffState->SetIntProperty("free",total_bytes-used_bytes);
  free(spiffState);

  ESP_LOGD(__FUNCTION__, "Space: %d/%d", used_bytes, total_bytes);
  struct dirent *de;
  bool hasCsv = false;
  bool hasLogs = false;
  bool hasFw = false;
  bool hasCfg = false;
  bool hasStat = false;

  ESP_LOGD(__FUNCTION__, "Spiff is spiffy");

  DIR *root = opendir("/lfs");
  if (root == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot open lfs");
    return ESP_FAIL;
  }

  while ((de = readdir(root)) != NULL)
  {
    ESP_LOGD(__FUNCTION__, "%d %s", de->d_type, de->d_name);
    if (strcmp(de->d_name, "csv") == 0)
    {
      hasCsv = true;
    }
    if (strcmp(de->d_name, "logs") == 0)
    {
      hasLogs = true;
    }
    if (strcmp(de->d_name, "firmware") == 0)
    {
      hasFw = true;
    }
    if (strcmp(de->d_name, "config") == 0)
    {
      hasCfg = true;
    }
    if (strcmp(de->d_name, "status") == 0)
    {
      hasStat = true;
    }
  }
  if ((ret = closedir(root)) != ESP_OK)
  {
    ESP_LOGE(__FUNCTION__, "failed to close root %s", esp_err_to_name(ret));
    return ret;
  }

  if (!hasCsv)
  {
    if (mkdir("/lfs/csv", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating csv folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "csv folder created");
  }
  if (!hasLogs)
  {
    if (mkdir("/lfs/logs", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating logs folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "logs folder created");
  }
  if (!hasFw)
  {
    if (mkdir("/lfs/firmware", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating firmware folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "firmware folder created");
  }

  if (!hasCfg)
  {
    if (mkdir("/lfs/config", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating config folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "config folder created");
  }

  if (!hasStat)
  {
    if (mkdir("/lfs/status", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating status folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "status folder created");
  }

  return ESP_OK;
}

int32_t GenerateDevId()
{
  bootloader_random_enable();
  int32_t devId = esp_random();
  bootloader_random_disable();
  return devId;
}

void ConfigurePins(AppConfig* cfg)
{
  ESP_LOGD(__FUNCTION__, "Configuring pins");
  cJSON *pin = NULL;
  uint8_t numPins = 0;
  cJSON_ArrayForEach(pin, cfg->GetJSONConfig("pins"))
  {
    AppConfig *cpin = new AppConfig(pin,cfg);
    gpio_num_t pinNo = cpin->GetPinNoProperty("pinNo");
    if (pinNo > 0)
    {
      ESP_LOGD(__FUNCTION__, "Configuring pin %d", pinNo);
      new Pin(cpin);
      numPins++;
    }
    ldfree(cpin);
  }
}

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGD(__FUNCTION__,"eFuse Two Point: Supported\n");
    } else {
        ESP_LOGD(__FUNCTION__,"eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGD(__FUNCTION__,"eFuse Vref: Supported\n");
    } else {
        ESP_LOGD(__FUNCTION__,"eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGD(__FUNCTION__,"Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGD(__FUNCTION__,"Characterized using eFuse Vref\n");
    } else {
        ESP_LOGD(__FUNCTION__,"Characterized using Default Vref\n");
    }
}

void app_main(void)
{
  if (setupLittlefs() == ESP_OK)
  {
    ESP_LOGI(__FUNCTION__, "Starting");
    check_efuse();
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);
    //adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);
    uint32_t defvref = 1100;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, defvref, &characteristics);
    print_char_val_type(val_type);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    AppConfig *appcfg = new AppConfig(CFG_PATH);
    esp_err_t ret = ESP_OK;

    DIR *root = opendir("/lfs");
    if (root == NULL)
    {
      ESP_LOGE(__FUNCTION__, "Cannot open lfs");
    } else if (closedir(root) != ESP_OK)
    {
      ESP_LOGE(__FUNCTION__, "failed to close root %s", esp_err_to_name(ret));
    } else {
      ESP_LOGD(__FUNCTION__,"Spiff is still spiffy");
    }

    if (appcfg->GetStringProperty("wifitype") == NULL)
    {
      ESP_LOGE(__FUNCTION__, "We have invalid configuration, resetting to default");
      appcfg->ResetAppConfig(false);
    }

    if (appcfg->GetIntProperty("deviceid") <= 0)
    {
      ESP_LOGD(__FUNCTION__, "Seeding device id");
      appcfg->SetIntProperty("deviceid", GenerateDevId());
    }

    initLog();
    sampleBatteryVoltage();
    EventManager *mgr = new EventManager(appcfg->GetJSONConfig("/events"));
    uint32_t tmp;
    ESP_LOGV(__FUNCTION__, "Pre-Loading Image....");
    loadImage(false, &tmp);
    //initSPISDCard();
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO, 1);

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
      lastPoiState = poiState_t::unknown;
    }
    print_wakeup_reason();
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
    {
      if (hibernate)
      {
        ESP_LOGD(__FUNCTION__, "re-sleeping...");
        Hibernate();
      }
    }
    ESP_LOGD(__FUNCTION__, "Starting bumps:%d, lastMovement:%d", bumpCnt, CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);

    RawDegrees lat;
    RawDegrees lng;
    lat.deg = lastLatDeg;
    lat.billionths = lastLatBil;
    lat.negative = lastLatNeg;
    lng.deg = lastLngDeg;
    lng.billionths = lastLngBil;
    lng.negative = lastLngNeg;
    if (appcfg->HasProperty("/gps/rxPin") && appcfg->GetIntProperty("/gps/rxPin"))
    {
      ESP_LOGD(__FUNCTION__, "Starting GPS");
      gps = new TinyGPSPlus(appcfg->GetConfig("/gps"));
      if (gps != NULL)
      {
        ESP_LOGD(__FUNCTION__, "Waiting for GPS");
        if (xEventGroupWaitBits(gps->eg, TinyGPSPlus::gpsEvent::gpsRunning, pdFALSE, pdTRUE, 1500 / portTICK_RATE_MS)&TinyGPSPlus::gpsEvent::gpsRunning)
        {
          createTrip();
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::msg, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::go, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::stop, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::sleeping, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::wakingup, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::rateChanged, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::systimeChanged, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::locationChanged, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::significantCourseChange, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::significantDistanceChange, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::significantSpeedChange, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::gpsPaused, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::gpsResumed, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::atSyncPoint, gpsEvent, &gps));
          ESP_ERROR_CHECK(esp_event_handler_register(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::outSyncPoint, gpsEvent, &gps));
          //configureMotionDetector();
          //commitTripToDisk((void*)(BIT1|BIT2));
          //ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS,TinyGPSPlus::gpsEvent::atSyncPoint,NULL,0,portMAX_DELAY));
        }
        else
        {
          ESP_LOGW(__FUNCTION__, "No GPS Connected");
          if (gps != NULL)
            ldfree(gps);
          gps = NULL;
        }
      }
    }
    else
    {
      xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void *)(BIT2 | BIT3), tskIDLE_PRIORITY, NULL);
    }

    if (indexOf(appcfg->GetStringProperty("wifitype"), "AP") != NULL)
    {
      ESP_LOGD(__FUNCTION__, "Starting puller's wifi");
      xTaskCreate(wifiSallyForth, "wifiSallyForth", 8192, gps, tskIDLE_PRIORITY, NULL);
    }
    ConfigurePins(appcfg);
  }
}

void stopGps()
{
  ESP_LOGD(__FUNCTION__, "Stopping GPS");
  if (gps != NULL)
  {
    ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::gpsStopped, NULL, 0, portMAX_DELAY));
    xEventGroupWaitBits(gps->eg, TinyGPSPlus::gpsEvent::gpsStopped, pdFALSE, pdTRUE, portMAX_DELAY);
  }
  else
  {
    ESP_LOGD(__FUNCTION__, "No gps to stop");
  }
}