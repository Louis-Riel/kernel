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
#include "driver/uart.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/timers.h"
#include "esp_sleep.h"

#define _GPRMCterm   "GPRMC"
#define _GPGGAterm   "GPGGA"
#define _GNRMCterm   "GNRMC"
#define _GNGGAterm   "GNGGA"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

unsigned long IRAM_ATTR millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000LL);
}

double radians(double angle) {
  return (angle*3.14159265358979323846/180.0);
}

double degrees(double angle) {
  return (angle*180.0/3.14159265358979323846);
}

uint32_t TinyGPSPlus::getSleepTime(){
  return sleepTimes[curFreqIdx];
}


void TinyGPSPlus::gotoSleep(void* param) {
  TinyGPSPlus* gps = (TinyGPSPlus*)param;
  if (gps->curFreqIdx>=1)
  {
    ESP_LOGV(__FUNCTION__,"posting sleeping msg");
    esp_event_post_to(gps->loop_handle,gps->GPSPLUS_EVENTS,TinyGPSPlus::gpsEvent::sleeping,NULL,0,portMAX_DELAY);
    //ESP_ERROR_CHECK(esp_sleep_enable_uart_wakeup(2));
    gps->gpsPause();

    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleepTimes[gps->curFreqIdx]*1000000));
    ESP_LOGD(__FUNCTION__,"Napping for %d", sleepTimes[gps->curFreqIdx]);
    ESP_ERROR_CHECK(esp_light_sleep_start());
    esp_event_post_to(gps->loop_handle,gps->GPSPLUS_EVENTS,TinyGPSPlus::gpsEvent::wakingup,NULL,0,portMAX_DELAY);
    gps->gpsResume();
  } else {
    ESP_LOGD(__FUNCTION__,"No sleep till Brookland, cause curfreq is less than zero");
  }

  gps->gpspingTask=NULL;
  vTaskDelete(NULL);
}

TinyGPSPlus::TinyGPSPlus(gpio_num_t rxpin, gpio_num_t txpin, gpio_num_t enpin, uint8_t lastSleepIdx,
                         RawDegrees llat, RawDegrees llng, uint32_t course, uint32_t speed,uint32_t altitude)
  :  parity(0)
  ,  isChecksumTerm(false)
  ,  curSentenceType(GPS_SENTENCE_OTHER)
  ,  curTermNumber(0)
  ,  curTermOffset(0)
  ,  sentenceHasFix(false)
  ,  customElts(0)
  ,  customCandidates(0)
  ,  encodedCharCount(0)
  ,  sentencesWithFixCount(0)
  ,  failedChecksumCount(0)
  ,  passedChecksumCount(0)
{
  esp_log_level_set("uart_event_task",ESP_LOG_VERBOSE);

  lastLocation.rawLatData=llat;
  lastLocation.rawLngData=llng;
  lastCourse.val=course;
  lastSpeed.val=speed;
  lastAltitude.val=altitude;

  numRunners=0;
  gpspingTask=NULL;
  term[0]=0;
  this->enpin=enpin;
  curFreqIdx=0;
  toBeFreqIdx=0;
  bool woke = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;

  ESP_LOGD(__FUNCTION__, "Initializing GPS");

  if (!woke) {
    ESP_LOGD(__FUNCTION__, "Initializing GPS Pins");
    ESP_ERROR_CHECK(gpio_hold_dis(enpin));
    ESP_ERROR_CHECK(gpio_reset_pin(enpin));
    ESP_ERROR_CHECK(gpio_set_direction(enpin, GPIO_MODE_OUTPUT));
  }

  ESP_LOGD(__FUNCTION__, "Turning on enable pin");
  ESP_ERROR_CHECK(gpio_set_level(enpin,1));

  esp_event_loop_args_t loop_args = {
      .queue_size = 5,
      .task_name = "GPSLoop",
      .task_priority = 12,
      .task_stack_size = 4096,
      .task_core_id = tskNO_AFFINITY
  };
  memset(runners,0,sizeof(runners));

  ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &loop_handle));

  ESP_LOGD(__FUNCTION__, "Initializing UART");

  uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      .use_ref_tick = true
  };
  ESP_LOGD(__FUNCTION__, "Turning on uart");
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, txpin, rxpin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  //ESP_ERROR_CHECK(uart_enable_pattern_det_intr(UART_NUM_2, '\n', 1, 10000, 10, 10));
  ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(UART_NUM_2, '\n', 1, 9, 0, 0));

  ESP_ERROR_CHECK(uart_pattern_queue_reset(UART_NUM_2, 20));
  xTaskCreate(uart_event_task, "uart_event_task", 2048, this, 12, NULL);
  ESP_LOGD(__FUNCTION__, "UART Initialized");

  if (woke) {
    gpsResume();
    curFreqIdx=lastSleepIdx;
    toBeFreqIdx=lastSleepIdx;
    lastCourse.setValue(course);
    lastSpeed.setValue(speed);
    lastLocation.setLat(llat);
    lastLocation.setLng(llng);
    ESP_LOGD(__FUNCTION__,"llng:%3.7f,llat:%3.7f,lspeed:%f, lcourse:%f",
      lastLocation.lng(),
      lastLocation.lat(),
      lastSpeed.kmph(),
      lastCourse.deg());
  } else {
    int wb = uart_write_bytes(UART_NUM_2, (const char*) update_0_2_secs, 16);
    ESP_LOGD(__FUNCTION__,"Sent freq scaledown command (%d bytes), ret %d bytes",16,wb);
    //const uint8_t* inits[7] = {VTG_Off,GSA_Off,GSV_Off,GLL_Off,GGA_On,ZDA_Off,update_0_2_secs};
    //for (int idx=0; idx < 7; idx++){
    //  wb = uart_write_bytes(UART_NUM_2, (const char*) inits[idx], 16);
    //  ESP_LOGD(__FUNCTION__,"Sent init command %d with %d bytes, ret %d bytes",idx,16,wb);
    //}
    curFreqIdx=0;
    toBeFreqIdx=0;
  }

  ESP_LOGD(__FUNCTION__, "GPS Initialized");

}

