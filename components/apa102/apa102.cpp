#include "apa102.h"
#include "esp_sleep.h"
#include "eventmgr.h"
#include "math.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

const char* Apa102::APA102_BASE="Apa102";

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

Apa102::Apa102(AppConfig* config)
    :ManagedDevice(APA102_BASE,config->GetStringProperty("name"),NULL,&ProcessCommand),
    config(config),
    pwrPin(config->GetPinNoProperty("pwrPin")),
    dataPin(config->GetPinNoProperty("dataPin")),
    clkPin(config->GetPinNoProperty("clkPin")),
    numLeds(config->GetIntProperty("numLeds")),
    refreshFreq(config->GetIntProperty("refreshFreq")),
    cpol(config->GetBoolProperty("cpol")),
    cpha(config->GetBoolProperty("cpha")),
    spiBufferLen(sizeof(uint8_t) * (4+(4*numLeds)+4)),
    eg(xEventGroupCreate())
{
    if (numLeds && spiBufferLen && clkPin && dataPin && pwrPin){
        xEventGroupClearBits(eg, 0xff);
        xEventGroupSetBits(eg,apa102_state_t::idle);
        auto* appstate = new AppConfig(status,AppConfig::GetAppStatus());
        memset(&spiTransObject, 0, sizeof(spiTransObject));
        memset(&devcfg, 0, sizeof(devcfg));
        memset(&buscfg, 0, sizeof(buscfg));
        spiTransObject.tx_buffer = (uint8_t*)dmalloc(spiBufferLen);
        spiTransObject.length = (spiBufferLen)*8;
        memset((void*)spiTransObject.tx_buffer,0,spiBufferLen);
        memset((void*)spiTransObject.tx_buffer+(4+(numLeds*4)),1,4);

        cJSON* methods = cJSON_AddArrayToObject(appstate->GetJSONConfig(NULL),"commands");
        cJSON* flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"className",APA102_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","ON");
        cJSON_AddStringToObject(flush,"caption","On");

        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"className",APA102_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","OFF");
        cJSON_AddStringToObject(flush,"caption","Off");

        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"className",APA102_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","TRIGGER");
        cJSON_AddStringToObject(flush,"caption","Trigger");


        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"caption","Set brightness(0-31)");
        cJSON_AddStringToObject(flush,"command","brightness");
        cJSON_AddStringToObject(flush,"className",APA102_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1_label","ledno");
        cJSON_AddNumberToObject(flush,"param1",0);
        cJSON_AddBoolToObject(flush,"param1_editable",true);
        cJSON_AddStringToObject(flush,"param2_label","brightness");
        cJSON_AddNumberToObject(flush,"param2",31);
        cJSON_AddBoolToObject(flush,"param2_editable",true);

        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"caption","Set color");
        cJSON_AddStringToObject(flush,"command","color");
        cJSON_AddStringToObject(flush,"className",APA102_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1_label","ledno");
        cJSON_AddNumberToObject(flush,"param1",0);
        cJSON_AddBoolToObject(flush,"param1_editable",true);
        cJSON_AddStringToObject(flush,"param2_label","Red");
        cJSON_AddNumberToObject(flush,"param2",255);
        cJSON_AddBoolToObject(flush,"param2_editable",true);
        cJSON_AddStringToObject(flush,"param3_label","Green");
        cJSON_AddNumberToObject(flush,"param3",255);
        cJSON_AddBoolToObject(flush,"param3_editable",true);
        cJSON_AddStringToObject(flush,"param4_label","Blue");
        cJSON_AddNumberToObject(flush,"param4",255);
        cJSON_AddBoolToObject(flush,"param4_editable",true);


        appstate->SetIntProperty("powered",0);    
        appstate->SetIntProperty("DevReady",0);
        appstate->SetIntProperty("Painting",0);
        appstate->SetIntProperty("Idle",1);
        jPower = appstate->GetPropertyHolder("powered");
        jSpiReady = appstate->GetPropertyHolder("SpiReady");
        jDevReady = appstate->GetPropertyHolder("DevReady");
        jPainting = appstate->GetPropertyHolder("Painting");
        jIdle = appstate->GetPropertyHolder("Idle");

        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));

        delete appstate;
    } else {
        ESP_LOGW(__FUNCTION__,"Invalid config. numLeds:%d spiBufferLen:%d clkPin:%d dataPin:%d pwrPin:%d",numLeds, spiBufferLen, clkPin, dataPin, pwrPin);
    }
}

