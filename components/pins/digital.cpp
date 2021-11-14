#include "pins.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static Pin** pins=NULL;
uint8_t Pin::numPins=0;
QueueHandle_t Pin::eventQueue;

Pin::~Pin(){
    ldfree((void*)name);
    for (int idx=0; idx < MAX_NUM_PINS; idx++){
        if (pins[idx] == this) {
            pins[idx]=NULL;
            numPins--;
        }
    }
    if (numPins == 0) {
        EventManager::UnRegisterEventHandler(handlerDescriptors);
    }
    ESP_LOGD(__FUNCTION__,"Destructor");
}

Pin::Pin(AppConfig* config)
    :ManagedDevice("DigitalPin","DigitalPin",BuildStatus),
    pinNo(config->GetPinNoProperty("pinNo")),
    flags(config->GetIntProperty("driverFlags")),
    config(config),
    pinStatus(NULL),
    buf((char*)dmalloc(1024))
{
    const char* pname = config->GetStringProperty("pinName");
    ldfree(name);
    uint32_t sz = strlen(pname)+1;
    name = (char*)dmalloc(200);
    if (sz > 0){
        strcpy(name,pname);
    } else {
        sprintf(name,"DigitalPin pin(%d)",pinNo);
    }
    if (pins == NULL){
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
        pins = (Pin**)dmalloc(sizeof(void*)*MAX_NUM_PINS);
        memset(pins,0,sizeof(void*)*MAX_NUM_PINS);
        ESP_LOGV(__FUNCTION__,"Registering handler");
        PollPins();
    }
    ESP_LOGV(__FUNCTION__,"Pin(%d):%s at idx:%d",pinNo,name,numPins);
    pins[numPins++]=this;
    InitDevice();
    cJSON* jcfg;
    AppConfig* apin = new AppConfig((jcfg=BuildStatus(this)),AppConfig::GetAppStatus());
    apin->SetPinNoProperty("pinNo",pinNo);
    apin->SetIntProperty("state",-1);
    pinStatus = apin->GetPropertyHolder("state");
    if (flags&gpio_driver_t::driver_type_t::digital_out){
        cJSON* methods = cJSON_AddArrayToObject(jcfg,"commands");
        cJSON* flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","ON");
        cJSON_AddStringToObject(flush,"caption","On");
        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","OFF");
        cJSON_AddStringToObject(flush,"caption","Off");
        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","TRIGGER");
        cJSON_AddStringToObject(flush,"caption","Trigger");
    }

    delete apin;
}

EventHandlerDescriptor* Pin::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"Pin(%d):%s BuildHandlerDescriptors",pinNo,name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(eventIds::OFF,"OFF",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::ON,"ON",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::TRIGGER,"TRIGGER");
  handler->AddEventDescriptor(eventIds::STATUS,"STATUS",event_data_type_tp::JSON);
  return handler;
}

void Pin::InitDevice(){
    ESP_LOGD(__FUNCTION__,"Initializing pin %d as %s(%d)",pinNo,name,numPins);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = 1ULL << pinNo;
    io_conf.mode = flags&gpio_driver_t::driver_type_t::digital_out ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    io_conf.pull_up_en = flags&gpio_driver_t::driver_type_t::pullup?GPIO_PULLUP_ENABLE:GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = flags&gpio_driver_t::driver_type_t::pullup?GPIO_PULLDOWN_ENABLE:GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGV(__FUNCTION__,"Pin(%d):%s Direction:%s",pinNo,name,flags&gpio_driver_t::driver_type_t::digital_out ? "Out":"In");
    if (flags&gpio_driver_t::driver_type_t::pullup) {
        ESP_LOGV(__FUNCTION__,"Pin(%d):%s pullup enabled",pinNo,name);
    }
    if (flags&gpio_driver_t::driver_type_t::pulldown) {
        ESP_LOGV(__FUNCTION__,"Pin(%d):%s pulldown enabled",pinNo,name);
    }
    isRtcGpio = rtc_gpio_is_valid_gpio(pinNo);
    ESP_LOGD(__FUNCTION__, "Pin %d: RTC:%d", pinNo, isRtcGpio);
    ESP_LOGD(__FUNCTION__, "Pin %d: Level:%d", pinNo, gpio_get_level(pinNo));
    ESP_LOGV(__FUNCTION__, "Pin %d: 0x%" PRIXPTR "", pinNo,(uintptr_t)this);
    ESP_LOGV(__FUNCTION__, "Numpins %d", numPins);
    if (isRtcGpio)
        ESP_ERROR_CHECK(gpio_isr_handler_add(pinNo, pinHandler, this));
}

void Pin::RefrestState(){
    int curState = gpio_get_level(pinNo);
    if (curState != pinStatus->valueint) {
        cJSON_SetIntValue(pinStatus,curState);
        ESP_LOGV(__FUNCTION__,"Pin(%d)%s RefreshState:%s",pinNo, eventBase,pinStatus->valueint?"On":"Off");
        //AppConfig::SignalStateChange(state_change_t::EVENT);
        if (cJSON_PrintPreallocated(ManagedDevice::BuildStatus(this),this->buf,1024,false)){
            //AppConfig::SignalStateChange(state_change_t::MAIN);
            esp_event_post(eventBase,pinStatus->valueint ? eventIds::ON : eventIds::OFF,buf,strlen(buf),portMAX_DELAY);
        } else {
            ESP_LOGW(__FUNCTION__,"Could not parse status");
        }
    }
}

void Pin::queuePoller(void *arg){
    Pin* pin = NULL;

    while(xQueueReceive(eventQueue,&pin,portMAX_DELAY)){
        pin->RefrestState();
    }
    ESP_LOGE(__FUNCTION__,"%s","Failed");
}

