/*
TinyGPS++ - a small GPS library for Arduino providing universal NMEA parsing
Based on work by and "distanceBetween" and "courseTo" courtesy of Maarten Lamers.
Suggestion to add satellites, courseTo(), and cardinal() by Matt Monson.
Location precision improvements suggested by Wayne Holder.
Copyright (C) 2008-2013 Mikal Hart
All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "./TinyGPS++.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <time.h>
#include <sys/time.h>
#include "esp_sleep.h"
#include "../../main/utils.h"
#include "eventmgr.h"

#define _GPRMCterm "GPRMC"
#define _GPGGAterm "GPGGA"
#define _GNRMCterm "GNRMC"
#define _GNGGAterm "GNGGA"
#define _GPZDAterm "--ZDA"
#define _GPTXTterm "GPTXT"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define BLINKY

const uint8_t numWakePins = 3;
const gpio_num_t wakePins[] = {GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_34};
static const char *gps_protocol_name[] = {
    "GPS_GGA",
    "GPS_GLL",
    "GPS_GSA",
    "GPS_GSV",
    "GPS_RMC",
    "GPS_VTG",
    "GPS_GRS",
    "GPS_GST",
    "GPS_ZDA"};

unsigned long IRAM_ATTR millis()
{
  return (unsigned long)(esp_timer_get_time() / 1000LL);
}

double radians(double angle)
{
  return (angle * 3.14159265358979323846 / 180.0);
}

double degrees(double angle)
{
  return (angle * 180.0 / 3.14159265358979323846);
}

uint32_t TinyGPSPlus::getSleepTime()
{
  return sleepTimes[curFreqIdx];
}

void TinyGPSPlus::waitOnStop(void *param)
{
  TinyGPSPlus *gps = (TinyGPSPlus *)param;
  ESP_LOGD(__FUNCTION__, "We are stopped, checking bumps");
  if ((xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged, pdFALSE, pdFALSE, 5000 / portTICK_PERIOD_MS) & gpsEvent::locationChanged) && (gps->speed.kmph() > 5))
  {
    ESP_LOGD(__FUNCTION__, "We are not so stopped, not using bump bumps");
  }
  else
  {
    uint64_t ext_wakeup_pin_mask = 0;
    int curLvl = 0;
    for (int idx = 0; idx < numWakePins; idx++)
    {
      curLvl = gpio_get_level(wakePins[idx]);
      ESP_LOGD(__FUNCTION__, "Pin %d is %d", wakePins[idx], curLvl);
      if (curLvl == 0)
      {
        ext_wakeup_pin_mask |= (1ULL << wakePins[idx]);
      }
    }
    int32_t timeToGo = sleepTimes[gps->curFreqIdx] * 1000;
    if (xEventGroupGetBits(getAppEG()) & app_bits_t::WIFI_ON)
    {
      ESP_LOGD(__FUNCTION__, "We are stopped with wifi on, waiting on bumps");
      while ((timeToGo > 0) && !(xEventGroupWaitBits(gps->eg, gpsEvent::outSyncPoint, pdFALSE, pdTRUE, 500 / portTICK_PERIOD_MS) & gpsEvent::outSyncPoint))
      {
        gps->gpsPause();
#ifdef BLINKY
        gpio_set_level(BLINK_GPIO, 0);
#endif
        xEventGroupWaitBits(gps->eg, gpsEvent::outSyncPoint, pdFALSE, pdTRUE, 300 / portTICK_PERIOD_MS);
#ifdef BLINKY
        gpio_set_level(BLINK_GPIO, 1);
#endif
        gps->gpsResume();
        timeToGo -= 804;
      }
#ifdef BLINKY
      gpio_set_level(BLINK_GPIO, 1);
#endif
    }
    else
    {
      timeToGo = GPS_TIMEOUT*1000;
      esp_sleep_wakeup_cause_t wcause = esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TIMER;
      while ((timeToGo > 0) && (wcause == esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TIMER)){
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(SLEEP_TIMEOUT * 1000000));
        if (ext_wakeup_pin_mask != 0)
        {
          ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_mask, ESP_EXT1_WAKEUP_ANY_HIGH));
        }
        else
        {
          ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(wakePins[0], 0));
        }
        WaitToSleep();
        ESP_LOGD(__FUNCTION__, "Sleeping %d secs on stop", SLEEP_TIMEOUT);
        gps->gpsPause();
        ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::sleeping, NULL, 0, portMAX_DELAY));
        esp_light_sleep_start();
        ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::wakingup, NULL, 0, portMAX_DELAY));
        //gpio_set_level(BLINK_GPIO, 0);
        wcause = esp_sleep_get_wakeup_cause();
        gps->gpsResume();
        timeToGo -= (SLEEP_TIMEOUT * 1000);
        if (!(xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged, pdFALSE, pdFALSE, 5000 / portTICK_PERIOD_MS) & gpsEvent::locationChanged))
        {
          ESP_LOGD(__FUNCTION__, "We are stopped, lost GPS signal");
          break;
        }
      }
    }
    gps->toBeFreqIdx = 0;
    gps->adjustRate();
  }
}

void TinyGPSPlus::theLoop(void *param)
{
  TinyGPSPlus *gps = (TinyGPSPlus *)param;
  double dist = -1.0;
  double dtmp = -1.0;
  AppConfig *appcfg = NULL;
  cJSON *jcfg = NULL;
  cJSON *poi = NULL;
  cJSON *pois = NULL;
  bool hasPois = false;
  EventBits_t bits = 0;
  uint32_t cfgVer = 0;

  struct poi_t
  {
    double lng;
    double lat;
  } thePois[10];
  uint8_t numPois = 0;

  memset(&thePois, 0, sizeof(thePois));

  gps->gpsResume();

  ESP_LOGD(__FUNCTION__, "Waiting for First GPS msg");
  bool flipidyFlop = true;
#ifdef BLINKY
  while ((xEventGroupGetBits(gps->eg) & gpsEvent::gpsRunning) &&
         !(bits = xEventGroupWaitBits(gps->eg, gpsEvent::msg, pdFALSE, pdTRUE, 100 / portTICK_RATE_MS) & gpsEvent::msg))
  {
    gpio_set_level(BLINK_GPIO, flipidyFlop = !flipidyFlop);
  }
  gpio_set_level(BLINK_GPIO, 1);
#else
  xEventGroupWaitBits(gps->eg, gpsEvent::msg, pdFALSE, pdTRUE, portMAX_DELAY);
#endif

  ESP_LOGD(__FUNCTION__, "Waiting for GPS Initialization");
  bool loged = false;
  //int64_t startTs = esp_timer_get_time();
  while (((bits=xEventGroupGetBits(gps->eg)) & gpsEvent::gpsRunning) &&
         !(bits&gpsEvent::initialized)){
    if (xEventGroupWaitBits(gps->eg,gpsEvent::initialized,pdFALSE,pdTRUE,1000/portTICK_PERIOD_MS) & gpsEvent::initialized) {
      int wb;
      if ((wb = uart_write_bytes(UART_NUM_2, (const char *)update_0_2_secs, sizeof(update_0_2_secs))) != sizeof(update_0_2_secs))
      {
        ESP_LOGW(__FUNCTION__, "Failed sending freq scaledown command (%d bytes), ret %d bytes", sizeof(update_0_2_secs), wb);
      }
      ESP_LOGD(__FUNCTION__, "Flagging protocols");
      gps->flagProtocol(gps_protocol_t::GPS_GGA, pdTRUE);
      gps->flagProtocol(gps_protocol_t::GPS_RMC, pdTRUE);
      gps->flagProtocol(gps_protocol_t::GPS_GLL, pdFALSE);
      gps->flagProtocol(gps_protocol_t::GPS_GSA, pdFALSE);
      gps->flagProtocol(gps_protocol_t::GPS_GSV, pdFALSE);
      gps->flagProtocol(gps_protocol_t::GPS_VTG, pdFALSE);
      gps->flagProtocol(gps_protocol_t::GPS_GRS, pdFALSE);
      gps->flagProtocol(gps_protocol_t::GPS_GST, pdFALSE);
      gps->flagProtocol(gps_protocol_t::GPS_ZDA, pdTRUE);
    } else {
      if (!loged){
        ESP_LOGW(__FUNCTION__, "GPS Not Initializing, rebooting GPS");
        loged = true;
      }
      gps->gpsStop();
      vTaskDelay(200/portTICK_PERIOD_MS);
      gps->gpsStart();
    }
  }

  gps->curFreqIdx = 0;
  gps->toBeFreqIdx = 0;

  ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(10 * 1000000));
  gpio_hold_en(gps->enpin);
  gpio_deep_sleep_hold_en();

  uint8_t retryCnt = 0;
#ifdef BLINKY
  ESP_LOGV(__FUNCTION__, "Waiting on GPS %d secs...", GPS_WAIT_PERIOD);
  while ((xEventGroupGetBits(gps->eg) & gpsEvent::gpsRunning) &&
         !(bits = (xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged, pdFALSE, pdFALSE, 400 / portTICK_PERIOD_MS)) & gpsEvent::locationChanged) &&
         (gpio_set_level(BLINK_GPIO, 0) == ESP_OK) &&
         !(bits = (xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged, pdFALSE, pdFALSE, 400 / portTICK_PERIOD_MS)) & gpsEvent::locationChanged) &&
         (gpio_set_level(BLINK_GPIO, 1) == ESP_OK))
#else
  while ((xEventGroupGetBits(gps->eg) & gpsEvent::gpsRunning) &&
         !(bits = (xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged, pdFALSE, pdFALSE, 600 / portTICK_PERIOD_MS)) & gpsEvent::locationChanged))
#endif
  {

    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(GPS_WAIT_PERIOD * 1000000));
    ++retryCnt;
    ESP_LOGV(__FUNCTION__, "Waiting on GPS %d secs %d", GPS_WAIT_PERIOD, retryCnt);
    if (xEventGroupGetBits(getAppEG()) & app_bits_t::WIFI_ON)
      xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged, pdFALSE, pdFALSE, 1000 / portTICK_PERIOD_MS);
    else
    {
      ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::sleeping, NULL, 0, portMAX_DELAY));
      //gps->gpsPause();
      gps->skipNext=true;
      esp_light_sleep_start();
      //gps->gpsResume();
      ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::wakingup, NULL, 0, portMAX_DELAY));
    }
  }

  gpio_set_level(BLINK_GPIO, 1);

  if (!(xEventGroupGetBits(gps->eg) & gpsEvent::gpsRunning))
  {
    ESP_LOGD(__FUNCTION__, "GPS stopped");
    xEventGroupSetBits(gps->eg, gpsEvent::gpsStopped);
    return;
  }

  ESP_LOGD(__FUNCTION__, "Initializing POIs");
  appcfg = AppConfig::GetAppConfig();
  cfgVer = appcfg->version;
  jcfg = appcfg->GetJSONConfig(NULL);
  if (!cJSON_HasObjectItem(jcfg, "pois"))
  {
    ESP_LOGD(__FUNCTION__, "No POIs currently configured");
    pois = cJSON_AddArrayToObject(jcfg, "pois");
  }
  else
  {
    ESP_LOGD(__FUNCTION__, "POIs are currently configured");
    pois = cJSON_GetObjectItem(jcfg, "pois");
  }

  cJSON_ArrayForEach(poi, pois)
  {
    AppConfig *apoi = new AppConfig(poi, appcfg);
    hasPois = true;
    thePois[numPois].lat = apoi->GetDoubleProperty("lat");
    thePois[numPois++].lng = apoi->GetDoubleProperty("lng");
    ESP_LOGV(__FUNCTION__, "POI%d: lat:%f lng:%f", numPois, thePois[numPois - 1].lat, thePois[numPois - 1].lng);
    if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
    {
      char *ctmp = cJSON_Print(poi);
      ESP_LOGD(__FUNCTION__, "%s", ctmp);
      ldfree(ctmp);
    }
    free(apoi);
  }

  if (!hasPois)
  {
    ESP_LOGD(__FUNCTION__, "No POIs, adding one here");
    poi = cJSON_CreateObject();
    cJSON_AddItemToArray(pois, poi);
    AppConfig *cpoi = new AppConfig(poi, appcfg);
    thePois[0].lat = gps->location.lat();
    thePois[0].lng = gps->location.lng();
    cpoi->SetDoubleProperty("lat", thePois[0].lat);
    cpoi->SetDoubleProperty("lng", thePois[0].lng);
    ESP_LOGD(__FUNCTION__, "1POI%d: lat:%f lng:%f", numPois, thePois[numPois].lat, thePois[numPois].lng);
    numPois = 1;
    hasPois = true;
    free(cpoi);
  }

  dist = -1;
  for (int idx = 0; idx < numPois; idx++)
  {
    if (thePois[idx].lat == 0)
    {
      break;
    }
    dtmp = TinyGPSPlus::distanceBetween(gps->location.lat(), gps->location.lng(), thePois[idx].lat, thePois[idx].lng);
    if ((dist == -1) || (dist > dtmp))
    {
      dist = dtmp;
    }
  }

  if (dist >= 0.0)
  {
    gps->poiState = (dist >= PIO_MIN_DIST) ? poiState_t::out : poiState_t::in;
    if (gps->poiState == poiState_t::in)
    {
      ESP_LOGD(__FUNCTION__, "Initially in PIO dist:%f lat:%f lng:%f", dist, gps->location.lat(), gps->location.lng());
      ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::atSyncPoint, NULL, 0, portMAX_DELAY));
    }
    else
    {
      ESP_LOGD(__FUNCTION__, "Initially out of PIO dist:%f lat:%f lng:%f", dist, gps->location.lat(), gps->location.lng());
      ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::outSyncPoint, NULL, 0, portMAX_DELAY));
    }
  }
  else
  {
    ESP_LOGE(__FUNCTION__, "Cannot get dist to POI");
  }
  ESP_LOGI(__FUNCTION__,"GPS Signal Aquired");

  while (xEventGroupGetBits(gps->eg) & gpsEvent::gpsRunning)
  {
    bits = xEventGroupWaitBits(gps->eg, gpsEvent::locationChanged | gpsEvent::msg | gpsEvent::gpsPaused, pdFALSE, pdTRUE, 200 / portTICK_PERIOD_MS);
    if (bits & gpsEvent::msg)
    {
      gps->processEncoded();
#ifdef BLINKY
      gpio_set_level(BLINK_GPIO, 1);
#endif
    } 
    if (bits & gpsEvent::locationChanged)
    {
#ifdef BLINKY
      gpio_set_level(BLINK_GPIO, 1);
#endif
      if ((gps->location.lat() == 0) || (gps->location.lng() == 0))
      {
        ESP_LOGV(__FUNCTION__, "Whacky coords");
        continue;
      }

      dist = -1;
      for (int idx = 0; idx < numPois; idx++)
      {
        if (thePois[idx].lat == 0)
        {
          break;
        }
        dtmp = TinyGPSPlus::distanceBetween(gps->location.lat(), gps->location.lng(), thePois[idx].lat, thePois[idx].lng);
        if ((dist == -1) || (dist > dtmp))
        {
          dist = dtmp;
        }
      }

      if (dist >= 0.0)
      {
        switch (gps->poiState)
        {
        case poiState_t::unknown:
          gps->poiState = (dist >= PIO_MIN_DIST) ? poiState_t::out : poiState_t::in;
          if (gps->poiState == poiState_t::in)
          {
            ESP_LOGD(__FUNCTION__, "Weirly in PIO %f", dist);
            ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::atSyncPoint, NULL, 0, portMAX_DELAY));
          }
          else
          {
            ESP_LOGD(__FUNCTION__, "Weirly out of PIO %f", dist);
            ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::outSyncPoint, NULL, 0, portMAX_DELAY));
          }
          break;
        case poiState_t::in:
          if ((dist >= PIO_MIN_DIST) && (dist < 1000000))
          {
            ESP_LOGD(__FUNCTION__, "Out of PIO dist:%f lat:%f lng:%f", dist, gps->location.lat(), gps->location.lng());
            gps->poiState = poiState_t::out;
            ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::outSyncPoint, NULL, 0, portMAX_DELAY));
          }
          break;
        case poiState_t::out:
          if (dist <= PIO_MIN_DIST)
          {
            ESP_LOGD(__FUNCTION__, "In of PIO %f", dist);
            gps->poiState = poiState_t::in;
            ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::atSyncPoint, NULL, 0, portMAX_DELAY));
          }
          break;
        }
      }
    }
    else
    {
#ifdef BLINKY
      if (!(bits & gpsEvent::gpsPaused)){
        gpio_set_level(BLINK_GPIO, flipidyFlop = !flipidyFlop);
      }
#endif
    }
  }
  ESP_LOGD(__FUNCTION__, "GPS Exited");
  xEventGroupSetBits(gps->eg, gpsEvent::gpsStopped);
}

void TinyGPSPlus::gpsEventProcessor(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  TinyGPSPlus *gps = (TinyGPSPlus *)handler_args;
  switch (id)
  {
  case TinyGPSPlus::gpsEvent::gpsPaused:
    if (!(xEventGroupGetBits(gps->eg) & TinyGPSPlus::gpsEvent::gpsPaused))
      gps->gpsPause();
    break;
  case TinyGPSPlus::gpsEvent::gpsResumed:
    if (xEventGroupGetBits(gps->eg) & TinyGPSPlus::gpsEvent::gpsPaused)
      gps->gpsResume();
    break;
  case TinyGPSPlus::gpsEvent::stop:
    CreateBackgroundTask(TinyGPSPlus::waitOnStop, "waitonstop", 4096, handler_args, tskIDLE_PRIORITY, NULL);
    break;
  case TinyGPSPlus::gpsEvent::locationChanged:
    if (!gps->isSignificant())
    {
      if (gps->toBeFreqIdx < ((sizeof(sleepTimes) / sizeof(uint8_t)) - 1))
      {
        if (gps->speed.kmph() < 80)
        {
          if (sleepTimes[gps->toBeFreqIdx] < 60)
            gps->toBeFreqIdx++;
        }
        else
        {
          gps->toBeFreqIdx++;
        }
      }
    }
    gps->adjustRate();
    if ((gps->poiState == poiState_t::out) || !(xEventGroupGetBits(getAppEG()) & app_bits_t::WIFI_ON))
    {
      gps->gpsPause();
      ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleepTimes[gps->curFreqIdx] * 1000000));

      WaitToSleep();
      ESP_LOGV(__FUNCTION__, "Napping for %d", sleepTimes[gps->curFreqIdx]);
      ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::sleeping, NULL, 0, portMAX_DELAY));
      ESP_ERROR_CHECK(esp_light_sleep_start());
      ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::wakingup, (void *)&sleepTimes[gps->curFreqIdx], sizeof(uint8_t), portMAX_DELAY));
      gps->gpsResume();
    }
    break;
  case TinyGPSPlus::gpsEvent::gpsStopped:
    xEventGroupClearBits(gps->eg, TinyGPSPlus::gpsEvent::gpsRunning);
    ESP_LOGD(__FUNCTION__, "Stopping GPS %d", xEventGroupGetBits(gps->eg) & TinyGPSPlus::gpsEvent::gpsRunning);
    break;
  }
}

static TinyGPSPlus *instance = NULL;

TinyGPSPlus::TinyGPSPlus(AppConfig *config) : TinyGPSPlus(
                                                  config->GetPinNoProperty("rxPin"),
                                                  config->GetPinNoProperty("txPin"),
                                                  config->GetPinNoProperty("enPin"))
{
}

TinyGPSPlus *TinyGPSPlus::runningInstance()
{
  return instance;
}

TinyGPSPlus::TinyGPSPlus(gpio_num_t rxpin, gpio_num_t txpin, gpio_num_t enpin)
    : location(NULL)
    , speed("Speed")
    , lastSpeed(NULL)
    , course("Course")
    , lastCourse(NULL)
    , altitude("Altitude")
    , lastAltitude(NULL)
    , satellites("Satellites")
    , eg(xEventGroupCreate())
    , poiState(poiState_t::unknown)
    , parity(0)
    , isChecksumTerm(false)
    , curSentenceType(GPS_SENTENCE_OTHER)
    , curTermNumber(0)
    , curTermOffset(0)
    , sentenceHasFix(false)
    , customElts(0)
    , customCandidates(0)
    , encodedCharCount(0)
    , sentencesWithFixCount(0)
    , failedChecksumCount(0)
    , passedChecksumCount(0)
    , enpin(enpin)
    , curFreqIdx(0)
    , gpTxt(new TinyGPSCustom(*this, "GPTXT", 4)), gpsWarmTime(3), toBeFreqIdx(0), app_eg(getAppEG())
    , skipNext(false)
{
  instance = this;
  AppConfig* stat = AppConfig::GetAppStatus()->GetConfig("gps");
  stat->SetStringProperty("firmware","");
  gpsVersion=stat->GetPropertyHolder("firmware"); 
  gpsStatus=stat->GetConfig("status"); 
  delete stat;
  insertCustom(gpTxt, "GPTXT", 4);
  ESP_ERROR_CHECK(esp_event_handler_instance_register(GPSPLUS_EVENTS, ESP_EVENT_ANY_ID, gpsEventProcessor, this, NULL));
  EventHandlerDescriptor *handler = new EventHandlerDescriptor(GPSPLUS_EVENTS, GPSPLUS_EVENTS);
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::locationChanged, "locationChanged");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::systimeChanged, "systimeChanged");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::significantDistanceChange, "significantDistanceChange");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::significantSpeedChange, "significantSpeedChange");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::significantCourseChange, "significantCourseChange");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::significantAltitudeChange, "significantAltitudeChange");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::rateChanged, "rateChanged");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::msg, "msg");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::sleeping, "sleeping");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::wakingup, "wakingup");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::stop, "stop");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::go, "go");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::atSyncPoint, "atSyncPoint");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::outSyncPoint, "outSyncPoint");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::gpsPaused, "gpsPaused");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::gpsResumed, "gpsResumed");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::gpsRunning, "gpsRunning");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::gpsStopped, "gpsStopped");
  handler->AddEventDescriptor(TinyGPSPlus::gpsEvent::poiChecked, "poiChecked");
  EventManager::RegisterEventHandler(handler);

  term[0] = 0;
  bool woke = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;

  ESP_LOGV(__FUNCTION__, "Initializing GPS Pins");
  ESP_ERROR_CHECK(gpio_hold_dis(enpin));
  ESP_ERROR_CHECK(gpio_reset_pin(enpin));
  ESP_ERROR_CHECK(gpio_set_direction(enpin, GPIO_MODE_OUTPUT));
  for (int idx = 0; idx < numWakePins; idx++){
    ESP_ERROR_CHECK(gpio_set_direction(wakePins[idx], GPIO_MODE_INPUT));
  }

  gpsStart();

  ESP_LOGV(__FUNCTION__, "Initializing UART");

  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0));

  ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, txpin, rxpin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(UART_NUM_2, '\n', 1, 9, 0, 0));
  ESP_ERROR_CHECK(uart_pattern_queue_reset(UART_NUM_2, 20));
  xEventGroupClearBits(eg, gpsEvent::gpsStopped);
  xEventGroupSetBits(eg, gpsEvent::gpsRunning);
  CreateBackgroundTask(uart_event_task, "uart_event_task", 8196, this, tskIDLE_PRIORITY, NULL);
  ESP_LOGV(__FUNCTION__, "UART Initialized");
  xEventGroupClearBits(eg, gpsEvent::msg);
  CreateBackgroundTask(TinyGPSPlus::theLoop, "theLoop", 8196, this, tskIDLE_PRIORITY, NULL);
}

TinyGPSPlus::~TinyGPSPlus()
{
  ESP_LOGI(__FUNCTION__, "Stopping GPS Service");
  gpsStop();
  xEventGroupClearBits(app_eg, app_bits_t::GPS_ON);
  xEventGroupClearBits(eg, gpsEvent::gpsRunning);
  xEventGroupWaitBits(eg, gpsEvent::gpsStopped, pdFALSE, pdTRUE, portMAX_DELAY);
  vEventGroupDelete(eg);
  eg = NULL;
  delete gpsStatus;
  instance = NULL;
}

gpio_num_t TinyGPSPlus::enPin()
{
  return enpin;
}

void TinyGPSPlus::CalcChecksum(uint8_t *Message, uint8_t Length)
{
  uint8_t i;
  uint8_t CK_A, CK_B;

  CK_A = 0;
  CK_B = 0;

  for (i = 2; i < (Length - 2); i++)
  {
    CK_A = CK_A + Message[i];
    CK_B = CK_B + CK_A;
  }

  Message[Length - 2] = CK_A;
  Message[Length - 1] = CK_B;
}

//const uint8_t GPS_ON_OFF[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0XFF, 0X00, 0XFF, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF};

void TinyGPSPlus::flagProtocol(gps_protocol_t protocol, bool state)
{
  ESP_LOGV(__FUNCTION__, "%s protocol %s", state ? "Enabling" : "Disabling", gps_protocol_name[protocol]);
  uint8_t cmd[16];
  memccpy(cmd, GPS_ON_OFF, 0, 16);
  cmd[7] = protocol;
  cmd[9] = state ? 1 : 0;
  CalcChecksum(cmd, 16);
  uint32_t wb = uart_write_bytes(UART_NUM_2, (const char *)cmd, 16);
  if (wb != 16) {
    ESP_LOGW(__FUNCTION__,"Failed setting protocol %d",protocol);
  } else{
    ESP_LOGV(__FUNCTION__, "Wrote %d bytes", wb);
  }
};

void TinyGPSPlus::processEncoded(void)
{
  curTermNumber = 0;
  if (gpTxt->isUpdated() && gpTxt->isValid())
  {
    if (strcmp(gpTxt->value(), "Stopping GPS") == 0)
    {
      ESP_LOGD(__FUNCTION__,"GPS is paused");
      xEventGroupSetBits(eg, gpsEvent::gpsPaused);
      xEventGroupClearBits(eg, gpsEvent::locationChanged|gpsEvent::gpsResumed);
      ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::gpsPaused, NULL, 0, portMAX_DELAY));
    }
    if (strcmp(gpTxt->value(), "Resuming GPS") == 0)
    {
      ESP_LOGD(__FUNCTION__,"GPS Resumed");
      xEventGroupSetBits(eg, gpsEvent::gpsResumed);
      xEventGroupClearBits(eg, gpsEvent::locationChanged|gpsEvent::gpsPaused);
      ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::gpsResumed, NULL, 0, portMAX_DELAY));
    }
  }
  if (date.isValid() && time.isValid() && time.isUpdated() && !(xEventGroupGetBits(getAppEG()) & app_bits_t::WIFI_ON))
  {
    struct tm tm;

    tm.tm_year = date.year() - 1900;
    tm.tm_mon = date.month() - 1;
    tm.tm_mday = date.day();
    tm.tm_hour = time.hour();
    tm.tm_min = time.minute();
    tm.tm_sec = time.second();

    time_t gpsDt = mktime(&tm);
    time_t curTs;
    ::time(&curTs);
    long diff = abs(gpsDt - curTs);

    if ((abs(diff - sleepTimes[curFreqIdx]) > 2) && (diff > 2))
    {
      struct timeval newTs = {tv_sec : gpsDt, tv_usec : 0};
      settimeofday(&newTs, NULL);
      ESP_LOGV(__FUNCTION__, "Time diff:%ld", diff);

      ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::systimeChanged, (void *)&diff, sizeof(long), portMAX_DELAY));
    }

    gettimeofday(&lastMsgTs, NULL);
  }
}

void TinyGPSPlus::uart_event_task(void *pvParameters)
{
  assert(pvParameters);
  TinyGPSPlus *gps = (TinyGPSPlus *)pvParameters;
  uart_event_t event;
  size_t buffered_size;
  uint8_t *dtmp = (uint8_t *)dmalloc(RD_BUF_SIZE);
  int pos = 0;
  uint32_t loopNo = 0;
  int msgLen = 0;
  esp_err_t ret = ESP_OK;
  ESP_LOGD(__FUNCTION__, "uart[%d] starting:", UART_NUM_2);
  EventBits_t bits;
  while (instance && gps->eg && (bits=xEventGroupGetBitsFromISR(gps->eg)) & gpsEvent::gpsRunning)
  {
    //Waiting for UART event.
    if (gps->uart_queue && xQueueReceive(gps->uart_queue, (void *)&event, (portTickType)portMAX_DELAY))
    {
      bzero(dtmp, RD_BUF_SIZE);
      switch (event.type)
      {
      case UART_EVENT_MAX:
        ESP_LOGW(__FUNCTION__, "UART_EVENT_MAX");
        break;
      case UART_DATA_BREAK:
        ESP_LOGW(__FUNCTION__, "UART_DATA_BREAK");
        break;
      case UART_DATA:
        //ESP_LOGV(__FUNCTION__, "[UART DATA]: %d", event.size);
        //ESP_LOGV(__FUNCTION__, "[DATA EVT]:%s(%d)%d",dtmp,event.size,dtmp[event.size-1]);
        break;
      case UART_FIFO_OVF:
        ESP_LOGW(__FUNCTION__, "hw fifo overflow");
        uart_flush_input(UART_NUM_2);
        if (xQueueReset(gps->uart_queue) != pdPASS)
        {
          ESP_LOGE(__FUNCTION__, "Failed to reset UART queue");
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Reset UART queue");
        }
        break;
      //Event of UART ring buffer full
      case UART_BUFFER_FULL:
        ESP_LOGW(__FUNCTION__, "ring buffer full");
        uart_flush_input(UART_NUM_2);
        if (xQueueReset(gps->uart_queue) != pdPASS)
        {
          ESP_LOGE(__FUNCTION__, "Failed to reset UART queue");
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Reset UART queue..");
        }
        break;
      case UART_BREAK:
        ESP_LOGE(__FUNCTION__, "uart rx break");
        break;
      case UART_PARITY_ERR:
        ESP_LOGE(__FUNCTION__, "uart parity error");
        break;
      case UART_FRAME_ERR:
        ESP_LOGE(__FUNCTION__, "uart frame error");
        break;
      case UART_PATTERN_DET:
        loopNo = 0;
        while (((bits=xEventGroupGetBitsFromISR(gps->eg)) & gpsEvent::gpsRunning) &&
                ((ret = uart_get_buffered_data_len(UART_NUM_2, &buffered_size)) ==  ESP_OK) && 
                (buffered_size > 0) &&
                ((pos = uart_pattern_pop_pos(UART_NUM_2)) > 0) &&
                ((msgLen = uart_read_bytes(UART_NUM_2, dtmp, pos, 100 / portTICK_PERIOD_MS)) > 0)) {
          loopNo++;
          if (!gps->skipNext){
            ESP_LOGV(__FUNCTION__, "[UART PATTERN DETECTED%d] pos:%d, buffered size:%d msglen:%d gpsrunning:%d  %s", loopNo, pos, buffered_size, msgLen,bits & gpsEvent::gpsRunning, dtmp);
            for (int idx = 0; idx < msgLen; idx++)
            {
              if (gps->encode(dtmp[idx]))
              {
                ESP_ERROR_CHECK(gps->gps_esp_event_post(gps->GPSPLUS_EVENTS, TinyGPSPlus::gpsEvent::msg, dtmp + 1, msgLen, portMAX_DELAY));
              }
            }
          } else {
            gps->skipNext = false;
          }
        }
        if (loopNo == 0)
        {
          if (ret != ESP_OK) {
            ESP_LOGE(__FUNCTION__,"Error parsing UART:%s",esp_err_to_name(ret));
          }
          if (buffered_size <= 0){
            ESP_LOGW(__FUNCTION__, "Empty buffer");
          } else {
            if (pos <= 0){
              ESP_LOGV(__FUNCTION__, "No pattern in %d bytes",buffered_size);
            } else if (msgLen <= 0){
              ESP_LOGW(__FUNCTION__, "Failed to read %d bytes, read %d",buffered_size, msgLen);
            }
            ret = uart_flush_input(UART_NUM_2);
            if (ret != ESP_OK) {
              ESP_LOGE(__FUNCTION__,"Error flushing UART:%s",esp_err_to_name(ret));
            }
            ESP_LOGD(__FUNCTION__, "Flushed UART");
            gps->skipNext=true;
          }
        }
        break;
      }
    }
  }
  ESP_LOGD(__FUNCTION__, "uart[%d] done:", UART_NUM_2);
  ldfree(dtmp);
  dtmp = NULL;
}

//
// public methods
//

bool TinyGPSPlus::encode(char c)
{
  ++encodedCharCount;

  switch (c)
  {
  case ',': // term terminators
    parity ^= (uint8_t)c;
  case '\r':
  case '\n':
  case '*':
  {
    bool isValidSentence = false;
    if (curTermOffset < sizeof(term))
    {
      term[curTermOffset] = 0;
      isValidSentence = endOfTermHandler();
    }
    ++curTermNumber;
    curTermOffset = 0;
    isChecksumTerm = c == '*';

    return isValidSentence;
  }
  break;

  case '$': // sentence begin
    curTermNumber = curTermOffset = 0;
    parity = 0;
    curSentenceType = GPS_SENTENCE_OTHER;
    isChecksumTerm = false;
    sentenceHasFix = false;
    return false;

  default: // ordinary characters
    if (curTermOffset < sizeof(term) - 1)
      term[curTermOffset++] = c;
    if (!isChecksumTerm)
      parity ^= c;
    return false;
  }

  return false;
}

//
// internal utilities
//
int TinyGPSPlus::fromHex(char a)
{
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else
    return a - '0';
}

// static
// Parse a (potentially negative) number with up to 2 decimal digits -xxxx.yy
int32_t TinyGPSPlus::parseDecimal(const char *term)
{
  bool negative = *term == '-';
  if (negative)
    ++term;
  int32_t ret = 100 * (int32_t)atol(term);
  while (isdigit(*term))
    ++term;
  if (*term == '.' && isdigit(term[1]))
  {
    ret += 10 * (term[1] - '0');
    if (isdigit(term[2]))
      ret += term[2] - '0';
  }
  return negative ? -ret : ret;
}

// static
// Parse degrees in that funny NMEA format DDMM.MMMM
bool TinyGPSPlus::parseDegrees(const char *term, RawDegrees &deg)
{
  uint32_t slen = strlen(term);
  if ((slen < 6) || (slen > 10))
  {
    ESP_LOGV(__FUNCTION__, "Bad Degree to parse:%s len:%d", term, slen);
    return false;
  }
  char ch = 0;
  for (uint32_t idx = 0; idx < slen; idx++)
  {
    ch = *(term + idx);
    if (((ch < '0') && (ch != '.')) ||
        (ch > '9'))
    {
      ESP_LOGV(__FUNCTION__, "Bad Degree to parse:%s", term);
      return false;
    }
  }
  uint32_t leftOfDecimal = (uint32_t)atol(term);
  uint16_t minutes = (uint16_t)(leftOfDecimal % 100);
  uint32_t multiplier = 10000000UL;
  uint32_t tenMillionthsOfMinutes = minutes * multiplier;

  deg.deg = (int16_t)(leftOfDecimal / 100);

  while (isdigit(*term))
    ++term;

  if (*term == '.')
    while (isdigit(*++term))
    {
      multiplier /= 10;
      tenMillionthsOfMinutes += (*term - '0') * multiplier;
    }

  deg.billionths = (5 * tenMillionthsOfMinutes + 1) / 3;
  deg.negative = false;
  return true;
}

#define COMBINE(sentence_type, term_number) (((unsigned)(sentence_type) << 5) | term_number)

esp_err_t TinyGPSPlus::gps_esp_event_post(esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data,
                                          size_t event_data_size,
                                          TickType_t ticks_to_wait)
{
  if (eg) {
    esp_event_post(event_base, event_id, event_data, event_data_size, ticks_to_wait);
    xEventGroupSetBits(eg, event_id);
    if (event_id == gpsEvent::go)
    {
      xEventGroupClearBits(eg, gpsEvent::stop);
    }
    if (event_id == gpsEvent::stop)
    {
      xEventGroupClearBits(eg, gpsEvent::go);
    }
  }
  return ESP_OK;
}

bool TinyGPSPlus::isSignificant()
{
  if (!location.isValid() ||
      !speed.isValid() ||
      !course.isValid() ||
      (location.lat() == location.lng()))
  {
    ESP_LOGD(__FUNCTION__,"invalid loc data location:%d speed:%d lastSpeed:%d course:%d lastCourse:%d",
    location.isValid(),
    speed.isValid(),
    lastSpeed.isValid(),
    course.isValid(),
    lastCourse.isValid());
    return false;
  }

  bool isc = false;

  double val;

  if (!lastLocation.isValid() || ((lastLocation.lat() == lastLocation.lng()) && (lastLocation.lat() == 0.0))){
    lastLocation = location;
    isc=true;
  }
  
  if ((val=distanceBetween(location,lastLocation)) > 50000) {
    ESP_LOGV(__FUNCTION__,"Dodgy gps coords lat:%f lng:%f llat:%f llng:%f",location.lat(),location.lng(),lastLocation.lat(),lastLocation.lng());
    return false;
  }
  if (val > fmax(250, speed.kmph() * 30))
  {
    ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::significantDistanceChange, &val, sizeof(val), portMAX_DELAY));
    isc=true;
  }

  if (!lastCourse.isValid()) {
    lastCourse = course;
    isc=true;
  }

  if (!lastSpeed.isValid()) {
    lastSpeed = speed;
    isc=true;
  }
  val = abs(speed.kmph() - lastSpeed.kmph());
  if (speed.kmph() > 0.0)
  {
    if (lastSpeed.val == 0)
    {
      if (speed.kmph() > 7.0)
      {
        lastSpeed = speed;
        toBeFreqIdx = 0;
        isc = true;
        ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::go, &val, sizeof(val), portMAX_DELAY));
        ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::significantSpeedChange, &val, sizeof(val), portMAX_DELAY));
      }
    }
    else
    {
      if (speed.kmph() < 7.0)
      {
        isc = true;
        ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::stop, &lastSpeed.val, sizeof(lastSpeed.val), portMAX_DELAY));
        ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::significantSpeedChange, &val, sizeof(val), portMAX_DELAY));
        lastSpeed.val = 0;
      }
      else if (val > fmax(1.0, fmin(5.0, speed.kmph() / 20.0)))
      {
        if ((speed.kmph() < 90) && (toBeFreqIdx > 1))
        {
          toBeFreqIdx--;
        }
        lastSpeed = speed;
        isc = true;
        ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::significantSpeedChange, &val, sizeof(val), portMAX_DELAY));
      }
    }
  }

  val = abs(course.deg() - lastCourse.deg());
  if ((speed.kmph() > 10) && (course.deg() != 0.0) && (val > fmin(15, 5 / (100 / speed.kmph()))))
  {
    ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::significantCourseChange, &val, sizeof(val), portMAX_DELAY));
    lastCourse = course;
    toBeFreqIdx = 0;
    isc = true;
  }

  val=abs(altitude.meters() - lastAltitude.meters() );
  if (val > 5) {
    ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS,gpsEvent::significantAltitudeChange,&val,sizeof(val),portMAX_DELAY));
    lastAltitude=altitude;
    //isc= true;
  }

  if (isc)
  {
    lastLocation = location;
  }

  return isc;
}

void TinyGPSPlus::gpsStop()
{
  ESP_LOGD(__FUNCTION__, "Turning off enable pin(%d)", enpin);
  ESP_ERROR_CHECK(gpio_set_level(enpin, 0));
}

void TinyGPSPlus::gpsStart()
{
  ESP_LOGV(__FUNCTION__, "Turning on enable pin(%d)", enpin);
  ESP_ERROR_CHECK(gpio_set_level(enpin, 1));
}

void TinyGPSPlus::gpsResume()
{
  int bw = uart_write_bytes(UART_NUM_2, (const char *)active_tracking, sizeof(active_tracking));
  ESP_ERROR_CHECK(uart_flush(UART_NUM_2));
  ESP_LOGV(__FUNCTION__, "Resumed GPS Output(%d)", bw);
}

void TinyGPSPlus::gpsPause()
{
  int bw = uart_write_bytes(UART_NUM_2, (const char *)silent_tracking, sizeof(silent_tracking) / sizeof(uint8_t));
  ESP_ERROR_CHECK(uart_flush(UART_NUM_2));
  ESP_LOGV(__FUNCTION__, "Paused GPS Output(%d)", bw);
}

void TinyGPSPlus::adjustRate()
{
  if (toBeFreqIdx == curFreqIdx)
    return;

  if (toBeFreqIdx >= (sizeof(sleepTimes) / sizeof(uint8_t)))
  {
    ESP_LOGE(__FUNCTION__, "At top of waitime %d %d", toBeFreqIdx, sizeof(sleepTimes) / sizeof(uint8_t));
    return;
  }

  curFreqIdx = toBeFreqIdx;
  ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::rateChanged, (void *)&sleepTimes[curFreqIdx], sizeof(curFreqIdx), portMAX_DELAY));
}
// Processes a just-completed term
// Returns true if new sentence has just passed checksum test and is validated
bool TinyGPSPlus::endOfTermHandler()
{
  // If it's the checksum term, and the checksum checks out, commit
  if (isChecksumTerm)
  {
    u_short checksum = 16 * fromHex(term[0]) + fromHex(term[1]);
    if (checksum == parity)
    {
      passedChecksumCount++;
      if (sentenceHasFix)
        ++sentencesWithFixCount;

      switch (curSentenceType)
      {
      case GPS_SENTENCE_GPRMC:
        date.commit();
        if (sentenceHasFix)
        {
          location.commit();
          speed.commit();
          course.commit();
        }
        break;
      case GPS_SENTENCE_GPGGA:
        time.commit();
        if (sentenceHasFix)
        {
          location.commit();
          altitude.commit();
        }
        satellites.commit();

        hdop.commit();
        break;
      }

      // Commit all custom listeners of this sentence type
      for (TinyGPSCustom *p = customCandidates; p != NULL && strcmp(p->sentenceName, customCandidates->sentenceName) == 0; p = p->next)
        p->commit();

      if (altitude.isValid() && altitude.isUpdated() && (location.lat() != 0) && (location.lng() != 0) && (location.lat() != location.lng()))
      {
        double dtmp = TinyGPSPlus::distanceBetween(location.lat(), location.lng(), lastLocation.lat(), lastLocation.lng());
        if ((lastLocation.lat() != 0) && (dtmp > 50000)) {
          ESP_LOGV(__FUNCTION__,"Dodgy gps coords lat:%f lng:%f llat:%f llng:%f",location.lat(),location.lng(),lastLocation.lat(),lastLocation.lng());
          return false;
        }
        ESP_ERROR_CHECK(gps_esp_event_post(GPSPLUS_EVENTS, gpsEvent::locationChanged, &dtmp, sizeof(dtmp), portMAX_DELAY));
        AppConfig::SignalStateChange(state_change_t::GPS);
      }
      return true;
    }
    else
    {
      ESP_LOGV(__FUNCTION__, "Bad checksum %d vs %d", checksum, parity);
      ++failedChecksumCount;
    }

    return false;
  }

  // the first term determines the sentence type
  if (curTermNumber == 0)
  {
    if (!strcmp(term, _GPRMCterm) || !strcmp(term, _GNRMCterm))
      curSentenceType = GPS_SENTENCE_GPRMC;
    else if (!strcmp(term, _GPGGAterm) || !strcmp(term, _GNGGAterm))
      curSentenceType = GPS_SENTENCE_GPGGA;
    else if (!strcmp(term, _GPZDAterm))
      curSentenceType = GPS_SENTENCE_GPZDA;
    else if (!strcmp(term, _GPTXTterm))
      curSentenceType = GPS_SENTENCE_GPTXT;
    else
      curSentenceType = GPS_SENTENCE_OTHER;

    // Any custom candidates of this sentence type?
    for (customCandidates = customElts; customCandidates != NULL && strcmp(customCandidates->sentenceName, term) < 0; customCandidates = customCandidates->next)
      ;
    if (customCandidates != NULL && strcmp(customCandidates->sentenceName, term) > 0)
      customCandidates = NULL;

    return false;
  }
  char* msgbuf=NULL;
  char* equal=NULL;
  uint32_t msglen=0;
  if (curSentenceType != GPS_SENTENCE_OTHER && term[0])
    switch (COMBINE(curSentenceType, curTermNumber))
    {
    case COMBINE(GPS_SENTENCE_GPRMC, 1): // Time in both sentences
    case COMBINE(GPS_SENTENCE_GPGGA, 1):
      time.setTime(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 2): // GPRMC validity
      sentenceHasFix = term[0] == 'A';
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 3): // Latitude
    case COMBINE(GPS_SENTENCE_GPGGA, 2):
      if (!location.setLatitude(term))
      {
        ESP_LOGV(__FUNCTION__, "sent:%d term:%d comba:%d combb:%d comp:%d", curSentenceType, curTermNumber, COMBINE(GPS_SENTENCE_GPRMC, 3), COMBINE(GPS_SENTENCE_GPGGA, 2), COMBINE(curSentenceType, curTermNumber));
      }
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 4): // N/S
    case COMBINE(GPS_SENTENCE_GPGGA, 3):
      location.rawNewLatData.negative = term[0] == 'S';
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 5): // Longitude
    case COMBINE(GPS_SENTENCE_GPGGA, 4):
      if (!location.setLongitude(term))
      {
        ESP_LOGV(__FUNCTION__, "sent:%d term:%d comba:%d combb:%d comp:%d", curSentenceType, curTermNumber, COMBINE(GPS_SENTENCE_GPRMC, 5), COMBINE(GPS_SENTENCE_GPGGA, 4), COMBINE(curSentenceType, curTermNumber));
      }
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 6): // E/W
    case COMBINE(GPS_SENTENCE_GPGGA, 5):
      location.rawNewLngData.negative = term[0] == 'W';
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 7): // Speed (GPRMC)
      speed.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 8): // Course (GPRMC)
      course.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 9): // Date (GPRMC)
      date.setDate(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 6): // Fix data (GPGGA)
      sentenceHasFix = term[0] > '0';
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 7): // Satellites used (GPGGA)
      satellites.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 8): // HDOP
      hdop.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPGGA, 9): // Altitude (GPGGA)
      altitude.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GPZDA, 1): // Time (GPZDA)
      ESP_LOGW(__FUNCTION__, "Time:%s", term);
      break;
    case COMBINE(GPS_SENTENCE_GPZDA, 2): // TZ (GPZDA)
      ESP_LOGW(__FUNCTION__, "TZ:%s", term);
      break;
    case COMBINE(GPS_SENTENCE_GPZDA, 3): // Day (GPZDA)
      ESP_LOGW(__FUNCTION__, "Day:%s", term);
      break;
    case COMBINE(GPS_SENTENCE_GPTXT, 3): // Gps Message Type
      ESP_LOGV(__FUNCTION__, "TXTL:%s", term);
      if (strcmp(term,"00")==0) {
        xEventGroupSetBits(eg,gpsEvent::error);
      } else {
        xEventGroupClearBits(eg,gpsEvent::error);
      }
      break;
    case COMBINE(GPS_SENTENCE_GPTXT, 4): // Gps Message
      ESP_LOGV(__FUNCTION__,"%s",term);
      equal=indexOf(term,"=");
      if (equal) {
        *equal=0;
        gpsStatus->SetStringProperty(term,equal+1);
        ESP_LOGV(__FUNCTION__,"%s set to %s",term,equal+1);
        *equal='=';
      } else if (xEventGroupGetBits(eg)&gpsEvent::error) {
        if (xEventGroupGetBitsFromISR(app_eg) & app_bits_t::WIFI_OFF)
          gpsStatus->SetStringProperty("error",term);
      } else if (xEventGroupGetBits(eg)&gpsEvent::initialized) {
        gpsStatus->SetStringProperty("action",term);
      } else {
        msglen=strlen(gpsVersion->valuestring);
        if (msglen) {
          msgbuf=(char*)dmalloc(msglen+strlen(term)+2);
          sprintf(msgbuf,"%s\n%s",gpsVersion->valuestring,term);
        } else {
          msgbuf=(char*)dmalloc(strlen(term)+1);
          strcpy(msgbuf,term);
        }
        cJSON_SetValuestring(gpsVersion,msgbuf);
        ldfree(msgbuf);
      }
      if (startsWith(term,"LLC")) {
        gps_esp_event_post(GPSPLUS_EVENTS,gpsEvent::initialized,NULL,0,portMAX_DELAY);
        ESP_LOGI(__FUNCTION__,"GPS INITIALIZED");
      }
      break;
    }

  // Set custom values as needed
  for (TinyGPSCustom *p = customCandidates;
       p != NULL && p->sentenceName != NULL &&
       strcmp(p->sentenceName, customCandidates->sentenceName) == 0 && p->termNumber <= curTermNumber;
       p = p->next)
    if (p->termNumber == curTermNumber)
      p->set(term);

  return false;
}

double TinyGPSPlus::distanceTo(TinyGPSLocation l1)
{
  return TinyGPSPlus::distanceBetween(this->location, l1);
}

/* static */
double TinyGPSPlus::distanceBetween(TinyGPSLocation l1, TinyGPSLocation l2)
{
  return TinyGPSPlus::distanceBetween(l1.lat(), l1.lng(), l2.lat(), l2.lng());
}