cJSON* Apa102::BuildConfigTemplate() {
    cJSON* commandTemplate = ManagedDevice::BuildConfigTemplate();
    cJSON_SetValuestring(cJSON_GetObjectItem(commandTemplate,"class"),"Apa102");
    cJSON_AddTrueToObject(commandTemplate, "isArray");
    cJSON_AddStringToObject(commandTemplate,"collectionName","dotstars");
    cJSON_AddStringToObject(commandTemplate,"name","New DotStar");
    cJSON_AddNumberToObject(commandTemplate,"pwrPin",1);
    cJSON_AddNumberToObject(commandTemplate,"dataPin",1);
    cJSON_AddNumberToObject(commandTemplate,"clkPin",1);
    cJSON_AddNumberToObject(commandTemplate,"numLeds",1);
    cJSON_AddNumberToObject(commandTemplate,"refreshFreq",1);
    cJSON_AddBoolToObject(commandTemplate,"cpol",0);
    cJSON_AddBoolToObject(commandTemplate,"cpha",0);
    return commandTemplate;
}

EventHandlerDescriptor* Apa102::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"Pin(%d):%s BuildHandlerDescriptors",pwrPin,name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(1,"trigger",event_data_type_tp::JSON);
  handler->AddEventDescriptor(2,"brightness",event_data_type_tp::JSON);
  handler->AddEventDescriptor(3,"color",event_data_type_tp::JSON);
  return handler;
}

void Apa102::InitDevice(){
    EventBits_t state = xEventGroupGetBits(eg);

    if (jPower->valueint) {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = (1ULL << dataPin) + (1ULL << clkPin)+ (1ULL << pwrPin);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        gpio_set_level(pwrPin, 0);
        gpio_set_level(clkPin, cpol);
        gpio_set_level(dataPin, cpha);
        state = xEventGroupSetBits(eg,apa102_state_t::powered_on);

        if (!(state & apa102_state_t::spi_ready)){
            buscfg.miso_io_num=-1;
            buscfg.mosi_io_num=dataPin;
            buscfg.sclk_io_num=clkPin;
            buscfg.quadwp_io_num=-1;
            buscfg.quadhd_io_num=-1;
            buscfg.max_transfer_sz=SPI_MAX_DMA_LEN;
            
            devcfg.clock_speed_hz=refreshFreq*1000*1000;
            devcfg.mode=(cpol?1:0)+(cpha?2:0);
            devcfg.spics_io_num=-1;             
            devcfg.queue_size=10;

            ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));	
            ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
            state=xEventGroupSetBits(eg,apa102_state_t::device_ready | apa102_state_t::spi_ready);
            CreateBackgroundTask(PaintIt,GetName(),4096,this,tskIDLE_PRIORITY,NULL);

            ESP_LOGI(__FUNCTION__,"Initialized Apa102 led. pwr pin:%d, clk pin:%d, dataPin:%d, mode: %d refreshFreq:%d",pwrPin, clkPin, dataPin, devcfg.mode, devcfg.clock_speed_hz);
        } else {
            ESP_LOGW(__FUNCTION__,"Weridness, spi was already initialized");
        }
    } else {
        ESP_LOGI(__FUNCTION__,"Deiniting Apa102 led at pin %d",this->pwrPin);
        //spi_device_release_bus(spi);
        xEventGroupSetBits(eg, apa102_state_t::brightness);
        state = xEventGroupWaitBits(eg,apa102_state_t::idle, pdFALSE,pdTRUE,portMAX_DELAY);

        if (state & apa102_state_t::device_ready)
            ESP_ERROR_CHECK(spi_bus_remove_device(spi));

        if (state & apa102_state_t::spi_ready)
            ESP_ERROR_CHECK(spi_bus_free(VSPI_HOST));	

        if (state & apa102_state_t::powered_on)
            gpio_set_level(pwrPin, 1);

        xEventGroupClearBits(eg,apa102_state_t::device_ready|apa102_state_t::powered_on|apa102_state_t::spi_ready);
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = (1ULL << dataPin) + (1ULL << clkPin) + (1ULL << pwrPin);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_LOGI(__FUNCTION__,"Done Deiniting Apa102 led at pin %d",this->pwrPin);
    }
    state = xEventGroupSetBits(eg,apa102_state_t::powered_on);
    cJSON_SetIntValue(jSpiReady, state & apa102_state_t::spi_ready ? 1 : 0);
    cJSON_SetIntValue(jDevReady, state & apa102_state_t::device_ready ? 1 : 0);
    cJSON_SetIntValue(jIdle, state & apa102_state_t::idle ? 1 : 0);
    AppConfig::SignalStateChange(state_change_t::MAIN);
}

