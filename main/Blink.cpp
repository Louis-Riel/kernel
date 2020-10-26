#include "../build/config/sdkconfig.h"

#include "utils.h"
#include <stdio.h>
#include <string.h>
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
RTC_DATA_ATTR uint8_t curRate = 0;
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
float batLvls[NUM_VOLT_CYCLE];
uint8_t batSmplCnt=NUM_VOLT_CYCLE+1;
bool balFullSet = false;
uint64_t sleepTime=0;

static const char* pmpt1 = "    <Placemark>\n\
      <styleUrl>#SpeedPlacemark</styleUrl>\n\
      <Point>\n\
        <coordinates>";
static const char* pmpt2 = "</coordinates>\n\
      </Point>\n\
      <description>";
static const char* pmpt3 = "km/h\n\
RAM:";
static const char* pmpt4 = "\nBattery:";
static const char* pmpt5 = "</description>\n\
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

trip* getActiveTrip(){
  return &curTrip;
}

float getBatteryVoltage();
void sampleBatteryVoltage()
{
  if (batSmplCnt>=NUM_VOLT_CYCLE){
    balFullSet=batSmplCnt==NUM_VOLT_CYCLE;
    batSmplCnt=0;
    ESP_LOGD(__FUNCTION__,"Voltage:%f",getBatteryVoltage());
  }

  uint32_t voltage;
  esp_adc_cal_get_voltage((adc_channel_t)ADC1_CHANNEL_7, &characteristics, &voltage);
  batLvls[batSmplCnt++] = voltage /4096.0*7.445;
}

float getBatteryVoltage()
{
  if (batSmplCnt>=NUM_VOLT_CYCLE){
    sampleBatteryVoltage();
  }

  float voltage=0;
  uint8_t uloop=balFullSet?NUM_VOLT_CYCLE:batSmplCnt;

  for (uint8_t idx=0; idx < uloop; idx++) {
    voltage+=batLvls[idx];
  }
  return uloop>0?voltage/uloop:0;
}

