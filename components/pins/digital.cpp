#include "pins.h"
#include "esp_sleep.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static Pin** pins=nullptr;
uint8_t Pin::numPins=0;
QueueHandle_t Pin::eventQueue;
esp_event_handler_instance_t* Pin::handlerInstance=nullptr;
const char* Pin::PIN_BASE="DigitalPin";
bool Pin::isRtcGpioInitialized = false;

Pin::~Pin(){
    ldfree((void*)name);
    for (int idx=0; idx < MAX_NUM_PINS; idx++){
        if (pins[idx] == this) {
            pins[idx]=nullptr;
            numPins--;
        }
    }
    if (numPins == 0) {
        EventManager::UnRegisterEventHandler(handlerDescriptors);
    }
    ESP_LOGI(__PRETTY_FUNCTION__,"Destructor");
}

Pin::Pin(AppConfig* config)
    :ManagedDevice(PIN_BASE,config->GetStringProperty("pinName"),nullptr,&ProcessCommand,&ProcessEvent),
    pinNo(config->GetPinNoProperty("pinNo")),
    flags(config->GetIntProperty("driverFlags")),
    config(config),
    pinStatus(nullptr)
{
    InitDevice();
}

cJSON* Pin::BuildConfigTemplate() {
    cJSON* commandTemplate = ManagedDevice::BuildConfigTemplate();
    cJSON_SetValuestring(cJSON_GetObjectItem(commandTemplate,"class"),"Pin");
    cJSON_AddStringToObject(commandTemplate,"name","New Analog Pin");
    cJSON_AddTrueToObject(commandTemplate, "isArray");
    cJSON_AddStringToObject(commandTemplate,"collectionName","pins");
    cJSON_AddNumberToObject(commandTemplate,"pinNo",1);
    cJSON_AddNumberToObject(commandTemplate,"driverFlags",1);
    return commandTemplate;
}

EventHandlerDescriptor* Pin::BuildHandlerDescriptors(){
  ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d):%s BuildHandlerDescriptors",pinNo,name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(eventIds::OFF,"OFF",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::ON,"ON",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::TRIGGER,"TRIGGER",event_data_type_tp::JSON);
  handler->AddEventDescriptor(eventIds::STATUS,"STATUS",event_data_type_tp::JSON);
  return handler;
}

