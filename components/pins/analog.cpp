#include "pins.h"
#include "esp_sleep.h"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
#define DEFAULT_VREF  1100

const char* AnalogPin::PIN_BASE="AnalogPin";

AnalogPin::AnalogPin(AppConfig* config)
    :ManagedDevice(PIN_BASE,config->GetStringProperty("name")),
    config(config),
    pinNo(config->GetPinNoProperty("pinNo")),
    channel(PinNoToChannel(config->GetPinNoProperty("pinNo"))),
    channel_width(GetChannelWidth(config->GetIntProperty("channel_width"))),
    channel_atten(GetChannelAtten(config->GetDoubleProperty("channel_atten"))),
    waitTime(config->GetIntProperty("waitTime")),
    configuredMinValue(config->GetPropertyHolder("minValue")),
    configuredMaxValue(config->GetPropertyHolder("maxValue")),
    isRunning(false)
{
    this->InitDevice();
}

cJSON* AnalogPin::BuildConfigTemplate() {
    cJSON* commandTemplate = ManagedDevice::BuildConfigTemplate();
    cJSON_SetValuestring(cJSON_GetObjectItem(commandTemplate,"class"),"AnalogPin");
    cJSON_AddTrueToObject(commandTemplate, "isArray");
    cJSON_AddStringToObject(commandTemplate,"collectionName","analogPins");
    cJSON_AddStringToObject(commandTemplate,"name","New Analog Pin");
    cJSON_AddNumberToObject(commandTemplate,"pinNo",1);
    cJSON_AddNumberToObject(commandTemplate,"channel_width",1);
    cJSON_AddNumberToObject(commandTemplate,"channel_atten",1.1);
    cJSON_AddNumberToObject(commandTemplate,"waitTime",0);
    cJSON_AddNumberToObject(commandTemplate,"minValue",0);
    cJSON_AddNumberToObject(commandTemplate,"minValue",0);
    return commandTemplate;
}


adc1_channel_t AnalogPin::PinNoToChannel(gpio_num_t pinNo) {
    switch(pinNo) {
        case GPIO_NUM_32:
            return ADC1_CHANNEL_4;
        case GPIO_NUM_33:
            return ADC1_CHANNEL_5;
        case GPIO_NUM_34:
            return ADC1_CHANNEL_6;
        case GPIO_NUM_35:
            return ADC1_CHANNEL_7;
        case GPIO_NUM_36:
            return ADC1_CHANNEL_0;
        case GPIO_NUM_37:
            return ADC1_CHANNEL_1;
        case GPIO_NUM_38:
            return ADC1_CHANNEL_2;
        case GPIO_NUM_39:
            return ADC1_CHANNEL_3;
        default:
            return ADC1_CHANNEL_MAX;
    }
}

adc_bits_width_t AnalogPin::GetChannelWidth(uint8_t value) {
    switch (value) {
        case 9:
            return ADC_WIDTH_BIT_9;
        case 10:
            return ADC_WIDTH_BIT_10;
        case 11:
            return ADC_WIDTH_BIT_11;
        case 12:
            return ADC_WIDTH_BIT_12;
        default:
            return ADC_WIDTH_MAX;
    }
}

adc_atten_t AnalogPin::GetChannelAtten(double value) {
    if ( value == 0.0)                     // Measurable input vol__PRETTY_FUNCTION__e range
            return ADC_ATTEN_DB_0;  // 100 mV ~ 950 mV
    if ( value == 2.5)
            return ADC_ATTEN_DB_2_5; // 100 mV ~ 1250 mV
    if ( value == 6.0)
            return ADC_ATTEN_DB_6;   // 150 mV ~ 1750 mV    
    if ( value == 11.0)
            return ADC_ATTEN_DB_11;  // 150 mV ~ 2450 mV
    return ADC_ATTEN_MAX;
}