/* static */
double TinyGPSPlus::distanceBetween(double lat1, double long1, double lat2, double long2)
{
  // returns distance in meters between two positions, both specified
  // as signed decimal-degrees latitude and longitude. Uses great-circle
  // distance computation for hypothetical sphere of radius 6372795 meters.
  // Because Earth is no exact sphere, rounding errors may be up to 0.5%.
  // Courtesy of Maarten Lamers
  double delta = ((long1 - long2) * 3.14159265358979323846 / 180.0);
  double sdlong = sin(delta);
  double cdlong = cos(delta);
  lat1 = (lat1 * 3.14159265358979323846 / 180.0);
  lat2 = (lat2 * 3.14159265358979323846 / 180.0);
  double slat1 = sin(lat1);
  double clat1 = cos(lat1);
  double slat2 = sin(lat2);
  double clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = pow(delta, 2);
  delta += pow(clat2 * sdlong, 2);
  delta = sqrt(delta);
  double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  return delta * 6372795;
}

double TinyGPSPlus::courseTo(double lat1, double long1, double lat2, double long2)
{
  // returns course in degrees (North=0, West=270) from position 1 to position 2,
  // both specified as signed decimal-degrees latitude and longitude.
  // Because Earth is no exact sphere, calculated course may be off by a tiny fraction.
  // Courtesy of Maarten Lamers
  double dlon = radians(long2 - long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double a1 = sin(dlon) * cos(lat2);
  double a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  if (a2 < 0.0)
  {
    a2 += 1.57079632679489661923;
  }
  return degrees(a2);
}

const char *TinyGPSPlus::cardinal(double course)
{
  static const char *directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}

void TinyGPSLocation::commit()
{
  rawLatData = rawNewLatData;
  rawLngData = rawNewLngData;
  lastCommitTime = millis();
  valid = updated = true;
  if (jlat)
  {
    cJSON_SetNumberValue(jlat, lat());
    cJSON_SetNumberValue(jlng, lng());
  } else {
    ESP_LOGW(__FUNCTION__,"No GPS long/lat json stat");
  }
}

bool TinyGPSLocation::setLatitude(const char *term)
{
  return TinyGPSPlus::parseDegrees(term, rawNewLatData);
}

bool TinyGPSLocation::setLongitude(const char *term)
{
  return TinyGPSPlus::parseDegrees(term, rawNewLngData);
}

double TinyGPSLocation::lat()
{
  updated = false;
  double ret = rawLatData.deg + rawLatData.billionths / 1000000000.0;
  return rawLatData.negative ? -ret : ret;
}

double TinyGPSLocation::lng()
{
  updated = false;
  double ret = rawLngData.deg + rawLngData.billionths / 1000000000.0;
  return rawLngData.negative ? -ret : ret;
}

void TinyGPSDate::commit()
{
  date = newDate;
  lastCommitTime = millis();
  valid = updated = (year() > 2000) && (year() < 2036);
}

void TinyGPSTime::commit()
{
  time = newTime;
  lastCommitTime = millis();
  valid = updated = time > 0;
}

void TinyGPSTime::setTime(const char *term)
{
  newTime = (uint32_t)TinyGPSPlus::parseDecimal(term);
}

void TinyGPSDate::setDate(const char *term)
{
  newDate = atol(term);
}

uint16_t TinyGPSDate::year()
{
  updated = false;
  uint16_t year = date % 100;
  return year + 2000;
}

uint8_t TinyGPSDate::month()
{
  updated = false;
  return (date / 100) % 100;
}

uint8_t TinyGPSDate::day()
{
  updated = false;
  return date / 10000;
}

uint8_t TinyGPSTime::hour()
{
  updated = false;
  return time / 1000000;
}

uint8_t TinyGPSTime::minute()
{
  updated = false;
  return (time / 10000) % 100;
}

uint8_t TinyGPSTime::second()
{
  updated = false;
  return (time / 100) % 100;
}

uint8_t TinyGPSTime::centisecond()
{
  updated = false;
  return time % 100;
}

void TinyGPSDecimal::commit()
{
  diff = abs(val - newval);
  val = newval;
  lastCommitTime = millis();
  valid = updated = true;
  if (jval)
  {
    cJSON_SetNumberHelper(jval, val);
  }
}

void TinyGPSDecimal::set(const char *term)
{
  newval = TinyGPSPlus::parseDecimal(term);
}

void TinyGPSInteger::commit()
{
  diff = abs((int)val - (int)newval);
  val = newval;
  lastCommitTime = millis();
  valid = updated = true;
  if (jval)
  {
    cJSON_SetNumberValue(jval, val);
  }
}

void TinyGPSInteger::set(const char *term)
{
  newval = atol(term);
}

TinyGPSCustom::TinyGPSCustom(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber)
{
  begin(gps, _sentenceName, _termNumber);
}

void TinyGPSCustom::begin(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber)
{
  lastCommitTime = 0;
  updated = valid = false;
  sentenceName = _sentenceName;
  termNumber = _termNumber;
  memset(stagingBuffer, '\0', sizeof(stagingBuffer));
  memset(buffer, '\0', sizeof(buffer));
  ESP_LOGV(__FUNCTION__, "begin:%s", _sentenceName);

  //  Insert this item into the GPS tree
  //insertCustom(this, _sentenceName, _termNumber);
}

void TinyGPSCustom::commit()
{
  strcpy(this->buffer, this->stagingBuffer);
  lastCommitTime = millis();
  valid = updated = true;
}

void TinyGPSCustom::set(const char *term)
{
  strncpy(this->stagingBuffer, term, sizeof(this->stagingBuffer) - 1);
}

void TinyGPSPlus::insertCustom(TinyGPSCustom *pElt, const char *sentenceName, int termNumber)
{
  TinyGPSCustom **ppelt;

  for (ppelt = &this->customElts; *ppelt != NULL; ppelt = &(*ppelt)->next)
  {
    int cmp = strcmp(sentenceName, (*ppelt)->sentenceName);
    if (cmp < 0 || (cmp == 0 && termNumber < (*ppelt)->termNumber))
      break;
  }

  pElt->next = *ppelt;
  *ppelt = pElt;
}

void gpsSallyForth(void *params)
{
  if (TinyGPSPlus::runningInstance() == NULL)
  {
    AppConfig *appcfg = (AppConfig *)params;
    if (!instance && appcfg->HasProperty("/gps/rxPin") && appcfg->GetIntProperty("/gps/rxPin"))
    {
      ESP_LOGV(__FUNCTION__, "Starting GPS");
      instance = new TinyGPSPlus(appcfg->GetConfig("/gps"));
    }
  }
}