void Pin::pinHandler(void *arg)
{
  xQueueSendFromISR(eventQueue, &arg, NULL);
}

void Pin::PollPins(){
  ESP_LOGV(__FUNCTION__, "Configuring Pins.");
  eventQueue = xQueueCreate(10, sizeof(uint32_t));
  CreateBackgroundTask(queuePoller, "pinsPoller", 4096, NULL, tskIDLE_PRIORITY, NULL);
  ESP_LOGV(__FUNCTION__, "ISR Service Started");
}

void Pin::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    Pin* pin = NULL;
    ESP_LOGV(__FUNCTION__,"Event %s-%d - %" PRIXPTR, base,id,(uintptr_t)event_data);
    cJSON* params=*(cJSON**)event_data;
    if (cJSON_IsInvalid(params)) {
        ESP_LOGE(__FUNCTION__,"Invalid params for digital pin");
    } else {
        ESP_LOGV(__FUNCTION__,"Params(0x%" PRIXPTR " 0x%" PRIXPTR ")",(uintptr_t)event_data,(uintptr_t)params);
        if (params != NULL) {
            uint32_t pinNo = (((uintptr_t)params) < 35 || !cJSON_HasObjectItem(params,"pinNo"))?(uintptr_t)params:cJSON_GetObjectItem(cJSON_GetObjectItem(params,"pinNo"),"value")->valueint;
            const char* pinName = (((uintptr_t)params) < 35 || !cJSON_HasObjectItem(params,"name"))?NULL:cJSON_GetObjectItem(cJSON_GetObjectItem(params,"name"),"value")->valuestring;
            for (uint32_t idx = 0; idx < numPins; idx++) {
                if (pins[idx]){
                    ESP_LOGV(__FUNCTION__,"%d:Checking %s==%s %d==%d",idx, pins[idx]->name,pinName==NULL?"NULL":pinName,pins[idx]->pinNo,pinNo);
                    if ((pinNo && (pins[idx]->pinNo == pinNo)) || 
                        (pinName && (strcmp(pinName,pins[idx]->GetName())==0))) {
                        pin = pins[idx];
                        if (pin && pin->flags&gpio_driver_t::driver_type_t::digital_in) {
                            return;
                        }

                        break;
                    }
                } else {
                    ESP_LOGV(__FUNCTION__,"No pin at idx:%d",idx);
                }
            }
            if (pin) {
                pin->ProcessEvent((Pin::eventIds)id,id == Pin::eventIds::STATUS ? cJSON_GetObjectItem(cJSON_GetObjectItem(params,"state"),"value")->valueint : pin->pinStatus->valueint ? 0 : 1);
            } else {
                char* stmp = cJSON_Print(params);
                ESP_LOGV(__FUNCTION__,"Invalid pin %s", stmp);
                ldfree(stmp);
            }
        } else {
            ESP_LOGE(__FUNCTION__,"Missing params");
        }
    }
}

bool Pin::ProcessEvent(Pin::eventIds event,uint8_t state){
    if (flags&gpio_driver_t::driver_type_t::digital_in){
        ESP_LOGV(__FUNCTION__,"Skipping event because %s is an input", name);
        return false;
    }
    switch (event)
    {
    case Pin::eventIds::TRIGGER:
    case Pin::eventIds::STATUS:
        if (state > 1) {
            state = pinStatus->valueint ? 0 : 1;
        }
        ESP_LOGV(__FUNCTION__,"Turning(%d) %s %d",event,name,state);
        break;
    case Pin::eventIds::ON:
        ESP_LOGV(__FUNCTION__,"Turning %s ON",name);
        state = 1;
        break;
    case Pin::eventIds::OFF:
        ESP_LOGV(__FUNCTION__,"Turning %s OFF",name);
        state = 0;
        break;
    default:
        ESP_LOGE(__FUNCTION__,"Invalid event id for %s",name);
        break;
    }
    if ((state <= 1) && (state != pinStatus->valueint)) {
        gpio_set_level(pinNo,state);
        ESP_LOGV(__FUNCTION__,"Pin %s from %d to %d",name,pinStatus->valueint,state);
        if (!isRtcGpio || flags&gpio_driver_t::driver_type_t::digital_out) {
            cJSON_SetIntValue(pinStatus,state);
            if (cJSON_PrintPreallocated(ManagedDevice::BuildStatus(this),this->buf,1024,false)){
                //AppConfig::SignalStateChange(state_change_t::MAIN);
                esp_event_post(eventBase,pinStatus->valueint ? eventIds::ON : eventIds::OFF,buf,strlen(buf),portMAX_DELAY);
                return true;
            } else {
                ESP_LOGW(__FUNCTION__,"Could not parse status");
            }
        }
    }
    return false;
}

bool Pin::HealthCheck(void* instance){
    Pin* thePin = (Pin*)instance;
    int curState = gpio_get_level(thePin->pinNo);
    if (thePin->pinStatus && thePin->pinStatus->valueint != curState) {
        ESP_LOGW(__FUNCTION__,"State discrepancy on pin %s, expected %d got %d",thePin->GetName(), thePin->pinStatus->valueint, curState);
        cJSON_SetIntValue(thePin->pinStatus,curState);
        ESP_LOGW(__FUNCTION__,"State discrepancy fixed on pin %s, %d == %d",thePin->GetName(), thePin->pinStatus->valueint, curState);
        thePin->PostEvent((void*)&thePin->pinNo,sizeof(thePin->pinNo),thePin->pinStatus->valueint ? eventIds::ON : eventIds::OFF);
        return false;
    }
    return true;
}