dataPoint *getCurDataPoint()
{
  time(&lastDpTs);
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

bool commitTrip(trip *trip)
{
  if ((trip == NULL) || (trip->numNodes <= 0))
  {
    ESP_LOGW(__FUNCTION__,"commitTrip called without an active trip");
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
  ESP_LOGI(__FUNCTION__, "Saving trip with %d nodes to %s %s", trip->numNodes, trip->fname,curTrip.fname);
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
      fprintf(f, "%s,%3.7f,%3.7f,%d,%d,%d,%d,%f,%d\n", dt, dp->lng, dp->lat, dp->speed, dp->altitude, dp->course, dp->freeRam, dp->bat, bumpCnt);
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

    if (lineCount <= 0) {
      ESP_LOGE(__FUNCTION__,"Nothing in this trip");
      return true;
    }
    ESP_LOGD(__FUNCTION__, "Baking KML from %d lines %d chars", lineCount, pos);
    FILE *kml = fopen(kmlFileName, "w", true);
    if (kml == NULL)
    {
      ESP_LOGE(__FUNCTION__, "Cannot open %s", kmlFileName);
      return false;
    }
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
                    memset(speed,0,10);
                    memset(ts,0,30);
                    speed[0]=0;
                    uint8_t speedLen=0;
                    uint8_t tsLen=0;
                    ts[0]=0;
                    while ((fc = fgetc(trp)) != EOF)
                    {
                      if ((fc == 10) || (fc == 13))
                      {
                        if ((bc != 10) && (bc != 13))
                        {
                          lineCount++;
                          fldNo = 1;
                          tsLen=0;
                          ts[0]=0;
                          speedLen=0;
                          speed[0]=0;
                        }
                      }
                      if (fc == ',')
    //fprintf(f, "timestamp,longitude,latitude,speed,altitude,course,RAM,Battery,Bumps\n");
                      {
                        if (fldNo == 1){
                          fprintf(kml,pmpt1);
                          ts[tsLen]=0;
                        }
                        if ((fldNo >=2) && (fldNo <=3)){
                            fputc(fc, kml);
                        }

                        if (fldNo == 4){
                          fprintf(kml,pmpt2);
                          fprintf(kml,ts);
                          fprintf(kml,"\nSpeed:");
                          fprintf(kml,speed);
                          fprintf(kml,pmpt3);
                          speed[speedLen]=0;
                        }
                        if (fldNo == 7){
                          fprintf(kml,pmpt4);
                        }
                        if (fldNo == 8){
                          fprintf(kml,pmpt5);
                        }
                        fldNo++;
                      } else {
                        if (fldNo == 1) {
                          ts[tsLen++] = fc;
                        }
                        if ((fldNo >=2) && (fldNo <=4)){
                            fputc(fc, kml);
                        }
                        if ((fldNo >= 7) && (fldNo <=8))
                        {
                          fputc(fc, kml);
                        }
                        if (fldNo==4) {
                          speed[speedLen++]=fc;
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
    fclose(kml);
    fclose(trp);
    ESP_LOGD(__FUNCTION__, "Done baking KML");
    return true;
  }
  return false;
}

void commitTripToDisk(void* param)
{
  xEventGroupSetBits(app_eg,app_bits_t::COMMITTING_TRIPS);
  xEventGroupClearBits(app_eg,app_bits_t::TRIPS_COMMITTED);
  uint32_t theBits = (uint32_t)param;
  if (initSPISDCard())
  {
    char *kFName = (char *)malloc(350);
    char *cFName = (char *)malloc(350);
    char *csvs = (char *)malloc(300 * 10);

    if (commitTrip(&curTrip))
    {
      if (theBits&BIT1)
      {
        sprintf(kFName, "/sdcard/kml/%s.kml", &curTrip.fname[8]);
        char* theChar;
        for (theChar = kFName; *theChar != 0; theChar++) {
          if (*theChar == '_') {
            *theChar='/';
            break;
          }
          if (*theChar == '-') {
            *theChar='/';
          }
        }
        sprintf(theChar+1, "%s.kml", &curTrip.fname[8]);
        
        if (bakeKml(curTrip.fname, kFName))
        {
          sprintf(kFName, "/sdcard/converted/%s", &curTrip.fname[8]);
          if (moveFile(curTrip.fname, kFName))
          {
            ESP_LOGD(__FUNCTION__, "Moved %s to %s.", curTrip.fname, kFName);
          } else
          {
            ESP_LOGE(__FUNCTION__, "Failed moving %s to %s", curTrip.fname, kFName);
          }
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Failed baking %s from %s", curTrip.fname, cFName);
        }
      }
    }
    if (theBits&BIT2)
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
              char* theChar;
              for (theChar = kFName; *theChar != 0; theChar++) {
                if (*theChar == '_') {
                  *theChar='/';
                  break;
                }
                if (*theChar == '-') {
                  *theChar='/';
                }
              }
              sprintf(theChar+1, "%s.kml", curCsv);
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
    deinitSPISDCard();
    free(kFName);
    free(cFName);
    free(csvs);
  }
  xEventGroupClearBits(app_eg,app_bits_t::COMMITTING_TRIPS);
  xEventGroupSetBits(app_eg,app_bits_t::TRIPS_COMMITTED);
  if (theBits&BIT3)
    vTaskDelete(NULL);
}

void addDataPoint()
{
  dataPoint *curNode = getCurDataPoint();
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

  if (curTrip.numNodes % 10 == 0) { 
    if (gps != NULL){
      xEventGroupSetBits(app_eg,app_bits_t::COMMITTING_TRIPS);
      xEventGroupClearBits(app_eg,app_bits_t::TRIPS_COMMITTED);
      xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void*)(BIT3), tskIDLE_PRIORITY, NULL);
    }
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

void doHibernate(void* param)
{
  if (gps!=NULL)
  {
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Deep Sleeping %d = %d", bumpCnt, gps->numRunners);
    if (gps != NULL){
      commitTripToDisk((void*)0);
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
    lastPoiState = poiState_t::unknown;
    curTrip.fname[0] = 0;
    curTrip.lastExportedTs = 0;
    curTrip.nodesAllocated = 0;
    curTrip.numNodes = 0;
    hibernate = true;
    ESP_LOGD(__FUNCTION__,"Hybernating");

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

void Hibernate() {
  if (xEventGroupGetBits(gps->eg) & TinyGPSPlus::gpsEvent::gpsRunning){
    xEventGroupClearBits(gps->eg,TinyGPSPlus::gpsEvent::gpsRunning);
    xEventGroupSetBits(gps->eg,TinyGPSPlus::gpsEvent::gpsStopped);
    xTaskCreate(doHibernate, "doHibernate", 8192, NULL , tskIDLE_PRIORITY, NULL);
  }
}

static void gpsEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  time(&now);
  sampleBatteryVoltage();
  app_config_t* appcfg=NULL;
  switch (id)
  {
  case TinyGPSPlus::gpsEvent::locationChanged:
    ESP_LOGD(__FUNCTION__, "Location: %3.6f, %3.6f, %3.6f, %4.2f", gps->location.lat(), gps->location.lng(), gps->speed.kmph(), gps->altitude.meters());
    if (lastLocTs == 0) {
      appcfg=getAppConfig();
      if (appcfg->pois->minDistance==0 ) {
        appcfg->pois->minDistance=200;
        appcfg->pois->lat=gps->location.lat();
        appcfg->pois->lng=gps->location.lng();
        saveConfig();
      }
    }
    lastLocTs = now;
    break;
  case TinyGPSPlus::gpsEvent::systimeChanged:
    char strftime_buf[64];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(__FUNCTION__, "System Time: %s", strftime_buf);
    break;
  case TinyGPSPlus::gpsEvent::significantDistanceChange:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Distance Diff: %f", *((double *)event_data));
    //xTaskCreate(flash, "flashy", 2048, (void *)1, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::significantSpeedChange:
    //addDataPoint();
    ESP_LOGI(__FUNCTION__, "Speed Diff: %f %f", gps->speed.kmph(), *((double *)event_data));
    //xTaskCreate(flash, "flashy", 2048, (void *)2, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::significantCourseChange:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Course Diff: %f %f", gps->course.deg(), *((double *)event_data));
    //xTaskCreate(flash, "flashy", 2048, (void *)3, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::rateChanged:
    ESP_LOGI(__FUNCTION__, "Rate Changed to %d", *((uint8_t *)event_data));
    curRate = *((uint8_t *)event_data);
    //xTaskCreate(flash, "flashy", 2048, (void *)curRate, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::msg:
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

    //ESP_LOGV(__FUNCTION__, "msg: %s", (char *)event_data);
    break;
  case TinyGPSPlus::gpsEvent::wakingup:
    sleepTime+=((uint64_t)event_data)*1000000;
    ESP_LOGD(__FUNCTION__,"Sleep at %lu", (time_t)getSleepTime());
    break;
  case TinyGPSPlus::gpsEvent::go:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Go");
    isStopped = false;
    break;
  case TinyGPSPlus::gpsEvent::stop:
    addDataPoint();
    ESP_LOGI(__FUNCTION__, "Stop");
    isStopped = true;
    break;
  case TinyGPSPlus::gpsEvent::gpsPaused:
    getAppState()->gps=(item_state_t)(item_state_t::ACTIVE|item_state_t::PAUSED);
    break;
  case TinyGPSPlus::gpsEvent::gpsResumed:
    getAppState()->gps=(item_state_t)(item_state_t::ACTIVE);
    break;
  case TinyGPSPlus::gpsEvent::atSyncPoint:
    ESP_LOGD(__FUNCTION__, "Synching");
    if (gps != NULL){
      xEventGroupSetBits(app_eg,app_bits_t::COMMITTING_TRIPS);
      xEventGroupClearBits(app_eg,app_bits_t::TRIPS_COMMITTED);
      xEventGroupClearBits(app_eg,app_bits_t::TRIPS_SYNCED);
      xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void*)(BIT3), tskIDLE_PRIORITY, NULL);
    }
    xTaskCreate(wifiSallyForth, "wifiSallyForth", 8192, gps , tskIDLE_PRIORITY, NULL);
    //xTaskCreate(wifiStart, "wifiStart", 4096, NULL , tskIDLE_PRIORITY, NULL);
    //wifiStart(NULL);
    break;
  case TinyGPSPlus::gpsEvent::outSyncPoint:
    ESP_LOGD(__FUNCTION__, "Leaving Synching");
    wifiStop(NULL);
    break;
  default:
    break;
  }
}

int64_t getSleepTime(){
  return sleepTime;
}

int64_t getUpTime(){
  return esp_timer_get_time()-sleepTime;
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
            xEventGroupSetBits(app_eg,BUMP_BIT);
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
    ESP_LOGI(__FUNCTION__, "Wakeup was not caused by deep sleep %d",wakeup_reason);
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
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  app_config_t* appcfg=initConfig();
  initLog();
  gpio_reset_pin(BLINK_GPIO);
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(BLINK_GPIO,1);

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

  //gps = new TinyGPSPlus(GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_13, lastRate, lat, lng, lastCourse, lastSpeed, lastAltitude,app_eg);
  if ((appcfg->purpose&app_config_t::purpose_t::TRACKER) && (appcfg->gps_config.rxPin != 0)){
    gps = new TinyGPSPlus(appcfg->gps_config.rxPin, appcfg->gps_config.txPin, appcfg->gps_config.enPin, lastRate, lat, lng, lastCourse, lastSpeed, lastAltitude,app_eg);
    if (gps != NULL){
      ESP_LOGD(__FUNCTION__,"Waiting for GPS");
      if (xEventGroupWaitBits(gps->eg,TinyGPSPlus::gpsEvent::gpsRunning,pdFALSE,pdTRUE,1500/portTICK_RATE_MS)){
        gps->poiState=lastPoiState;
        createTrip();
        adc1_config_width(ADC_WIDTH_12Bit);
        adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);
        //gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        uint32_t defvref=1100;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, defvref, &characteristics);
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
        configureMotionDetector();
        //commitTripToDisk((void*)(BIT1|BIT2));
        getAppState()->sdCard=item_state_t::ACTIVE;
        //ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS,TinyGPSPlus::gpsEvent::atSyncPoint,NULL,0,portMAX_DELAY));

      } else {
        ESP_LOGD(__FUNCTION__,"No GPS Connected");
        if (gps != NULL)
          free(gps);
        gps=NULL;
        getAppState()->gps=item_state_t::ERROR;
      }
    }
  } else {
    getAppState()->gps = item_state_t::INACTIVE;
  }

  if (appcfg->purpose&app_config_t::purpose_t::PULLER){
    xTaskCreate(wifiSallyForth, "wifiSallyForth", 8192, gps , tskIDLE_PRIORITY, NULL);
  }
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
  //esp_deep_sleep_start();

}

void stopGps(){
  ESP_LOGD(__FUNCTION__,"Stopping GPS");
  if (gps != NULL) {
    ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS,TinyGPSPlus::gpsEvent::gpsStopped,NULL,0,portMAX_DELAY));
    xEventGroupWaitBits(gps->eg,TinyGPSPlus::gpsEvent::gpsStopped,pdFALSE,pdTRUE,portMAX_DELAY);
  } else {
    ESP_LOGD(__FUNCTION__,"No gps to stop");
  }
}