void TinyGPSPlus::processEncoded(void)
{
  curTermNumber=0;
  if (date.isValid() && time.isValid() && time.isUpdated())
  {
    struct tm tm;

    tm.tm_year = date.year()-1900;
    tm.tm_mon = date.month()-1;
    tm.tm_mday = date.day();
    tm.tm_hour = time.hour();
    tm.tm_min = time.minute();
    tm.tm_sec = time.second();

    struct timeval now = { .tv_sec=mktime(&tm),
                           .tv_usec=0 };
    time_t cutDt;

    ::time(&cutDt);
    struct timeval tv2;
    gettimeofday(&tv2,NULL);

    if ((tm.tm_mon >= 2) && (tm.tm_mon <= 11)){
      tv2.tv_sec+=(4*60*60);
    } else {
      tv2.tv_sec+=(5*60*60);
    }

    if (abs(tv2.tv_sec-now.tv_sec)>2){
      ESP_LOGD(__FUNCTION__,"Time diff:%li gps:%li cur:%li",abs(tv2.tv_sec-now.tv_sec),tv2.tv_sec,now.tv_sec);
      timezone tz;
      tz.tz_dsttime=0;
      tz.tz_minuteswest=0;
      if (cutDt < 10000){
        setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
        tzset();
        settimeofday(&now, &tz);
      } else {
        adjtime(NULL,&now);
      }
      esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::systimeChanged,NULL,0,portMAX_DELAY);
    }

    gettimeofday(&lastMsgTs, NULL);
  }
}