void AnalogPin::InitDevice(){
    if (channel == ADC1_CHANNEL_MAX) {
        ESP_LOGE(PIN_BASE, "Invalid channel");
        return;
    }
    if (channel_width == ADC_WIDTH_MAX) {
        ESP_LOGE(PIN_BASE, "Invalid channel width");
        return;
    }
    if (channel_atten == ADC_ATTEN_MAX) {
        ESP_LOGE(PIN_BASE, "Invalid channel atten");
        return;
    }
    ESP_LOGI(__PRETTY_FUNCTION__,"Initializing analog %s pin %d status null:%d invalid: %d",this->name,this->pinNo, status == NULL, status == NULL ? -1 : cJSON_IsInvalid(status));
    ESP_ERROR_CHECK(adc1_config_width(this->channel_width));   
    ESP_ERROR_CHECK(adc1_config_channel_atten(this->channel, this->channel_atten));
    
    switch (esp_adc_cal_characterize(ADC_UNIT_1,this->channel_atten ,this->channel_width,DEFAULT_VREF,&chars))
    {
    case ESP_ADC_CAL_VAL_EFUSE_VREF:
        ESP_LOGI(__PRETTY_FUNCTION__,"VRef Calibration used for pin %s:%d",GetName(),pinNo);
        break;
    case ESP_ADC_CAL_VAL_EFUSE_TP:
        ESP_LOGI(__PRETTY_FUNCTION__,"Two point calibration used for pin %s:%d",GetName(),pinNo);
        break;
    case ESP_ADC_CAL_VAL_EFUSE_TP_FIT:
        ESP_LOGI(__PRETTY_FUNCTION__,"Two point fit calibration used for pin %s:%d",GetName(),pinNo);
        break;
    case ESP_ADC_CAL_VAL_NOT_SUPPORTED:
        ESP_LOGI(__PRETTY_FUNCTION__,"Calibration not supported for pin %s:%d",GetName(),pinNo);
        break;
    default:
        ESP_LOGW(__PRETTY_FUNCTION__,"Weird calibration, not supported for pin %s:%d",GetName(),pinNo);
        break;
    };

    AppConfig* appstate = new AppConfig(status,AppConfig::GetAppStatus());

    appstate->SetStringProperty("name",name);
    appstate->SetIntProperty("pinNo",pinNo);
    appstate->SetIntProperty("value",0);
    appstate->SetIntProperty("minValue",4096);
    appstate->SetIntProperty("maxValue",0);
    appstate->SetIntProperty("voltage",0);
    voltage = appstate->GetPropertyHolder("voltage");
    value = appstate->GetPropertyHolder("value");
    currentMinValue = appstate->GetPropertyHolder("minValue");
    currentMaxValue = appstate->GetPropertyHolder("maxValue");
    if (configuredMaxValue && configuredMinValue) {
        ESP_LOGI(__PRETTY_FUNCTION__,"Pin %d has range of %f/%f",this->pinNo, cJSON_GetNumberValue(configuredMinValue), cJSON_GetNumberValue(configuredMaxValue));
        appstate->SetDoubleProperty("percentage",0.0);
        currentPercentage = appstate->GetPropertyHolder("percentage");
    } else {
        ESP_LOGI(__PRETTY_FUNCTION__,"Pin %d has no range %s/%s",this->pinNo, configuredMinValue == NULL ? "" : "min", configuredMaxValue == NULL ? "" : "max");
        currentPercentage = NULL;
    }

    ESP_LOGI(__PRETTY_FUNCTION__,"Analog pin %d initialised",this->pinNo);

    CreateRepeatingTask(PollPin,this->GetName(),this,this->waitTime);
    delete appstate;
}

void AnalogPin::PollPin(void* instance) {
    AnalogPin* pin = (AnalogPin*)instance;
    pin->isRunning = true;
    ESP_LOGD(__PRETTY_FUNCTION__,"Polling analog pin %d",pin->pinNo);
    pin->value->valueint = 0;
    for (int idx = 0; idx < 10; idx++) {
        pin->value->valueint += adc1_get_raw(pin->channel);
        //vTaskDelay(1);
    }
    cJSON_SetIntValue(pin->value, pin->value->valueint / 10);
    cJSON_SetIntValue(pin->currentMinValue,pin->currentMinValue->valueint < pin->value->valueint ? pin->currentMinValue->valueint : pin->value->valueint);
    cJSON_SetIntValue(pin->currentMaxValue,pin->currentMaxValue->valueint > pin->value->valueint ? pin->currentMaxValue->valueint : pin->value->valueint);
    if (pin->configuredMinValue && pin->configuredMaxValue && pin->currentPercentage) {
        cJSON_SetNumberValue(pin->currentPercentage,
            ((cJSON_GetNumberValue(pin->value) - cJSON_GetNumberValue(pin->configuredMinValue)) / 
                (cJSON_GetNumberValue(pin->configuredMaxValue) - cJSON_GetNumberValue(pin->configuredMinValue))) * 100.0
        );
    }
    cJSON_SetIntValue(pin->voltage, ((float)esp_adc_cal_raw_to_voltage(pin->value->valueint, &pin->chars) * (LOWER_DIVIDER+UPPER_DIVIDER) / LOWER_DIVIDER)/1000.0);
}