void Apa102::PaintIt(void* instance)
{
    if (!instance) {
        ESP_LOGE(__FUNCTION__,"Missing instance to run on");
        return;
    }
    Apa102* leds = (Apa102*) instance;
    spi_transaction_t* txn;
    txn = (spi_transaction_t*)dmalloc(sizeof(*txn));
    esp_err_t ret = ESP_OK;
    ESP_LOGI(__FUNCTION__,"Painting %s", leds->GetName());
    xEventGroupSetBits(leds->eg,apa102_state_t::painting);
    xEventGroupClearBits(leds->eg,apa102_state_t::idle);
    cJSON_SetIntValue(leds->jIdle,0);
    cJSON_SetIntValue(leds->jPainting,1);
    while (leds->jPower->valueint){
        memcpy(txn,&leds->spiTransObject,sizeof(spi_transaction_t));

        xEventGroupClearBits(leds->eg,apa102_state_t::sent);
        xEventGroupSetBits(leds->eg,apa102_state_t::sending);
        ESP_ERROR_CHECK(spi_device_queue_trans(leds->spi, txn, portMAX_DELAY));
        ret = spi_device_get_trans_result(leds->spi, &txn, portMAX_DELAY);
        xEventGroupSetBits(leds->eg,apa102_state_t::sent);
        xEventGroupClearBits(leds->eg,apa102_state_t::sending);

        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(__FUNCTION__,"%s timeout",leds->GetName());
        } else if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGE(__FUNCTION__,"%s invalid args",leds->GetName());
        }
        xEventGroupWaitBits(leds->eg,apa102_state_t::brightness|apa102_state_t::color,pdTRUE,pdFALSE,portMAX_DELAY);
    }
    ESP_LOGI(__FUNCTION__,"Done painting %s", leds->GetName());
    xEventGroupClearBits(leds->eg,apa102_state_t::painting);
    xEventGroupSetBits(leds->eg,apa102_state_t::idle);
    cJSON_SetIntValue(leds->jIdle,1);
    cJSON_SetIntValue(leds->jPainting,0);
    ldfree(txn);
    AppConfig::SignalStateChange(state_change_t::MAIN);
}

bool Apa102::ProcessCommand(ManagedDevice* dev, cJSON * parms) {
    Apa102* leds = (Apa102*)dev;
    if (cJSON_HasObjectItem(parms,"command") && 
        cJSON_HasObjectItem(parms,"name") && 
        cJSON_HasObjectItem(parms,"param1") && 
        (strcmp(leds->GetName(), cJSON_GetObjectItem(parms,"name")->valuestring) == 0)) {
        char* command = cJSON_GetObjectItem(parms,"command")->valuestring;
        if (strcmp("trigger", command) == 0) {
            cJSON* param = cJSON_GetObjectItem(parms, "param1");
            if (strcmp(param->valuestring,"TRIGGER") == 0) {
                cJSON_SetIntValue(leds->jPower,!leds->jPower->valueint);
            } else if (strcmp(param->valuestring,"ON") == 0) {
                cJSON_SetIntValue(leds->jPower,1);
            } else if (strcmp(param->valuestring,"OFF") == 0) {
                cJSON_SetIntValue(leds->jPower,0);
            } else {
                ESP_LOGW(__FUNCTION__,"Invalid param1 value");
                return false;
            }
            leds->InitDevice();
        } else if (strcmp("brightness", command) == 0) {
            uint32_t ledNo = cJSON_GetObjectItem(parms, "param1")->valueint;
            if (ledNo > leds->numLeds) {
                ESP_LOGW(__FUNCTION__,"Invalid led no %d",ledNo);
                return false;
            }
            ((uint8_t*)leds->spiTransObject.tx_buffer)[4+(ledNo*4)] = 0xe0+min(cJSON_GetObjectItem(parms, "param2")->valueint,31);
            xEventGroupSetBits(leds->eg,apa102_state_t::brightness);
            ESP_LOGI(__FUNCTION__,"%d Brightness:%d",ledNo, ((uint8_t*)leds->spiTransObject.tx_buffer)[4+(ledNo*4)]);
        } else if (strcmp("color", command) == 0) {
            uint32_t ledNo = cJSON_GetObjectItem(parms, "param1")->valueint;
            uint32_t red = cJSON_GetObjectItem(parms, "param2")->valueint;
            uint32_t green = cJSON_GetObjectItem(parms, "param3")->valueint;
            uint32_t blue = cJSON_GetObjectItem(parms, "param4")->valueint;
            if (ledNo > leds->numLeds) {
                ESP_LOGW(__FUNCTION__,"Invalid led no %d",ledNo);
                return false;
            }

            ((uint8_t*)leds->spiTransObject.tx_buffer)[4+(ledNo*4)+1] = red;
            ((uint8_t*)leds->spiTransObject.tx_buffer)[4+(ledNo*4)+2] = green;
            ((uint8_t*)leds->spiTransObject.tx_buffer)[4+(ledNo*4)+3] = blue;
            xEventGroupSetBits(leds->eg,apa102_state_t::color);
            ESP_LOGV(__FUNCTION__,"%d red:%d",ledNo,red);
            ESP_LOGV(__FUNCTION__,"%d green:%d",ledNo,green);
            ESP_LOGV(__FUNCTION__,"%d blue:%d",ledNo,blue);
        } else {
            ESP_LOGV(__FUNCTION__,"Invalid Command %s", command);
            return false;
        }
        AppConfig::SignalStateChange(state_change_t::MAIN);
        return true;
    }
    return false;
}
