#include "./eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

esp_event_base_t ManagedDevice::eventBase = NULL;
EventHandlerDescriptor* ManagedDevice::handlerDescriptors = NULL;

ManagedDevice::ManagedDevice(AppConfig* config, char* type):config(config) {
  if (ManagedDevice::eventBase == NULL) {
      ManagedDevice::eventBase = (esp_event_base_t)dmalloc(strlen(type)+1);
      strcpy((char*)ManagedDevice::eventBase,type);
  }
}

EventHandlerDescriptor* ManagedDevice::BuildHandlerDescriptors(){
  return new EventHandlerDescriptor(ManagedDevice::eventBase,(char*)ManagedDevice::eventBase);
}

void ManagedDevice::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){

}

void ManagedDevice::PostEvent(void* content, size_t len,int32_t event_id){
    esp_err_t ret = esp_event_post(eventBase,event_id,content,len,portMAX_DELAY);
}

void ManagedDevice::InitDevice(){

}

cJSON* ManagedDevice::GetStatus(){
    return cJSON_CreateObject();
}