void Pin::InitDevice(){
    if (pins == nullptr){
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
        pins = (Pin**)dmalloc(sizeof(void*)*MAX_NUM_PINS);
        memset(pins,0,sizeof(void*)*MAX_NUM_PINS);
        ESP_LOGV(__PRETTY_FUNCTION__,"Registering handler");
        PollPins();
    }
    ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d):%s at idx:%d",pinNo,name,numPins);
    pins[numPins++]=this;

    ESP_LOGI(__PRETTY_FUNCTION__,"Initializing pin %d as %s(%d)",pinNo,name,numPins);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = 1ULL << pinNo;
    io_conf.mode = flags&gpio_driver_t::driver_type_t::digital_out ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    io_conf.pull_up_en = flags&gpio_driver_t::driver_type_t::pullup?GPIO_PULLUP_ENABLE:GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = flags&gpio_driver_t::driver_type_t::pullup?GPIO_PULLDOWN_ENABLE:GPIO_PULLDOWN_DISABLE;
    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK) {
        ESP_LOGE(__PRETTY_FUNCTION__,"Error in pin(%d):%s init failed: %s", pinNo, name, esp_err_to_name(ret));
    }

    if (flags&gpio_driver_t::driver_type_t::wakeonhigh || flags&gpio_driver_t::driver_type_t::wakeonlow) {
        ret = gpio_wakeup_enable(pinNo, flags&gpio_driver_t::driver_type_t::wakeonhigh ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);

        if (ret != ESP_OK) {
            ESP_LOGE(__PRETTY_FUNCTION__,"Error in pin(%d):%s wakeup failed: %s", pinNo, name, esp_err_to_name(ret));
        } else {
            ret = esp_sleep_enable_gpio_wakeup();

            if (ret != ESP_OK) {
                ESP_LOGE(__PRETTY_FUNCTION__,"Error in pin(%d):%s gpio wakeup failed: %s", pinNo, name, esp_err_to_name(ret));
            }
        }
    }

    ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d):%s Direction:%s",pinNo,name,flags&gpio_driver_t::driver_type_t::digital_out ? "Out":"In");
    if (flags&gpio_driver_t::driver_type_t::pullup) {
        ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d):%s pullup enabled",pinNo,name);
    }
    if (flags&gpio_driver_t::driver_type_t::pulldown) {
        ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d):%s pulldown enabled",pinNo,name);
    }
    isRtcGpio = rtc_gpio_is_valid_gpio(pinNo);
    ESP_LOGV(__PRETTY_FUNCTION__, "Pin %d: RTC:%d", pinNo, isRtcGpio);
    ESP_LOGV(__PRETTY_FUNCTION__, "Pin %d: Level:%d", pinNo, gpio_get_level(pinNo));
    ESP_LOGV(__PRETTY_FUNCTION__, "Pin %d: 0x%" PRIXPTR "", pinNo,(uintptr_t)this);
    ESP_LOGV(__PRETTY_FUNCTION__, "Numpins %d", numPins);
    if (isRtcGpio) {
        if (!isRtcGpioInitialized) {
            isRtcGpioInitialized = gpio_install_isr_service(0) == ESP_OK;
        }
        if (isRtcGpioInitialized) {
            ESP_ERROR_CHECK(gpio_isr_handler_add(pinNo, pinHandler, this));
        } else {
            ESP_LOGE(__PRETTY_FUNCTION__,"Failed to setup isr for Pin(%d):%s",pinNo,name);
        }
    }

    if (status == nullptr) {
        ESP_LOGE(__PRETTY_FUNCTION__,"No status object for %s",name);
    }
    AppConfig* apin = new AppConfig(status,AppConfig::GetAppStatus());
    apin->SetPinNoProperty("pinNo",pinNo);
    apin->SetStringProperty("name",name);
    apin->SetIntProperty("state",-1);
    pinStatus = apin->GetPropertyHolder("state");
    if (flags&gpio_driver_t::driver_type_t::digital_out){
        cJSON* methods = cJSON_AddArrayToObject(status,"commands");
        cJSON* flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"className",PIN_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","ON");
        cJSON_AddStringToObject(flush,"caption","On");
        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"className",PIN_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","OFF");
        cJSON_AddStringToObject(flush,"caption","Off");
        flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","trigger");
        cJSON_AddStringToObject(flush,"className",PIN_BASE);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1","TRIGGER");
        cJSON_AddStringToObject(flush,"caption","Trigger");
    }

    delete apin;
}

void Pin::RefrestState(){
    int curState = gpio_get_level(pinNo);
    if (curState != pinStatus->valueint) {
        cJSON_SetIntValue(pinStatus,curState);
        ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d)%s RefreshState:%s",pinNo, eventBase,pinStatus->valueint?"On":"Off");
        postedEvent_t pevent;
        pevent.base=PIN_BASE;
        pevent.id=curState?eventIds::ON : eventIds::OFF;
        pevent.event_data=status;
        pevent.eventDataType=event_data_type_tp::JSON;
        EventManager::ProcessEvent(this, &pevent);
    }
}

void Pin::queuePoller(void *arg){
    Pin* pin = nullptr;

    EventGroupHandle_t appEg = getAppEG();
    while(!(xEventGroupGetBits(appEg) & app_bits_t::HIBERNATE) && xQueueReceive(eventQueue,&pin,portMAX_DELAY)){
        pin->RefrestState();
    }
    ESP_LOGE(__PRETTY_FUNCTION__,"%s","Failed");
}

void Pin::pinHandler(void *arg)
{
  xQueueSendFromISR(eventQueue, &arg, nullptr);
}

void Pin::PollPins(){
  ESP_LOGV(__PRETTY_FUNCTION__, "Configuring Pins.");
  eventQueue = xQueueCreate(10, sizeof(uint32_t));
  CreateBackgroundTask(queuePoller, "pinsPoller", 4096, nullptr, tskIDLE_PRIORITY, nullptr);
  ESP_LOGV(__PRETTY_FUNCTION__, "ISR Service Started");
}

void Pin::ProcessEvent(ManagedDevice* dev, postedEvent_t* postedEvent){
    if (dev && postedEvent) {
        ESP_LOGV(__PRETTY_FUNCTION__,"posting to %s",dev ->GetName());
        ((Pin*) dev)->ProcessTheEvent(postedEvent);
    } else {
        ESP_LOGW(__PRETTY_FUNCTION__,"Stuff's missing dev:%d event:%d", dev==nullptr, postedEvent==nullptr);
    }
}

