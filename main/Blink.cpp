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
#include "eventmgr.h"
#include "rest.h"
#include "route.h"
#include "../components/pins/pins.h"
#include "mfile.h"
#include "../components/IR/ir.h"
#include "../components/bluetooth/bt.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define BLINK_GPIO GPIO_NUM_5
#define GPS_EN_PIN GPIO_NUM_13
#define NUM_VOLT_CYCLE 20

#define TRIP_BLOCK_SIZE 255

static xQueueHandle gpio_evt_queue = NULL;
uint64_t now = 0;
uint64_t lastMovement = 0;
uint64_t lastLocTs = 0;
uint64_t lastSLocTs = 0;
uint64_t tv_sleep;
uint64_t startTs=0;
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
bool gpsto = false;
bool sgpsto = false;
bool buto = false;
bool sito = false;
bool boto = false;
bool dto = false;
char cctmp[70];
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

TinyGPSPlus *gps = NULL;
esp_adc_cal_characteristics_t characteristics;
uint16_t timeout = GPS_TIMEOUT;
uint64_t timeoutMicro = GPS_TIMEOUT * 1000000;

TaskHandle_t hybernator = NULL;

extern "C"
{
  void app_main(void);
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
    if ((ret = esp_adc_cal_get_voltage(ADC_CHANNEL_7, &characteristics, &tmp)) == ESP_OK)
    {
      voltage += (tmp * 2);
      cnt++;
    }
    else
    {
      ESP_LOGW(__FUNCTION__, "Error getting voltage %s", esp_err_to_name(ret));
    }
  }
  adc_power_release();
  if (cnt > 0)
  {
    batLvls[batSmplCnt++] = (voltage / cnt);
    ESP_LOGV(__FUNCTION__, "Voltage::%f", getBatteryVoltage());
  }
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

void doHibernate(void *param)
{
  ESP_LOGV(__FUNCTION__, "Deep Sleeping %d", bumpCnt);
  BufferedFile::FlushAll();
  hibernate = true;
  uint64_t ext_wakeup_on_pin_mask = 0;
  uint64_t ext_wakeup_off_pin_mask = 0;
  int curLvl = 0;
  for (int idx = 0; idx < numWakePins; idx++)
  {
    curLvl = gpio_get_level(wakePins[idx]);
    ESP_LOGD(__FUNCTION__, "Pin %d is %d", wakePins[idx], curLvl);
    if (curLvl == 0)
    {
      ESP_ERROR_CHECK(rtc_gpio_pulldown_en(wakePins[idx]));
      ext_wakeup_on_pin_mask |= (1ULL << wakePins[idx]);
    }
  }
  if (ext_wakeup_on_pin_mask != 0)
  {
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(ext_wakeup_on_pin_mask, ESP_EXT1_WAKEUP_ANY_HIGH));
  }
  else
  {
    ESP_ERROR_CHECK(rtc_gpio_pulldown_en(wakePins[0]));
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(wakePins[0], 0));
  }

  //gpio_deep_sleep_hold_en();
  if (gps)
  {
    lastSpeed = gps->speed.value();
    lastCourse = gps->course.value();
    lastAltitude = gps->altitude.value();
    lastLatDeg = gps->location.rawLat().deg;
    lastLatBil = gps->location.rawLat().billionths;
    lastLngDeg = gps->location.rawLng().deg;
    lastLngBil = gps->location.rawLng().billionths;
    lastDpTs = gps->time.value();
    bumpCnt = 0;
    lastRate = 0;
    gpio_set_level(BLINK_GPIO, 1);
    gpio_set_level(gps->enPin(), 0);
    gpio_hold_dis(gps->enPin());
    delete gps;
  }

  //CreateManagedTask(flash, "flashy", 2048, (void *)10, tskIDLE_PRIORITY, NULL);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF));
  ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF));
  ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF));
  ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF));
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  //CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA);

  if (TheRest::GetServer()) {
    delete TheRest::GetServer();
  }

  if (TheWifi::GetInstance()) {
    delete TheWifi::GetInstance();
  }

  ESP_LOGD(__FUNCTION__, "Waiting for sleepers");
  WaitToSleepExceptFor("doHibernate");
  ESP_LOGD(__FUNCTION__, "Hybernating");
  gpio_set_level(BLINK_GPIO, 1);
  esp_deep_sleep_start();
}

