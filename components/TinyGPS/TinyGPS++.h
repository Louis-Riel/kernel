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

#define _GPS_VERSION "1.0.2" // software version of this library
#define _GPS_MPH_PER_KNOT 1.15077945
#define _GPS_MPS_PER_KNOT 0.51444444
#define _GPS_KMPH_PER_KNOT 1.852
#define _GPS_MILES_PER_METER 0.00062137112
#define _GPS_KM_PER_METER 0.001
#define _GPS_FEET_PER_METER 3.2808399
#define _GPS_MAX_FIELD_SIZE 15
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define BLINK_GPIO GPIO_NUM_5

const uint8_t update_1_secs[] =  {0xB5,0x62,0x06,0x08,0x06,0x00,0xE8,0x03,0x01,0x00,0x01,0x00,0x01,0x39};
const uint8_t update_3_secs[] =  {0xB5,0x62,0x06,0x08,0x06,0x00,0x88,0x13,0x01,0x00,0x01,0x00,0xB1,0x49,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_5_secs[] =  {0xB5,0x62,0x06,0x08,0x06,0x00,0xB8,0x0B,0x01,0x00,0x01,0x00,0xD9,0x41,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_10_secs[] = {0xB5,0x62,0x06,0x08,0x06,0x00,0x10,0x27,0x01,0x00,0x01,0x00,0x4D,0xDD,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_20_secs[] = {0xB5,0x62,0x06,0x08,0x06,0x00,0x20,0x4E,0x01,0x00,0x01,0x00,0x84,0x00,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t update_0_2_secs[]= {0xB5,0x62,0x06,0x08,0x06,0x00,0x64,0x00,0x01,0x00,0x01,0x00,0x7A,0x12,0xB5,0x62,0x06,0x08,0x00,0x00,0x0E,0x30};
const uint8_t active_tracking[] =         {0xB5,0x62,0x06,0x04,0x04,0x00,0x00,0x00,0x02,0x00,0x10,0x68};
const uint8_t silent_tracking[] =    {0xB5,0x62,0x06,0x04,0x04,0x00,0x00,0x00,0x08,0x00,0x16,0x74};
const uint8_t RMC_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X04, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X03, 0X3F};
const uint8_t VTG_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X05, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X04, 0X46};
const uint8_t GSA_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X02, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X01, 0X31};
const uint8_t GSV_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X03, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X02, 0X38};
const uint8_t GLL_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X01, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X2A};
const uint8_t GGA_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0X23};
const uint8_t GGA_On[] =  { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X00, 0X00, 0X01, 0X01, 0X00, 0X00, 0X00, 0X01, 0X2C};
const uint8_t ZDA_Off[] = { 0XB5, 0X62, 0X06, 0X01, 0X08, 0X00, 0XF0, 0X08, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X07, 0X5B};
const uint8_t sleepTimes[] =     {1,3,5,10,20};

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

   TinyGPSLocation() : valid(false), updated(false)
   {}

private:
   bool valid, updated;
   RawDegrees rawLatData, rawLngData, rawNewLatData, rawNewLngData;
   uint32_t lastCommitTime;
   void commit();
   void setLatitude(const char *term);
   void setLongitude(const char *term);
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

   TinyGPSDecimal() : valid(false), updated(false), val(0)
   {}

private:
   bool valid, updated;
   uint32_t lastCommitTime;
   int32_t val, newval, diff;
   void commit();
   void set(const char *term);
};

struct TinyGPSInteger
{
   friend class TinyGPSPlus;
public:
   bool isValid() const    { return valid; }
   bool isUpdated() const  { return updated; }
   uint32_t age() const    { return valid ? millis() - lastCommitTime : (uint32_t)ULONG_MAX; }
   uint32_t value()        { updated = false; return val; }

   TinyGPSInteger() : valid(false), updated(false), val(0)
   {}

private:
   bool valid, updated;
   uint32_t lastCommitTime;
   uint32_t val, newval, diff;
   void commit();
   void set(const char *term);
};

struct TinyGPSSpeed : TinyGPSDecimal
{
   double knots()    { return value() / 100.0; }
   double mph()      { return _GPS_MPH_PER_KNOT * value() / 100.0; }
   double mps()      { return _GPS_MPS_PER_KNOT * value() / 100.0; }
   double kmph()     { return _GPS_KMPH_PER_KNOT * value() / 100.0; }
};

struct TinyGPSCourse : public TinyGPSDecimal
{
   double deg()      { return value() / 100.0; }
};

struct TinyGPSAltitude : TinyGPSDecimal
{
   double meters()       { return value() / 100.0; }
   double miles()        { return _GPS_MILES_PER_METER * value() / 100.0; }
   double kilometers()   { return _GPS_KM_PER_METER * value() / 100.0; }
   double feet()         { return _GPS_FEET_PER_METER * value() / 100.0; }
};

struct TinyGPSHDOP : TinyGPSDecimal
{
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

class TinyGPSPlus
{
public:
  TinyGPSPlus(gpio_num_t rxpin, gpio_num_t txpin, gpio_num_t enpin,uint8_t lastSleepIdx,
              RawDegrees llat, RawDegrees llng, uint32_t course, uint32_t speed, uint32_t altitude);
  bool encode(char c); // process one character received from GPS
  TinyGPSPlus &operator << (char c) {encode(c); return *this;}

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
  gpio_num_t enpin;
  uint8_t curFreqIdx;
  QueueHandle_t runnerSemaphore = xSemaphoreCreateCounting(10,0);

  static const char *libraryVersion() { return _GPS_VERSION; }
  void processEncoded(void);

  double distanceTo(TinyGPSLocation l1);
  static double distanceBetween(TinyGPSLocation l1,TinyGPSLocation l2);
  static double distanceBetween(double lat1, double long1, double lat2, double long2);
  static double courseTo(double lat1, double long1, double lat2, double long2);
  static const char *cardinal(double course);

  static int32_t parseDecimal(const char *term);
  static void parseDegrees(const char *term, RawDegrees &deg);

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
  static void gotoSleep(void* param);
  struct timeval lastMsgTs;
  struct timeval lastMsgTsSent;
  enum gpsEvent {
     locationChanged,
     systimeChanged,
     significantDistanceChange,
     significantSpeedChange,
     significantCourseChange,
     significantAltitudeChange,
     rateChanged,
     msg,
     sleeping,
     wakingup,
     stop,
     go,
     atSyncPoint,
     outSyncPoint
  };
  void gpsResume();
  void gpsPause();
  TaskHandle_t gpspingTask;
  TaskHandle_t runners[255];
  uint8_t numRunners;
  uint8_t stackTask();
  void unStackTask(uint8_t taskHandle);
  uint32_t getSleepTime();

private:
  enum {GPS_SENTENCE_GPGGA, GPS_SENTENCE_GPRMC, GPS_SENTENCE_OTHER};
  static QueueHandle_t uart0_queue;
  static void flash(void *pvParameters);
  static void uart_event_task(void *pvParameters);
  esp_timer_handle_t periodic_timer;
  uint8_t toBeFreqIdx;
  void adjustRate();

  QueueHandle_t uart_queue;

  static inline IRAM_ATTR BaseType_t myTaskCreate(
			TaskFunction_t pvTaskCode,
			const char * const pcName,
			const uint32_t usStackDepth,
			void * const pvParameters,
			UBaseType_t uxPriority,
			TaskHandle_t * const pvCreatedTask);

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

  // internal utilities
  int fromHex(char a);
  bool endOfTermHandler();
  bool isSignificant();
};

#endif // def(__TinyGPSPlus_h)
