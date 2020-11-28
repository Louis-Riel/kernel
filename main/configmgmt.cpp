#include "utils.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static app_config_t* _app_config = NULL;
static app_state_t app_state;

app_state_t* getAppState() {
  return &app_state;
}

static app_config_t* resetAppConfig() {
  ESP_LOGI(__FUNCTION__,"Resetting to Factory Default settings" );
  memset(_app_config,0,sizeof(app_config_t));
#ifdef IS_TRACKER
  _app_config->purpose = (app_config_t::purpose_t)(app_config_t::purpose_t::TRACKER);
  _app_config->sdcard_config.MisoPin=GPIO_NUM_19;
  _app_config->sdcard_config.MosiPin=GPIO_NUM_23;
  _app_config->sdcard_config.ClkPin=GPIO_NUM_18;
  _app_config->sdcard_config.Cspin=GPIO_NUM_4;
  _app_config->gps_config.enPin=GPIO_NUM_13;
  _app_config->gps_config.rxPin=GPIO_NUM_14;
  _app_config->gps_config.txPin=GPIO_NUM_27;
#endif
#ifndef IS_TRACKER
  _app_config->purpose = (app_config_t::purpose_t)(app_config_t::purpose_t::PULLER);
  _app_config->sdcard_config.MisoPin=GPIO_NUM_2;
  _app_config->sdcard_config.MosiPin=GPIO_NUM_15;
  _app_config->sdcard_config.ClkPin=GPIO_NUM_14;
  _app_config->sdcard_config.Cspin=GPIO_NUM_13;
#define PIN_NUM_MISO (gpio_num_t)2
#define PIN_NUM_MOSI (gpio_num_t)15
#define PIN_NUM_CLK (gpio_num_t)14
#define PIN_NUM_CS (gpio_num_t)13
#endif
  _app_config->pois = (poiConfig_t*)malloc(sizeof(poiConfig_t)*MAX_NUM_POIS);
  memset(_app_config->pois,0,sizeof(poiConfig_t)*MAX_NUM_POIS);
  return _app_config;
}

app_config_t* getAppConfig() {
  return _app_config;
}

app_config_t* initConfig() {
  if (_app_config == NULL) {
    _app_config = (app_config_t*)malloc(sizeof(app_config_t));
    memset(_app_config,0,sizeof(app_config_t));
    nvs_handle my_handle;
    esp_err_t err;

    if ((err=nvs_flash_init()) == ESP_OK){
      if ((err = nvs_open("nvs", NVS_READWRITE, &my_handle)) == ESP_OK) {
        ESP_LOGD(__FUNCTION__,"Opening app config blob");
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        if ((err = nvs_get_blob(my_handle, "mainappconfig", NULL, &required_size)) == ESP_OK) {
          if (required_size == sizeof(app_config_t) &&
              ((err = nvs_get_blob(my_handle, "mainappconfig", _app_config, &required_size)) == ESP_OK)) {
            ESP_LOGI(__FUNCTION__,"Main App Config Initialized as %d",_app_config->purpose);
          } else if (err != ESP_OK) {
              memset(_app_config,0,sizeof(app_config_t));
              ESP_LOGE(__FUNCTION__,"Failed to get blob content %s",esp_err_to_name(err));
          } else {
              memset(_app_config,0,sizeof(app_config_t));
              ESP_LOGE(__FUNCTION__,"App config size mismatch %d!=%d",required_size,sizeof(app_config_t));
          }
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
          if ((err = nvs_set_blob(my_handle,"mainappconfig",(void*)resetAppConfig(),sizeof(app_config_t))) == ESP_OK) {
            nvs_commit(my_handle);
          } else {
            ESP_LOGE(__FUNCTION__,"Failed to save default config %s",esp_err_to_name(err));
          }
        } else {
          ESP_LOGE(__FUNCTION__, "Failed to get blob size, %s",esp_err_to_name(err));
        }
        ESP_LOGD(__FUNCTION__,"Opening POI config blob");
        _app_config->pois = (poiConfig_t*)malloc(sizeof(poiConfig_t)*MAX_NUM_POIS);
        memset(_app_config->pois,0,sizeof(poiConfig_t)*MAX_NUM_POIS);

        if ((err = nvs_get_blob(my_handle, "pois", NULL, &required_size)) == ESP_OK) {
          poiConfig_t* pc = _app_config->pois;
          if (required_size == (sizeof(poiConfig_t)*MAX_NUM_POIS) &&
              ((err = nvs_get_blob(my_handle, "pois", _app_config->pois, &required_size)) == ESP_OK)) {
            ESP_LOGI(__FUNCTION__,"Got POIs %d",required_size);
            uint32_t idx=10;
            while((pc->minDistance < 2000) && (pc->minDistance > 0)) {
              ESP_LOGD(__FUNCTION__,"dist:%d,lat:%3.7f,lng:%3.7f",pc->minDistance,pc->lat,pc->lng);
              idx--;
              pc+=sizeof(poiConfig_t);
            }
          } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            _app_config->pois=(poiConfig_t*)malloc(sizeof(poiConfig_t)*MAX_NUM_POIS);
            memset(_app_config->pois,0,sizeof(poiConfig_t)*MAX_NUM_POIS);
            ESP_LOGD(__FUNCTION__,"No POI list Configured");
          } else if (err != ESP_OK) {
              ESP_LOGE(__FUNCTION__,"Failed to get POIs content %s",esp_err_to_name(err));
          } else {
              ESP_LOGE(__FUNCTION__,"POIs size mismatch %d!=%d",required_size,sizeof(poiConfig_t)*MAX_NUM_POIS);
              memset(pc,0,sizeof(poiConfig_t)*MAX_NUM_POIS);
              saveConfig();
              esp_restart();
          }
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            _app_config->pois=(poiConfig_t*)malloc(sizeof(poiConfig_t)*MAX_NUM_POIS);
            memset(_app_config->pois,0,sizeof(poiConfig_t)*MAX_NUM_POIS);
            ESP_LOGD(__FUNCTION__,"No POIs Configured");
        } else {
          ESP_LOGE(__FUNCTION__, "Failed to get POIs blob size, %s",esp_err_to_name(err));
        }

        nvs_close(my_handle);
        if (_app_config->purpose == app_config_t::purpose_t::UNKNOWN) {
          resetAppConfig();
          saveConfig();
        }
      } else {
        ESP_LOGE(__FUNCTION__, "Failed to open nvs %s",esp_err_to_name(err));
      }
    } else {
      ESP_LOGE(__FUNCTION__,"Cannot init the NVS %s",esp_err_to_name(err));
    }
  }
  if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG){
    ESP_LOGD(__FUNCTION__,"sdcard:{MisoPin:%d,MosiPin:%d,ClkPin:%d,Cspin;%d}",
        _app_config->sdcard_config.MisoPin,
        _app_config->sdcard_config.MosiPin,
        _app_config->sdcard_config.ClkPin,
        _app_config->sdcard_config.ClkPin);
    ESP_LOGD(__FUNCTION__,"gps:{rx:%d,tx:%d,en;%d}",
        _app_config->gps_config.rxPin,
        _app_config->gps_config.txPin,
        _app_config->gps_config.enPin);
    ESP_LOGD(__FUNCTION__,"Tracker:%s Puller:%s",
        _app_config->purpose&app_config_t::purpose_t::TRACKER?"Yes":"No",
        _app_config->purpose&app_config_t::purpose_t::PULLER?"Yes":"No");
  }

  //gpio_num_t wakePins[10];
  //poiConfig_t* pois;
  return _app_config;
}

