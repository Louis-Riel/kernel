#include "./eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

ManagedDevice::ManagedDevice(char* type)
:eventBase((esp_event_base_t)dmalloc(strlen(type)+1))
,handlerDescriptors(NULL)
,status(BuildStatus())
{
  strcpy((char*)eventBase, type);
}

ManagedDevice::~ManagedDevice() {
  ldfree((void*)eventBase);
}

EventHandlerDescriptor* ManagedDevice::BuildHandlerDescriptors(){
  return new EventHandlerDescriptor(eventBase,(char*)ManagedDevice::eventBase);
}

void ManagedDevice::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){

}

void ManagedDevice::PostEvent(void* content, size_t len,int32_t event_id){
    esp_event_post(eventBase,event_id,content,len,portMAX_DELAY);
}

cJSON* ManagedDevice::GetStatus(){
    return status == NULL ? status = BuildStatus() : status;
}

cJSON* ManagedDevice::BuildStatus(){
    return cJSON_CreateObject();
}