void Hibernate()
{
  if (hybernator == NULL){
    EventGroupHandle_t eg = getAppEG();
    EventBits_t bits = xEventGroupGetBits(getAppEG());
    ESP_LOGV(__FUNCTION__, "Hibernating");
    CreateWokeBackgroundTask(doHibernate, "doHibernate", 8192, NULL, tskIDLE_PRIORITY, &hybernator);
    //xEventGroupSetBits(eg,app_bits_t::HIBERNATE);
  }
}

static void gpsEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  if (TinyGPSPlus::runningInstance() && (base == TinyGPSPlus::runningInstance()->GPSPLUS_EVENTS)){
    gps = TinyGPSPlus::runningInstance();
    ESP_LOGV(__FUNCTION__,"gps event:%s %d",base,id);
    now = esp_timer_get_time();
    sampleBatteryVoltage();
    char* ctmp;
    double dtmp;
    switch (id)
    {
    case TinyGPSPlus::gpsEvent::locationChanged:
      ESP_LOGD(__FUNCTION__, "Location: %3.6f, %3.6f, speed: %3.6f, Altitude:%4.2f, Course:%4.2f Bat:%f diff:%.2f", gps->location.lat(), gps->location.lng(), gps->speed.kmph(), gps->altitude.meters(), gps->course.deg(), getBatteryVoltage(), *((double *)event_data));
      if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE){
        ctmp = cJSON_PrintUnformatted(AppConfig::GetAppStatus()->GetJSONConfig("gps"));
        ESP_LOGV(__FUNCTION__,"The j:%s",ctmp);
        ldfree(ctmp);
      }
      lastLocTs = now;
      gpsto = false;
      if (lastSLocTs == 0)
      {
        lastSLocTs = now;
      }
      break;
    case TinyGPSPlus::gpsEvent::systimeChanged:
      char strftime_buf[64];
      struct tm timeinfo;
      time_t cdate;
      time(&cdate);
      localtime_r(&cdate, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
      dumpLogs();
      ESP_LOGI(__FUNCTION__, "System Time: %s diff:%ld", strftime_buf, *(long*)event_data);
      break;
    case TinyGPSPlus::gpsEvent::significantDistanceChange:
      ESP_LOGD(__FUNCTION__, "Distance Diff: %f", *((double *)event_data));
      lastSLocTs = now;
      sgpsto = false;
      break;
    case TinyGPSPlus::gpsEvent::significantSpeedChange:
      ESP_LOGD(__FUNCTION__, "Speed Diff: %f %f", gps->speed.kmph(), *((double *)event_data));
      lastSLocTs = now;
      sgpsto = false;
      break;
    case TinyGPSPlus::gpsEvent::significantCourseChange:
      lastSLocTs = now;
      sgpsto = false;
      ESP_LOGD(__FUNCTION__, "Course Diff: %f %f", gps->course.deg(), *((double *)event_data));
      break;
    case TinyGPSPlus::gpsEvent::rateChanged:
      ESP_LOGD(__FUNCTION__, "Rate Changed to %d", *((uint8_t *)event_data));
      break;
    case TinyGPSPlus::gpsEvent::msg:
      ESP_LOGV(__FUNCTION__, "msg:%s", (char *)event_data);
      boto = ((now-startTs) > timeoutMicro);
      ESP_LOGV(__FUNCTION__,"%d = ((%" PRId64 "-%" PRId64 ") > %" PRId64 ")",boto, now, startTs, timeoutMicro);
      ESP_LOGV(__FUNCTION__,"if ((%" PRId64 " > 0) && (%" PRId64 " - %" PRId64 " > %" PRId64 "))",lastLocTs,now,lastLocTs,timeoutMicro);
      if ((lastLocTs > 0) && (now - lastLocTs > timeoutMicro))
      {
        if (!gpsto)
        {
          gpsto = true;
          ESP_LOGD(__FUNCTION__, "Timeout on GPS Location");
        }
      }
      if ((lastSLocTs > 0) && ((now - lastSLocTs) > timeoutMicro))
      {
        if (!sgpsto)
        {
          sgpsto = true;
          ESP_LOGD(__FUNCTION__, "Timeout on GPS Significant Change");
        }
      }
      if (((lastMovement > 0) && (now - lastMovement > timeoutMicro)) || ((lastMovement == 0) && boto))
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

      if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE){
        char ccctmp[70];
        sprintf(ccctmp, "gps:%d sig change:%d bump:%d signal:%d boto:%d dto:%d", gpsto, sgpsto, buto, sito, boto, dto);
        if (strcmp(ccctmp, cctmp) != 0)
        {
          ESP_LOGV(__FUNCTION__, "States: %s lastSLocTs:%" PRIx64 " now - lastSLocTs:%" PRIx64 " timeout:%d", ccctmp, lastSLocTs, now - lastSLocTs, timeout);
          ESP_LOGV(__FUNCTION__, "Bumps:%d, lastMovement:%" PRIx64 " state:%s", bumpCnt, lastMovement, ccctmp);
          strcpy(cctmp, ccctmp);
        }
      }

      if ((gpsto || sito || sgpsto) && buto)
      {
        if (hybernator != NULL)
        {
          return;
        }
        ESP_LOGD(__FUNCTION__, "Lost GPS Signal and no bumps gps:%d sig:%d", gpsto, sito);
        xEventGroupSetBits(getAppEG(), app_bits_t::HIBERNATE);
      }
      break;
    case TinyGPSPlus::gpsEvent::wakingup:
      sleepTime += (esp_timer_get_time() - tv_sleep);
      ESP_LOGV(__FUNCTION__, "wake at %lu", (time_t)getSleepTime());
      break;
    case TinyGPSPlus::gpsEvent::sleeping:
      ESP_LOGV(__FUNCTION__, "Sleep at %lu", (time_t)getSleepTime());
      WaitToSleep();
      tv_sleep = esp_timer_get_time();
      break;
    case TinyGPSPlus::gpsEvent::go:
      ESP_LOGD(__FUNCTION__, "Go");
      isStopped = false;
      break;
    case TinyGPSPlus::gpsEvent::stop:
      ESP_LOGD(__FUNCTION__, "Stop");
      isStopped = true;
      break;
    case TinyGPSPlus::gpsEvent::gpsPaused:
      ESP_LOGV(__FUNCTION__, "BatteryP %f", getBatteryVoltage());
      break;
    case TinyGPSPlus::gpsEvent::gpsResumed:
      ESP_LOGV(__FUNCTION__, "BatteryR %f", getBatteryVoltage());
      break;
    case TinyGPSPlus::gpsEvent::atSyncPoint:
      ESP_LOGD(__FUNCTION__, "atSyncPoint");
      break;
    case TinyGPSPlus::gpsEvent::outSyncPoint:
      ESP_LOGD(__FUNCTION__, "Leaving Synching");
      break;
    default:
      break;
    }
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
  EventGroupHandle_t app_eg = getAppEG();
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
            lastMovement = esp_timer_get_time();
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
  esp_reset_reason_t reset_reason = esp_reset_reason();

  switch (reset_reason)
  {
  case ESP_RST_UNKNOWN:
    ESP_LOGD(__FUNCTION__, "Reset reason can not be determined");
    break;
  case ESP_RST_POWERON:
    ESP_LOGD(__FUNCTION__, "Reset due to power-on event");
    break;
  case ESP_RST_EXT:
    ESP_LOGD(__FUNCTION__, "Reset by external pin (not applicable for ESP32)");
    break;
  case ESP_RST_SW:
    ESP_LOGD(__FUNCTION__, "Software reset via esp_restart");
    break;
  case ESP_RST_PANIC:
    ESP_LOGD(__FUNCTION__, "Software reset due to exception/panic");
    break;
  case ESP_RST_INT_WDT:
    ESP_LOGD(__FUNCTION__, "Reset (software or hardware) due to interrupt watchdog");
    break;
  case ESP_RST_TASK_WDT:
    ESP_LOGD(__FUNCTION__, "Reset due to task watchdog");
    break;
  case ESP_RST_WDT:
    ESP_LOGD(__FUNCTION__, "Reset due to other watchdogs");
    break;
  case ESP_RST_DEEPSLEEP:
    ESP_LOGD(__FUNCTION__, "Reset after exiting deep sleep mode");
    break;
  case ESP_RST_BROWNOUT:
    ESP_LOGD(__FUNCTION__, "Brownout reset (software or hardware)");
    break;
  case ESP_RST_SDIO:
    ESP_LOGD(__FUNCTION__, "Reset over SDIO");
    break;
  }

  if (reset_reason == ESP_RST_DEEPSLEEP)
  {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    uint64_t wakeup_pin_mask = 0;
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      ESP_LOGD(__FUNCTION__, "In case of deep sleep: reset was not caused by exit from deep sleep");
      break;
    case ESP_SLEEP_WAKEUP_ALL:
      ESP_LOGD(__FUNCTION__, "Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source");
      break;
    case ESP_SLEEP_WAKEUP_EXT0:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by external signal using RTC_CNTL");
      wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
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
    case ESP_SLEEP_WAKEUP_TIMER:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by ULP program");
      break;
    case ESP_SLEEP_WAKEUP_GPIO:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by GPIO (light sleep only)");
      break;
    case ESP_SLEEP_WAKEUP_UART:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by UART (light sleep only)");
      break;
    case ESP_SLEEP_WAKEUP_COCPU:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by ESP_SLEEP_WAKEUP_COCPU");
      break;
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG");
      break;
    case ESP_SLEEP_WAKEUP_BT:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by BT");
      break;
    case ESP_SLEEP_WAKEUP_WIFI:
      ESP_LOGD(__FUNCTION__, "Wakeup caused by Wifi");
      break;
    }
  } 
}
int32_t GenerateDevId()
{
  bootloader_random_enable();
  int32_t devId = esp_random();
  bootloader_random_disable();
  if (devId < 0)
  {
    devId *= -1;
  }
  return devId;
}