void Pin::ProcessTheEvent(postedEvent_t* postedEvent){
    if (flags&gpio_driver_t::driver_type_t::digital_out){
        int cPinNo = postedEvent->eventDataType == event_data_type_tp::Number ? *((int*)postedEvent->event_data) : -1;
        if (postedEvent->eventDataType == event_data_type_tp::Number) {
            cPinNo = *((int*)postedEvent->event_data);
            ESP_LOGV(__PRETTY_FUNCTION__,"d: %d==%d",cPinNo, pinNo);
        } else if (postedEvent->event_data) {
            cJSON* event = (cJSON*)postedEvent->event_data;
            cJSON* jpinNo = event ? cJSON_GetObjectItem(event, "pinNo") :nullptr;
            cPinNo = cJSON_GetNumberValue(jpinNo);
            ESP_LOGV(__PRETTY_FUNCTION__,"j: %d==%d",cPinNo, pinNo);
        }  else {
            ESP_LOGW(__PRETTY_FUNCTION__,"Missing event data");
        }
        if (cPinNo == pinNo){
            uint8_t state = 2;
            switch (postedEvent->id)
            {
            case Pin::eventIds::TRIGGER:
            case Pin::eventIds::STATUS:
                state = pinStatus->valueint ? 0 : 1;
                ESP_LOGV(__PRETTY_FUNCTION__,"Turning(%d) %s %d",pinNo,name,state);
                break;
            case Pin::eventIds::ON:
                ESP_LOGV(__PRETTY_FUNCTION__,"Turning %s ON",name);
                state = 1;
                break;
            case Pin::eventIds::OFF:
                ESP_LOGV(__PRETTY_FUNCTION__,"Turning %s OFF",name);
                state = 0;
                break;
            default:
                ESP_LOGE(__PRETTY_FUNCTION__,"Invalid event id for %s",name);
                break;
            }
            if ((state <= 1) && (state != pinStatus->valueint)) {
                gpio_set_level(pinNo,state);
                cJSON_SetIntValue(pinStatus,state);
                EventManager::ProcessEvent(this, postedEvent);
                ESP_LOGV(__PRETTY_FUNCTION__,"Pin %s from %d to %d",name,pinStatus->valueint,state);
            }
        }
    }
}

bool Pin::HealthCheck(void* instance){
    if (ManagedDevice::HealthCheck(instance)){
        Pin* thePin = (Pin*)instance;
        int curState = gpio_get_level(thePin->pinNo);
        if (thePin->pinStatus && thePin->pinStatus->valueint == curState) {
            return true;
        }
        ESP_LOGW(__PRETTY_FUNCTION__,"State discrepancy on pin %s, expected %d got %d",thePin->GetName(), thePin->pinStatus->valueint, curState);
        cJSON_SetIntValue(thePin->pinStatus,curState);
        ESP_LOGW(__PRETTY_FUNCTION__,"State discrepancy fixed on pin %s, %d == %d",thePin->GetName(), thePin->pinStatus->valueint, curState);
        thePin->PostEvent((void*)&thePin->pinNo,sizeof(thePin->pinNo),thePin->pinStatus->valueint ? eventIds::ON : eventIds::OFF);
    }
    return false;
}

bool Pin::ProcessCommand(ManagedDevice* dev, cJSON * parms) {
    Pin* pin = (Pin*)dev;
    if (strcmp(pin->GetName(),cJSON_GetObjectItem(parms,"name")->valuestring)==0) {
        cJSON *event = cJSON_GetObjectItemCaseSensitive(parms, "param1");
        cJSON *state = cJSON_GetObjectItemCaseSensitive(parms, "param2");
        postedEvent_t pevent;
        pevent.id = strcmp(event->valuestring,"TRIGGER") == 0 ? Pin::eventIds::TRIGGER : strcmp(event->valuestring,"ON") == 0 ? Pin::eventIds::ON : Pin::eventIds::OFF;
        pevent.event_data = pin->status;
        pevent.eventDataType = event_data_type_tp::JSON;
        pevent.base = pin->eventBase;
        pin->ProcessTheEvent(&pevent);
        return true;
    }
    return false;
}
