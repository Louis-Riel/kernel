#include "pins.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

Pin* Pin::pins[];
uint8_t Pin::numPins=0;
QueueHandle_t Pin::eventQueue;

Pin::~Pin(){
    ESP_LOGD(__FUNCTION__,"Destructor");
}

Pin::Pin(AppConfig* config)
    :ManagedDevice(config,"DigitalPin"),
    pinNo(config->GetPinNoProperty("pinNo")),
    flags(config->GetIntProperty("driverFlags")),
    name(config->GetStringProperty("pinName"))
{
    char* pname = (char*)malloc(sizeof(name)+1);
    strcpy(pname,name);
    uint32_t sz = strlen(pname)+1;
    name = (char*) malloc(sz);
    name[0]=0;
    if (sz > 1)
        strcpy(name,pname);
    ESP_LOGV(__FUNCTION__,"Pin(%d):%s",pinNo,name);
    
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
    pins[numPins++]=this;
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
        ESP_ERROR_CHECK(esp_event_handler_register(Pin::eventBase, ESP_EVENT_ANY_ID, ProcessEvent, this));
    }
}

cJSON* Pin::GetStatus(){
    ESP_LOGV(__FUNCTION__,"Pin(%d):%s GetStatus",pinNo,name);
    cJSON* status = ManagedDevice::GetStatus();
    cJSON_AddBoolToObject(status,"level",gpio_get_level(pinNo));
    return status;
}

void Pin::RefrestState(){
    bool curState = gpio_get_level(pinNo);
    if (curState != state) {
        state=curState;
        ESP_LOGV(__FUNCTION__,"Pin(%d)%s RefreshState:%s",pinNo, name,state?"On":"Off");
        PostEvent(this,sizeof(void*),state ? eventIds::ON : eventIds::OFF);
    }
}

void Pin::queuePoller(void *arg){
    Pin* pin = NULL;

    while(xQueueReceive(eventQueue,&pin,portMAX_DELAY)){
        pin->RefrestState();
    }
    ESP_LOGE(__FUNCTION__,"%s","Failed");
    vTaskDelete(NULL);
}

void Pin::pinHandler(void *arg)
{
  xQueueSendFromISR(eventQueue, &arg, NULL);
}

void Pin::PollPins(){
  ESP_LOGV(__FUNCTION__, "Configuring Pins.");
  eventQueue = xQueueCreate(10, sizeof(uint32_t));
  xTaskCreate(queuePoller, "pinsPoller", 4096, NULL, tskIDLE_PRIORITY, NULL);
  gpio_install_isr_service(0);
  ESP_LOGV(__FUNCTION__, "ISR Service Started");

}

void Pin::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    ESP_LOGV(__FUNCTION__,"Event %s-%d",base,id);
    Pin* pin;
    uint8_t idx;
    gpio_num_t pinNo;
    cJSON* event;
    cJSON* params;
    switch (id)
    {
    case Pin::eventIds::TRIGGER:
        params = *(cJSON**)event_data;
        if ((params != NULL) && cJSON_HasObjectItem(params,"pinNo")){
            pin = NULL;
            idx = 0;
            pinNo = (gpio_num_t)cJSON_GetObjectItem(cJSON_GetObjectItem(params,"pinNo"),"value")->valueint;
            while((pin=Pin::pins[idx++])!= NULL) {
                if (pin->pinNo == pinNo) {
                    pin->HandleEvent(params);
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

void Pin::HandleEvent(cJSON* params){
    if (cJSON_HasObjectItem(params,"state")) {
        uint32_t state = cJSON_GetObjectItem(cJSON_GetObjectItem(params,"state"),"value")->valueint;
        ESP_LOGV(__FUNCTION__,"Turing %d %d",pinNo,state);
        gpio_set_level(pinNo,state);
    } else {
        ESP_LOGW(__FUNCTION__,"Missing params");
    }
}