void ConfigurePins(AppConfig *cfg)
{
  ESP_LOGV(__FUNCTION__, "Configuring pins");
  cJSON *pin = NULL;
  uint8_t numPins = 0;
  cJSON_ArrayForEach(pin, cfg->GetJSONConfig("pins"))
  {
    AppConfig *cpin = new AppConfig(pin, cfg);
    gpio_num_t pinNo = cpin->GetPinNoProperty("pinNo");
    if (pinNo > 0)
    {
      ESP_LOGV(__FUNCTION__, "Configuring pin %d", pinNo);
      new Pin(cpin);
      numPins++;
    }
    ldfree(cpin);
  }
}

static void check_efuse()
{
  //Check TP is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
  {
    ESP_LOGV(__FUNCTION__, "eFuse Two Point: Supported\n");
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "eFuse Two Point: NOT supported\n");
  }

  //Check Vref is burned into eFuse
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
  {
    ESP_LOGV(__FUNCTION__, "eFuse Vref: Supported\n");
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "eFuse Vref: NOT supported\n");
  }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
  {
    ESP_LOGV(__FUNCTION__, "Characterized using Two Point Value");
  }
  else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
  {
    ESP_LOGV(__FUNCTION__, "Characterized using eFuse Vref");
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Characterized using Default Vref");
  }
}
esp_err_t setupLittlefs();