void saveConfig() {
  nvs_handle my_handle;
  esp_err_t err;
  if (_app_config != NULL) {
    if ((err = nvs_open("nvs", NVS_READWRITE, &my_handle)) == ESP_OK) {
      // Read the size of memory space required for blob
      ESP_LOGD(__FUNCTION__,"Opening app config blob");
      size_t required_size = 0;  // value will default to 0, if not set yet in NVS
      if ((err = nvs_get_blob(my_handle, "mainappconfig", NULL, &required_size)) == ESP_OK) {
        if (required_size == sizeof(app_config_t) &&
            ((err = nvs_set_blob(my_handle, "mainappconfig", _app_config, required_size)) == ESP_OK)) {
          nvs_commit(my_handle);
          ESP_LOGI(__FUNCTION__,"Main App Config Updated");
        } else if (err != ESP_OK) {
            ESP_LOGE(__FUNCTION__,"Failed to update blob content %s",esp_err_to_name(err));
        } else {
            ESP_LOGE(__FUNCTION__,"App config size mismatch %d!=%d",required_size,sizeof(app_config_t));
        }
        required_size =  sizeof(poiConfig_t) * MAX_NUM_POIS;
        if ((err = nvs_set_blob(my_handle, "pois", _app_config->pois, required_size)) == ESP_OK) {
          nvs_commit(my_handle);
          ESP_LOGI(__FUNCTION__,"POI data Updated");
        } else if (err != ESP_OK) {
            ESP_LOGE(__FUNCTION__,"Failed to update POI content %s",esp_err_to_name(err));
        } else {
            ESP_LOGE(__FUNCTION__,"POI config size mismatch %d!=%d",required_size,sizeof(app_config_t));
        }
      } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        if ((err = nvs_set_blob(my_handle,"mainappconfig",(void*)_app_config,sizeof(app_config_t))) == ESP_OK) {
          nvs_commit(my_handle);
        } else {
          ESP_LOGE(__FUNCTION__,"Failed to save default config %s",esp_err_to_name(err));
        }
      } else {
        ESP_LOGE(__FUNCTION__, "Failed to get blob size, %s",esp_err_to_name(err));
      }
      nvs_close(my_handle);
    } else {
      ESP_LOGE(__FUNCTION__, "Failed to open nvs %s",esp_err_to_name(err));
    }
  }
}