void TinyGPSPlus::uart_event_task(void *pvParameters)
{

    TinyGPSPlus* gps = (TinyGPSPlus*)pvParameters;
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    ESP_LOGD(__FUNCTION__, "uart[%d] starting:", UART_NUM_2);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(gps->uart_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            switch(event.type) {
                case UART_EVENT_MAX:
                    ESP_LOGW(__FUNCTION__, "UART_EVENT_MAX");
                    break;
                case UART_DATA_BREAK:
                    ESP_LOGW(__FUNCTION__, "UART_DATA_BREAK");
                    break;
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    //ESP_LOGV(__FUNCTION__, "[UART DATA]: %d", event.size);
                    //ESP_LOGV(__FUNCTION__, "[DATA EVT]:%s(%d)%d",dtmp,event.size,dtmp[event.size-1]);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGW(__FUNCTION__, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(gps->uart_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGW(__FUNCTION__, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(gps->uart_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGE(__FUNCTION__, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGE(__FUNCTION__, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGE(__FUNCTION__, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(UART_NUM_2, &buffered_size);
                    int pos = uart_pattern_pop_pos(UART_NUM_2);
                    ESP_LOGV(__FUNCTION__, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(UART_NUM_2);
                    } else {
                        int msgLen = uart_read_bytes(UART_NUM_2, dtmp, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        if (msgLen > 1){

                          for (int idx=1; idx < msgLen; idx++) {
                            if (gps->encode(dtmp[idx])){
                              esp_event_post_to(gps->loop_handle,gps->GPSPLUS_EVENTS,TinyGPSPlus::gpsEvent::msg,dtmp+1,msgLen,portMAX_DELAY);
                              gps->processEncoded();
                            }
                          }
                        }
                    }
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

//
// public methods
//

bool TinyGPSPlus::encode(char c)
{
  ++encodedCharCount;

  switch(c)
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
  if (negative) ++term;
  int32_t ret = 100 * (int32_t)atol(term);
  while (isdigit(*term)) ++term;
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
void TinyGPSPlus::parseDegrees(const char *term, RawDegrees &deg)
{
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
}

#define COMBINE(sentence_type, term_number) (((unsigned)(sentence_type) << 5) | term_number)

bool TinyGPSPlus::isSignificant()
{
  bool isc=false;
  double val=abs(speed.kmph() - lastSpeed.kmph());
  if (speed.kmph() > 0.0) {
    if (lastSpeed.val == 0){
      if (speed.kmph() > 4.0){
        lastSpeed=speed;
        isc= true;
        esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::go,&val,sizeof(val),portMAX_DELAY);
        esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::significantSpeedChange,&val,sizeof(val),portMAX_DELAY);
      }
    } else {
      if (speed.kmph() < 4.0){
        if (lastSpeed.val != 0){
          if (lastSpeed.kmph() > 4.0){
            isc= true;
            esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::stop,&lastSpeed.val,sizeof(lastSpeed.val),portMAX_DELAY);
            esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::significantSpeedChange,&val,sizeof(val),portMAX_DELAY);
          }
          lastSpeed.val = 0;
        }
      } else if (val > fmax(1.0,fmin(5.0,speed.kmph()/20.0))) {
        lastSpeed=speed;
        isc= true;
        esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::significantSpeedChange,&val,sizeof(val),portMAX_DELAY);
      }
    }
  }

  val=abs(course.deg() - lastCourse.deg());
  if ((speed.kmph() > 5) && (course.deg() != 0.0) && (val > 5)) {
    esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::significantCourseChange,&val,sizeof(val),portMAX_DELAY);
    lastCourse=course;
    isc= true;
  }

  //val=abs(altitude.meters() - lastAltitude.meters() );
  //if (val > 5) {
  //  esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::significantAltitudeChange,&val,sizeof(val),portMAX_DELAY);
  //  lastAltitude=altitude;
  //  isc= true;
  //}

  if (isc){
    lastLocation=location;
  }

  val=distanceTo(lastLocation);
  if (val>fmax(250,speed.kmph()*50)) {
    esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::significantDistanceChange,&val,sizeof(val),portMAX_DELAY);
    lastLocation=location;
  }

  return isc;
}

void TinyGPSPlus::gpsResume(){
  int bw = uart_write_bytes(UART_NUM_2, (const char*) active_tracking, sizeof(active_tracking));
  ESP_ERROR_CHECK(uart_flush(UART_NUM_2));
  ESP_LOGD(__FUNCTION__,"Resumed GPS Output(%d)",bw);
}

void TinyGPSPlus::gpsPause(){
  int bw = uart_write_bytes(UART_NUM_2, (const char*) silent_tracking, sizeof(silent_tracking));
  ESP_ERROR_CHECK(uart_flush(UART_NUM_2));
  ESP_LOGD(__FUNCTION__,"Paused GPS Output(%d)",bw);
}

