#include "pins.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

Pin* Pin::pins[];
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
    char* pname = config->GetStringProperty("pinName");
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
    if (numPins == 0) {
        memset(pins,0,sizeof(void*)*MAX_NUM_PINS);
        PollPins();
    }
    if (handlerDescriptors == NULL)
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
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
    ESP_LOGD(__FUNCTION__,"Initializing pin %d as %s",pinNo,name);
    for (int idx=0; idx < MAX_NUM_PINS; idx++) {
        if (pins[idx] == NULL) {
            pins[idx] = this;
            numPins++;
            break;
        }
    }
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
    valid = rtc_gpio_is_valid_gpio(pinNo);
    ESP_LOGV(__FUNCTION__, "Pin %d: RTC:%d", pinNo, valid);
    ESP_LOGV(__FUNCTION__, "Pin %d: Level:%d", pinNo, gpio_get_level(pinNo));
    if (valid){
        ESP_LOGV(__FUNCTION__,"0x%" PRIXPTR "\n",(uintptr_t)this);
        ESP_ERROR_CHECK(gpio_isr_handler_add(pinNo, pinHandler, this));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(Pin::eventBase, ESP_EVENT_ANY_ID, ProcessEvent, this, NULL));
    }
    RefrestState();
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
    ESP_LOGV(__FUNCTION__,"Event %s-%d",base,id);
    if (strcmp(base,"DigitalPin") == 0) {
        Pin* pin;
        uint8_t idx;
        gpio_num_t pinNo;
        cJSON* params;
        switch (id)
        {
        case Pin::eventIds::TRIGGER:
            params = *(cJSON**)event_data;
            if ((params != NULL) && cJSON_HasObjectItem(params,"pinNo")){
                pin = NULL;
                idx = 0;
                uint32_t state = cJSON_GetObjectItem(cJSON_GetObjectItem(params,"state"),"value")->valueint;
                pinNo = (gpio_num_t)cJSON_GetObjectItem(cJSON_GetObjectItem(params,"pinNo"),"value")->valueint;
                while((pin=Pin::pins[idx++])!= NULL) {
                    if (pin->pinNo == pinNo) {
                        ESP_LOGV(__FUNCTION__,"Turing %d %d",pinNo,state);
                        gpio_set_level(pinNo,state);
                        return;
                    }
                }
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
            }
            break;
        
        default:
            break;
        }
    }
}
