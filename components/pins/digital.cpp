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
    pinStatus(NULL)
{
    const char* pname = config->GetStringProperty("pinName");
    ldfree(name);
    uint32_t sz = strlen(pname)+1;
    name = (char*)malloc(200);
    if (sz > 0){
        sprintf(name,"%s pin(%d)",pname,pinNo);
    } else {
        sprintf(name,"DigitalPin pin(%d)",pinNo);
    }
    ESP_LOGV(__FUNCTION__,"Pin(%d):%s",pinNo,name);
    status = BuildStatus(this);
    EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(Pin::eventBase, ESP_EVENT_ANY_ID, ProcessEvent, this, NULL));

    if (pins == NULL){
        pins = (Pin**)dmalloc(sizeof(void*)*MAX_NUM_PINS);
        memset(pins,0,sizeof(void*)*MAX_NUM_PINS);
        ESP_LOGV(__FUNCTION__,"Registering handler");
        PollPins();
    }
    pins[numPins++]=this;
    InitDevice();
}

EventHandlerDescriptor* Pin::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"Pin(%d):%s BuildHandlerDescriptors",pinNo,name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(eventIds::OFF,"OFF");
  handler->AddEventDescriptor(eventIds::ON,"ON");
  handler->AddEventDescriptor(eventIds::TRIGGER,"TRIGGER");
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
    ESP_LOGV(__FUNCTION__, "Pin %d: RTC:%d", pinNo, isRtcGpio);
    ESP_LOGV(__FUNCTION__, "Pin %d: Level:%d", pinNo, gpio_get_level(pinNo));
    ESP_LOGV(__FUNCTION__, "Pin %d: 0x%" PRIXPTR "", pinNo,(uintptr_t)this);
    ESP_LOGV(__FUNCTION__, "Numpins %d", numPins);
    ESP_ERROR_CHECK(gpio_isr_handler_add(pinNo, pinHandler, this));
}

cJSON* Pin::BuildStatus(void* instance){
    Pin* pin = (Pin*) instance;
    ESP_LOGV(__FUNCTION__,"Pin(%d):%s GetStatus",pin->pinNo,pin->name);

    cJSON* sjson = NULL;
    AppConfig* apin = new AppConfig(sjson=ManagedDevice::BuildStatus(instance),AppConfig::GetAppStatus());
    apin->SetPinNoProperty("pinNo",pin->pinNo);
    apin->SetStringProperty("name",pin->name);
    apin->SetBoolProperty("state",pin->state);
    pin->pinStatus = AppConfig::GetPropertyHolder(apin->GetJSONConfig("state"));
    delete apin;
    return sjson;
}

void Pin::RefrestState(){
    bool curState = gpio_get_level(pinNo);
    if (curState != state) {
        cJSON_SetIntValue(pinStatus,curState);
        state=curState;
        ESP_LOGV(__FUNCTION__,"Pin(%d)%s RefreshState:%s",pinNo, eventBase,state?"On":"Off");
        PostEvent((void*)&pinNo,sizeof(pinNo),state ? eventIds::ON : eventIds::OFF);
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
    ESP_LOGV(__FUNCTION__,"Params(0x%" PRIXPTR " 0x%" PRIXPTR ")",(uintptr_t)event_data,(uintptr_t)params);
    uint32_t state;
    if ((params != NULL) && ((((uintptr_t)params) < 35) || cJSON_HasObjectItem(params,"pinNo"))){
        uint32_t pinNo = ((uintptr_t)params) < 35?(uintptr_t)params:cJSON_GetObjectItem(cJSON_GetObjectItem(params,"pinNo"),"value")->valueint;
        for (uint32_t idx = 0; idx < numPins; idx++) {
            if (pins[idx]){
                ESP_LOGV(__FUNCTION__,"%d:Checking %s %d",idx, pins[idx]->name,pins[idx]->pinNo);
                if (pins[idx]->pinNo == pinNo) {
                    pin = pins[idx];
                    break;
                }
            } else {
                ESP_LOGV(__FUNCTION__,"No pin at idx:%d",idx);
            }
        }
        if (pin) {
            if (pin->flags&gpio_driver_t::driver_type_t::digital_in){
                ESP_LOGV(__FUNCTION__,"Skipping event because %s is an input", pin->name);
                return;
            }
            switch (id)
            {
            case Pin::eventIds::TRIGGER:
                state = cJSON_GetObjectItem(cJSON_GetObjectItem(params,"state"),"value")->valueint;
                ESP_LOGV(__FUNCTION__,"Turning %s %d",pin->name,state);
                gpio_set_level((gpio_num_t)pinNo,state);
                break;
            case Pin::eventIds::ON:
                ESP_LOGV(__FUNCTION__,"Turning %s ON",pin->name);
                gpio_set_level((gpio_num_t)pinNo,1);
                break;
            case Pin::eventIds::OFF:
                ESP_LOGV(__FUNCTION__,"Turning %s OFF",pin->name);
                gpio_set_level((gpio_num_t)pinNo,0);
                break;
            default:
                ESP_LOGE(__FUNCTION__,"Invalid event id for %s",pin->name);
                break;
            }
        } else {
            ESP_LOGV(__FUNCTION__,"Invalid PinNo %d", pinNo);
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Missing params");
    }
}
