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

#ifndef __TinyGPSPlus_h
#define __TinyGPSPlus_h

//#include "Arduino.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include <limits.h>
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../main/utils.h"
#include "../../main/logs.h"
#include "cJSON.h"
#include "driver/uart.h"

#define _GPS_VERSION "1.0.2" // software version of this library
#define _GPS_MPH_PER_KNOT 1.15077945
#define _GPS_MPS_PER_KNOT 0.51444444
#define _GPS_KMPH_PER_KNOT 1.852
#define _GPS_MILES_PER_METER 0.00062137112
#define _GPS_KM_PER_METER 0.001
#define _GPS_FEET_PER_METER 3.2808399
#define _GPS_MAX_FIELD_SIZE 35
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define BLINK_GPIO GPIO_NUM_5
#define GPS_TIMEOUT 300
#define GPS_WAIT_PERIOD 5
#define SLEEP_TIMEOUT 60
#define PIO_MIN_DIST 200

const uint8_t update_1_secs[] =  {0xB5,0x62,0x06,0x08,0x06,0x00,0xE8,0x03,0x01,0x00,0x01,0x00,0x01,0x39};
const uint8_t update_3_secs[] =  {0xB5,0x62,0x06,0x08,0x06,0x00,0x88,0x13,0x01,0x00,0x01,0x00,0xB1,0x49,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_5_secs[] =  {0xB5,0x62,0x06,0x08,0x06,0x00,0xB8,0x0B,0x01,0x00,0x01,0x00,0xD9,0x41,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_10_secs[] = {0xB5,0x62,0x06,0x08,0x06,0x00,0x10,0x27,0x01,0x00,0x01,0x00,0x4D,0xDD,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_20_secs[] = {0xB5,0x62,0x06,0x08,0x06,0x00,0x20,0x4E,0x01,0x00,0x01,0x00,0x84,0x00,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_0_2_secs[]= {0xB5,0x62,0x06,0x08,0x06,0x00,0x64,0x00,0x01,0x00,0x01,0x00,0x7A,0x12,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t active_tracking[]= {0xB5,0x62,0x06,0x04,0x04,0x00,0x00,0x00,0x02,0x00,0x10,0x68};
const uint8_t silent_tracking[]= {0xB5,0x62,0x06,0x04,0x04,0x00,0x00,0x00,0x08,0x00,0x16,0x74};
const uint8_t gps_off[]=         {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x4D, 0x3B};
const uint8_t gps_power_save[] = {0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x22, 0x92};

const uint8_t GPS_ON_OFF[] =     { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0XFF, 0X00, 0XFF, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF};

const uint8_t sleepTimes[] =     {10,20,60,120,240};

typedef enum {
   GPS_GGA = 0X00,
   GPS_GLL = 0X01,
   GPS_GSA = 0X02,
   GPS_GSV = 0X03,
   GPS_RMC = 0X04,
   GPS_VTG = 0X05,
   GPS_GRS = 0x06,
   GPS_GST = 0x07,
   GPS_ZDA = 0X08
} gps_protocol_t;

//const uint8_t GGA_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0X23};
//const uint8_t GLL_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X2A};
//const uint8_t GSA_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X02, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X01, 0X31};
//const uint8_t GSV_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X03, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X02, 0X38};
//const uint8_t RMC_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X04, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X03, 0X3F};
//const uint8_t VTG_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X05, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X04, 0X46};
//const uint8_t GRS_off[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x4D};
//const uint8_t GST_Off[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x54};
//const uint8_t ZDA_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X08, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X07, 0X5B};
//const uint8_t GGA_On[] =  { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X00, 0X00, 0X01, 0X01, 0X00, 0X00, 0X00, 0X01, 0X2C};


unsigned long IRAM_ATTR millis();

struct RawDegrees
{
   uint16_t deg;
   uint32_t billionths;
   bool negative;
public:
   RawDegrees() : deg(0), billionths(0), negative(false)
   {}
};

struct TinyGPSLocation
{
   friend class TinyGPSPlus;
public:
   bool isValid() const    { return valid; }
   void inValidate()       { valid=false; }
   bool isUpdated() const  { return updated; }
   uint32_t age() const    { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }
   const RawDegrees &rawLat()     { updated = false; return rawLatData; }
   const RawDegrees &rawLng()     { updated = false; return rawLngData; }
   double lat();
   double lng();
   void setLat(RawDegrees val) {rawLatData=val;}
   void setLng(RawDegrees val) {rawLngData=val;}

   TinyGPSLocation()
   {
   }

   TinyGPSLocation(char* name) : valid(false), updated(false)
   {
      AppConfig* stat = AppConfig::GetAppStatus()->GetConfig("gps");
      stat->SetDoubleProperty("Lattitude",0.0);
      stat->SetDoubleProperty("Longitude",0.0);
      jlat = stat->GetPropertyHolder("Lattitude");
      jlng = stat->GetPropertyHolder("Longitude");
      ESP_LOGD(__FUNCTION__,"TinyGPSLocation initializing %d %d",jlat== NULL,jlng == NULL);
      delete stat;
   }

private:
   bool valid, updated;
   RawDegrees rawLatData, rawLngData, rawNewLatData, rawNewLngData;
   uint32_t lastCommitTime;
   void commit();
   bool setLatitude(const char *term);
   bool setLongitude(const char *term);
   cJSON* jlat;
   cJSON* jlng;
};

struct TinyGPSDate
{
   friend class TinyGPSPlus;
public:
   bool isValid() const       { return valid; }
   void inValidate()          { valid=false; }
   bool isUpdated() const     { return updated; }
   uint32_t age() const       { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }

   uint32_t value()           { updated = false; return date; }
   uint16_t year();
   uint8_t month();
   uint8_t day();

   TinyGPSDate() : valid(false), updated(false), date(0)
   {}

private:
   bool valid, updated;
   uint32_t date, newDate;
   uint32_t lastCommitTime;
   void commit();
   void setDate(const char *term);
};

struct TinyGPSTime
{
   friend class TinyGPSPlus;
public:
   bool isValid() const       { return valid; }
   void inValidate()          { valid=false; }
   bool isUpdated() const     { return updated; }
   uint32_t age() const       { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }

   uint32_t value()           { updated = false; return time; }
   uint8_t hour();
   uint8_t minute();
   uint8_t second();
   uint8_t centisecond();

   TinyGPSTime() : valid(false), updated(false), time(0)
   {}

private:
   bool valid, updated;
   uint32_t time, newTime;
   uint32_t lastCommitTime;
   void commit();
   void setTime(const char *term);
};

struct TinyGPSDecimal
{
   friend class TinyGPSPlus;
public:
   bool isValid() const    { return valid; }
   bool isUpdated() const  { return updated; }
   uint32_t age() const    { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }
   int32_t value()         { updated = false; return val; }
   void setValue(uint32_t val) {this->val=val;}

   TinyGPSDecimal(const char* name) : valid(false), updated(false), val(0), name(name)
   {
      jval = NULL;
      if (name) {
         AppConfig* stat = AppConfig::GetAppStatus()->GetConfig("gps");
         stat->SetIntProperty(name,0);
         jval = stat->GetPropertyHolder(name);
         delete stat;
      }
   }

private:
   bool valid, updated;
   uint32_t lastCommitTime;
   int32_t val, newval, diff;
   void commit();
   void set(const char *term);
   const char* name;
   cJSON* jval;
};

struct TinyGPSInteger
{
   friend class TinyGPSPlus;
public:
   bool isValid() const    { return valid; }
   bool isUpdated() const  { return updated; }
   uint32_t age() const    { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }
   uint32_t value()        { updated = false; return val; }

   TinyGPSInteger(const char* name):
      valid(false), 
      updated(false), 
      val(0),
      name(name)
   {
      if (name) {
         AppConfig* stat = AppConfig::GetAppStatus()->GetConfig("gps");
         stat->SetIntProperty(name,0);
         jval = stat->GetJSONConfig(name);
         delete stat;
      }
   }

private:
   bool valid, updated;
   uint32_t lastCommitTime;
   uint32_t val, newval, diff;
   void commit();
   void set(const char *term);
   const char* name;
   cJSON* jval;
};

struct TinyGPSSpeed : TinyGPSDecimal
{
   TinyGPSSpeed(char* name) : TinyGPSDecimal(name) {}
   double knots()    { return value() / 100.0; }
   double mph()      { return _GPS_MPH_PER_KNOT * value() / 100.0; }
   double mps()      { return _GPS_MPS_PER_KNOT * value() / 100.0; }
   double kmph()     { return _GPS_KMPH_PER_KNOT * value() / 100.0; }
};

struct TinyGPSCourse : public TinyGPSDecimal
{
   TinyGPSCourse(char* name) : TinyGPSDecimal(name) {}
   double deg()      { return value() / 100.0; }
};

struct TinyGPSAltitude : TinyGPSDecimal
{
   TinyGPSAltitude(char* name) : TinyGPSDecimal(name) {}
   double meters()       { return value() / 100.0; }
   double miles()        { return _GPS_MILES_PER_METER * value() / 100.0; }
   double kilometers()   { return _GPS_KM_PER_METER * value() / 100.0; }
   double feet()         { return _GPS_FEET_PER_METER * value() / 100.0; }
};

struct TinyGPSHDOP : TinyGPSDecimal
{
   TinyGPSHDOP() : TinyGPSDecimal(NULL) {}
   double hdop() { return value() / 100.0; }
};

class TinyGPSPlus;
class TinyGPSCustom
{
public:
   TinyGPSCustom() {};
   TinyGPSCustom(TinyGPSPlus &gps, const char *sentenceName, int termNumber);
   void begin(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber);

   bool isUpdated() const  { return updated; }
   bool isValid() const    { return valid; }
   uint32_t age() const    { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }
   const char *value()     { updated = false; return buffer; }

private:
   void commit();
   void set(const char *term);

   char stagingBuffer[_GPS_MAX_FIELD_SIZE + 1];
   char buffer[_GPS_MAX_FIELD_SIZE + 1];
   unsigned long lastCommitTime;
   bool valid, updated;
   const char *sentenceName;
   int termNumber;
   friend class TinyGPSPlus;
   TinyGPSCustom *next;
};

enum poiState_t {
   unknown = BIT0,
   in = BIT1,
   out = BIT2
};

class TinyGPSPlus
{
public:
  TinyGPSPlus(AppConfig* config);
  TinyGPSPlus(gpio_num_t rxpin, gpio_num_t txpin, gpio_num_t enpin);
  ~TinyGPSPlus();
  static TinyGPSPlus* runningInstance(); 
  bool encode(char c); // process one character received from GPS
  TinyGPSPlus &operator << (char c) {encode(c); return *this;}

  uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      .use_ref_tick = true};

  TinyGPSLocation location;
  TinyGPSLocation lastLocation;
  TinyGPSDate date;
  TinyGPSTime time;
  TinyGPSSpeed speed;
  TinyGPSSpeed lastSpeed;
  TinyGPSCourse course;
  TinyGPSCourse lastCourse;
  TinyGPSAltitude altitude;
  TinyGPSAltitude lastAltitude;
  TinyGPSInteger satellites;
  TinyGPSHDOP hdop;
  
  static const char *libraryVersion() { return _GPS_VERSION; }
  void processEncoded(void);

  double distanceTo(TinyGPSLocation l1);
  static double distanceBetween(TinyGPSLocation l1,TinyGPSLocation l2);
  static double distanceBetween(double lat1, double long1, double lat2, double long2);
  static double courseTo(double lat1, double long1, double lat2, double long2);
  static const char *cardinal(double course);

  static int32_t parseDecimal(const char *term);
  static bool parseDegrees(const char *term, RawDegrees &deg);

  uint32_t charsProcessed()   const { return encodedCharCount; }
  uint32_t sentencesWithFix() const { return sentencesWithFixCount; }
  uint32_t failedChecksum()   const { return failedChecksumCount; }
  uint32_t passedChecksum()   const { return passedChecksumCount; }

  typedef struct
  {
     TinyGPSPlus* gps;
     bool lightSleep;
  } sleepCallParam_t;


  ESP_EVENT_DEFINE_BASE(GPSPLUS_EVENTS);
  static void theLoop(void* param);
  struct timeval lastMsgTs;
  enum gpsEvent {
     locationChanged=BIT0,
     systimeChanged=BIT1,
     significantDistanceChange=BIT2,
     significantSpeedChange=BIT3,
     significantCourseChange=BIT4,
     significantAltitudeChange=BIT5,
     rateChanged=BIT6,
     msg=BIT7,
     sleeping=BIT8,
     wakingup=BIT9,
     stop=BIT10,
     go=BIT11,
     atSyncPoint=BIT12,
     outSyncPoint=BIT13,
     gpsPaused=BIT14,
     gpsResumed=BIT15,
     gpsRunning=BIT16,
     gpsStopped=BIT17,
     poiChecked=BIT18,
     initialized=BIT19,
     error=BIT20
  };
  void gpsResume();
  void gpsPause();
  void gpsStop();
  void gpsStart();
  uint8_t stackTask();
  gpio_num_t enPin();
  void unStackTask(uint8_t taskHandle);
  uint32_t getSleepTime();
  void flagProtocol(gps_protocol_t protocol, bool state);
  esp_err_t gps_esp_event_post(esp_event_base_t event_base,
                            int32_t event_id,
                            void* event_data,
                            size_t event_data_size,
                            TickType_t ticks_to_wait);
  EventGroupHandle_t eg;
  poiState_t poiState;

private:
  void Init();
  void checkPOIs();
  static void gpsEventProcessor(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
  enum {GPS_SENTENCE_GPGGA, GPS_SENTENCE_GPRMC, GPS_SENTENCE_GPZDA, GPS_SENTENCE_GPTXT, GPS_SENTENCE_OTHER};
  static QueueHandle_t uart0_queue;
  static void uart_event_task(void *pvParameters);
  esp_timer_handle_t periodic_timer;
  void adjustRate();
  void CalcChecksum(uint8_t *Message, uint8_t Length);
  static void waitOnStop(void* gps);

  QueueHandle_t uart_queue;

  // parsing state variables
  uint8_t parity;
  bool isChecksumTerm;
  char term[_GPS_MAX_FIELD_SIZE];
  uint8_t curSentenceType;
  uint8_t curTermNumber;
  uint8_t curTermOffset;
  bool sentenceHasFix;

  // custom element support
  friend class TinyGPSCustom;
  TinyGPSCustom *customElts;
  TinyGPSCustom *customCandidates;
  void insertCustom(TinyGPSCustom *pElt, const char *sentenceName, int index);

  // statistics
  uint32_t encodedCharCount;
  uint32_t sentencesWithFixCount;
  uint32_t failedChecksumCount;
  uint32_t passedChecksumCount;
  gpio_num_t enpin;
  uint8_t curFreqIdx;
  TinyGPSCustom* gpTxt;
  uint8_t gpsWarmTime;
  uint8_t toBeFreqIdx;
  EventGroupHandle_t app_eg;
  AppConfig* gpsStatus;
  cJSON* gpsVersion;
  bool skipNext;

  // internal utilities
  int fromHex(char a);
  bool endOfTermHandler();
  bool isSignificant();
};

void gpsSallyForth(void*);

#endif // def(__TinyGPSPlus_h)