static void serviceLoop(void *param)
{
  EventBits_t serviceBits;
  EventBits_t waitMask = APP_SERVICE_BITS;
  char *bits = (char *)malloc(app_bits_t::MAX_APP_BITS + 1);
  memset(bits, 0, app_bits_t::MAX_APP_BITS + 1);
  for (int idx = 0; idx < app_bits_t::MAX_APP_BITS; idx++)
  {
    bits[idx] = waitMask & (1 << idx) ? '1' : '0';
  }
  ESP_LOGV(__FUNCTION__, "wait:%s", bits);
  EventGroupHandle_t app_eg = getAppEG();
  AppConfig *appCfg = AppConfig::GetAppConfig();
  ESP_LOGV(__FUNCTION__, "ServiceLoop started");
  TaskHandle_t gpsTask = NULL;
  while (true)
  {
    serviceBits = (APP_SERVICE_BITS & xEventGroupWaitBits(app_eg, waitMask, pdFALSE, pdFALSE, portMAX_DELAY));
    if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE){
      for (int idx = 0; idx < app_bits_t::MAX_APP_BITS; idx++)
      {
        bits[idx] = serviceBits & (1 << idx) ? '1' : '0';
      }
      ESP_LOGV(__FUNCTION__, "curr::%s %d", bits, serviceBits);
    }
    if (serviceBits & app_bits_t::IR && !IRDecoder::IsRunning()) {
      new IRDecoder(appCfg->GetConfig("IR"));
    } 
    if (serviceBits & app_bits_t::WIFI_ON && !TheWifi::GetInstance())
    {
      CreateForegroundTask(wifiSallyForth, "WifiSallyForth", NULL);
    }
    else if ((serviceBits & app_bits_t::WIFI_OFF) && TheWifi::GetInstance())
    {
      ESP_LOGD(__FUNCTION__,"Turning Wifi Off");
      if (TheWifi *theWifi = TheWifi::GetInstance())
      {
        delete theWifi;
      }
    }
    ESP_LOGV(__FUNCTION__,"app_bits_t::REST:%d app_bits_t::WIFI_ON %d",serviceBits & app_bits_t::REST, serviceBits & app_bits_t::WIFI_ON);
    if (((serviceBits & (app_bits_t::REST | app_bits_t::WIFI_ON)) == (app_bits_t::REST | app_bits_t::WIFI_ON)) && !TheRest::GetServer())
    {
      ESP_LOGV(__FUNCTION__,"Turning Rest On");
      CreateForegroundTask(restSallyForth, "restSallyForth", TheWifi::GetEventGroup());
    }
    else if (((serviceBits & (app_bits_t::REST | app_bits_t::WIFI_OFF)) == (app_bits_t::REST | app_bits_t::WIFI_OFF)) && TheRest::GetServer())
    {
      ESP_LOGD(__FUNCTION__,"Turning Rest Off if on %d",TheRest::GetServer()!=NULL);
      if (TheRest *theRest = TheRest::GetServer())
      {
        delete theRest;
      }
    }
    if (serviceBits & (app_bits_t::HIBERNATE)){
      if (gps) {
        delete gps;
        gps = NULL;
      }
      doHibernate(NULL);
    }
    ESP_LOGV(__FUNCTION__,"Checking GPS");
    if (serviceBits & app_bits_t::GPS_ON && !gpsTask)
    {
      if ((TinyGPSPlus::runningInstance() == NULL) && (appCfg->HasProperty("/gps/rxPin") && appCfg->GetIntProperty("/gps/rxPin")))
      {
          ESP_LOGI(__FUNCTION__, "Starting GPS");
          CreateBackgroundTask(gpsSallyForth, "gpsSallyForth", 8196, appCfg, tskIDLE_PRIORITY, &gpsTask);
          ESP_LOGV(__FUNCTION__, "Started GPS");
      } else {
        ESP_LOGV(__FUNCTION__,"Cannot start GPS. Already running:%d has rx pin:%d",TinyGPSPlus::runningInstance()!=NULL,appCfg->HasProperty("/gps/rxPin"));
      }
    }
    
    if ((serviceBits & (app_bits_t::GPS_OFF)) && TinyGPSPlus::runningInstance())
    {
      if (TinyGPSPlus *gps = TinyGPSPlus::runningInstance())
      {
        ESP_LOGI(__FUNCTION__, "Stopping GPS");
        delete gps;
        gpsTask = NULL;
      }
    }
    waitMask = (APP_SERVICE_BITS ^ serviceBits);
    if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE){
      for (int idx = 0; idx < app_bits_t::MAX_APP_BITS; idx++)
      {
        bits[idx] = serviceBits & (1 << idx) ? '1' : '0';
      }
      ESP_LOGV(__FUNCTION__, "curr::%s %d", bits, serviceBits);
      for (int idx = 0; idx < app_bits_t::MAX_APP_BITS; idx++)
      {
        bits[idx] = waitMask & (1 << idx) ? '1' : '0';
      }
      ESP_LOGV(__FUNCTION__, "mask:%s %d", bits, serviceBits);
    }
    if (!waitMask)
    {
      waitMask = APP_SERVICE_BITS;
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGV(__FUNCTION__, "ServiceLoop end loop");
  }
  ESP_LOGW(__FUNCTION__, "ServiceLoop done");
}