void TinyGPSPlus::adjustRate(){
    if (toBeFreqIdx == curFreqIdx)
      return;
    switch (toBeFreqIdx)
    {
    case 0:
      //uart_write_bytes(UART_NUM_2, (const char*) update_1_secs, sizeof(update_1_secs));
      break;
    case 1:
      //uart_write_bytes(UART_NUM_2, (const char*) update_3_secs, sizeof(update_3_secs));
      break;
    case 2:
      //uart_write_bytes(UART_NUM_2, (const char*) update_5_secs, sizeof(update_5_secs));
      break;
    case 3:
      //uart_write_bytes(UART_NUM_2, (const char*) update_10_secs, sizeof(update_10_secs));
      break;
    case 4:
      //uart_write_bytes(UART_NUM_2, (const char*) update_20_secs, sizeof(update_20_secs));
      break;
    default:
      return;
      break;
    }
    curFreqIdx=toBeFreqIdx;
    esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::rateChanged,(void*)&sleepTimes[curFreqIdx],sizeof(curFreqIdx),portMAX_DELAY);
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

      switch(curSentenceType)
      {
      case GPS_SENTENCE_GPRMC:
        date.commit();
        time.commit();
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

      if (altitude.isValid() && altitude.isUpdated()){
        //toBeFreqIdx=curFreqIdx;
        if (isSignificant()){
          if (toBeFreqIdx>0){
            toBeFreqIdx--;
          }
        } else {
          if (toBeFreqIdx<4){
            toBeFreqIdx++;
          }
        }
        adjustRate();
        esp_event_post_to(loop_handle,GPSPLUS_EVENTS,gpsEvent::locationChanged,NULL,0,portMAX_DELAY);
      }
      return true;
    }
    else
    {
      ESP_LOGD(__FUNCTION__,"Bad checksum %d vs %d",checksum, parity);
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
    else
      curSentenceType = GPS_SENTENCE_OTHER;

    // Any custom candidates of this sentence type?
    for (customCandidates = customElts; customCandidates != NULL && strcmp(customCandidates->sentenceName, term) < 0; customCandidates = customCandidates->next);
    if (customCandidates != NULL && strcmp(customCandidates->sentenceName, term) > 0)
       customCandidates = NULL;

    return false;
  }
  if (curSentenceType != GPS_SENTENCE_OTHER && term[0])
    switch(COMBINE(curSentenceType, curTermNumber))
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
      location.setLatitude(term);
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 4): // N/S
    case COMBINE(GPS_SENTENCE_GPGGA, 3):
      location.rawNewLatData.negative = term[0] == 'S';
      break;
    case COMBINE(GPS_SENTENCE_GPRMC, 5): // Longitude
    case COMBINE(GPS_SENTENCE_GPGGA, 4):
      location.setLongitude(term);
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
  }

  // Set custom values as needed
  for (TinyGPSCustom *p = customCandidates; p != NULL && strcmp(p->sentenceName, customCandidates->sentenceName) == 0 && p->termNumber <= curTermNumber; p = p->next)
    if (p->termNumber == curTermNumber)
         p->set(term);

  return false;
}

double TinyGPSPlus::distanceTo(TinyGPSLocation l1)
{
  return TinyGPSPlus::distanceBetween(this->location,l1);
}

/* static */
double TinyGPSPlus::distanceBetween(TinyGPSLocation l1,TinyGPSLocation l2)
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
  double delta = ((long1-long2)*3.14159265358979323846/180.0);
  double sdlong = sin(delta);
  double cdlong = cos(delta);
  lat1 = (lat1*3.14159265358979323846/180.0);
  lat2 = (lat2*3.14159265358979323846/180.0);
  double slat1 = sin(lat1);
  double clat1 = cos(lat1);
  double slat2 = sin(lat2);
  double clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = pow(delta,2);
  delta += pow(clat2 * sdlong,2);
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
  double dlon = radians(long2-long1);
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
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}

void TinyGPSLocation::commit()
{
   rawLatData = rawNewLatData;
   rawLngData = rawNewLngData;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSLocation::setLatitude(const char *term)
{
   TinyGPSPlus::parseDegrees(term, rawNewLatData);
}

void TinyGPSLocation::setLongitude(const char *term)
{
   TinyGPSPlus::parseDegrees(term, rawNewLngData);
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
   valid = updated = (year()>2000);
}

void TinyGPSTime::commit()
{
   time = newTime;
   lastCommitTime = millis();
   valid = updated = true;
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
   diff=abs(val-newval);
   val = newval;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSDecimal::set(const char *term)
{
   newval = TinyGPSPlus::parseDecimal(term);
}

void TinyGPSInteger::commit()
{
   diff=abs((int)val-(int)newval);
   val = newval;
   lastCommitTime = millis();
   valid = updated = true;
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
   ESP_LOGD(__FUNCTION__,"begin:%s",_sentenceName);

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
   strncpy(this->stagingBuffer, term, sizeof(this->stagingBuffer));
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