void *spimalloc(size_t size)
{
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

bool CleanupEmptyDirs(char* path) {
  ESP_LOGV(__FUNCTION__,"Looking for files to cleanup in %s",path);

  DIR *theFolder;
  FILE *theFile;
  struct dirent *fi;
  struct stat fileStat;
  char* cpath = (char*)dmalloc(300);
  bool retVal = false;
  int retCode = 0;

  if ((theFolder = openDir(path)) != NULL) {
    while ((fi = readDir(theFolder)) != NULL) {
      if (fi->d_type == DT_DIR) {
        sprintf(cpath,"%s/%s",path,fi->d_name);
        if (!CleanupEmptyDirs(cpath)) {
          ESP_LOGD(__FUNCTION__,"%s is empty, deleting",cpath);
          if ((retCode = rmdir(cpath)) != 0) {
            ESP_LOGE(__FUNCTION__, "failed in deleting %s %s(%d)", cpath, getErrorMsg(retCode), retCode);
            retVal=true;
          }
        } else {
          retVal=true;
        }
      } else {
        ESP_LOGV(__FUNCTION__,"%s has %s, not deleting",path,fi->d_name);
        ldfree(cpath);
        closeDir(theFolder);
        return true;
      }
    }
    closeDir(theFolder);
  } else {
    ESP_LOGD(__FUNCTION__,"Cannot open %s",path);
  }

  ldfree(cpath);
  return retVal;
}

void app_main(void)
{
  cJSON_Hooks memoryHook;
  memoryHook.malloc_fn = spimalloc;
  memoryHook.free_fn = free;
  cJSON_InitHooks(&memoryHook);
  startTs = esp_timer_get_time();
  esp_log_level_set("*",ESP_LOG_INFO);

  if (setupLittlefs() == ESP_OK)
  {
    gpio_install_isr_service(0);

    AppConfig *appcfg = new AppConfig(CFG_PATH);
    AppConfig *appstat = AppConfig::GetAppStatus();

    if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
      char* ctmp = cJSON_Print(appcfg->GetJSONConfig(NULL));
      ESP_LOGV(__FUNCTION__,"cfg:%s",ctmp);
      ldfree(ctmp);
    }

    char bo[50];
    const esp_app_desc_t *ad = esp_ota_get_app_description();
    sprintf(bo, "%s %s", ad->date, ad->time);
    ESP_LOGD(__FUNCTION__, "Starting %s v%s %s", ad->project_name, ad->version, bo);
    appstat->SetStringProperty("/build/date", bo);
    appstat->SetStringProperty("/build/ver", ad->version);

    check_efuse();
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);
    //adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);
    uint32_t defvref = 1100;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, defvref, &characteristics);
    print_char_val_type(val_type);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    nvs_flash_init();

    initSDCard();
    CleanupEmptyDirs("/lfs");
    CleanupEmptyDirs("/sdcard");
    deinitSDCard();

    bool firstRun = false;

    if (appcfg->GetIntProperty("deviceid") <= 0)
    {
      firstRun=true;
      appcfg->ResetAppConfig(false);
      uint32_t did;
      appcfg->SetIntProperty("deviceid", (did = GenerateDevId()));
      char* ctmp = cJSON_Print(appcfg->GetJSONConfig(NULL));
      ESP_LOGD(__FUNCTION__, "Seeding device id to %d/%d %s",did,appcfg->GetIntProperty("deviceid"),ctmp);
      ldfree(ctmp);
    }
    if (appcfg->GetIntProperty("deviceid") > 0)
    {
      ESP_LOGD(__FUNCTION__, "Device Id %d", appcfg->GetIntProperty("deviceid"));
    }
    //Register event managers
    new BufferedFile();

    initLog();
    sampleBatteryVoltage();
    //UpgradeFirmware();
    //initSDCard();

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
    }
    else
    {
      ESP_LOGD(__FUNCTION__, "Lat:%d %d Lng:%d %d", lastLatDeg, lastLatBil, lastLngDeg, lastLatBil);
    }
    print_wakeup_reason();

    bool isTracker = appcfg->HasProperty("clienttype") && strcasecmp(appcfg->GetStringProperty("clienttype"),"tracker")==0;
    ConfigurePins(appcfg);
    EventGroupHandle_t app_eg = getAppEG();
    ESP_LOGV(__FUNCTION__,"gps rx pin:%d",appcfg->GetIntProperty("/gps/rxPin"));
    if (appcfg->GetIntProperty("/gps/rxPin")){
      xEventGroupSetBits(app_eg, app_bits_t::GPS_ON);
      ESP_ERROR_CHECK(esp_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, gpsEvent, NULL, NULL));
    }
    if (appcfg->IsAp() || firstRun || !isTracker)
    {
      xEventGroupSetBits(app_eg, app_bits_t::WIFI_ON);
      xEventGroupClearBits(app_eg, app_bits_t::WIFI_OFF);
    }

    if (appcfg->HasProperty("/IR/pinNo")) {
      ESP_LOGD(__FUNCTION__,"Starting IR");
      xEventGroupSetBits(app_eg, app_bits_t::IR);
    } else {
      ESP_LOGV(__FUNCTION__,"No IR");
    }
    
    CreateBackgroundTask(serviceLoop, "ServiceLoop", 8192, NULL, tskIDLE_PRIORITY, NULL);
  }
  if (!heap_caps_check_integrity_all(true))
  {
    ESP_LOGE(__FUNCTION__, "caps integrity error");
  }
  ESP_LOGD(__FUNCTION__, "Battery: %f", getBatteryVoltage());

  //new Bt();
}
