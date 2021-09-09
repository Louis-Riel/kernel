#include "eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

uint8_t ManagedDevice::numDevices=0;
ManagedDevice* ManagedDevice::runningInstances[MAX_NUM_DEVICES];

ManagedDevice::ManagedDevice(char *type,char* name,cJSON* (*statusFnc)(void*))
:eventBase((esp_event_base_t)dmalloc(strlen(type)+1))
,handlerDescriptors(NULL)
,statusFnc(statusFnc)
,status(NULL)
{
  strcpy((char*)eventBase, type);
  if (numDevices == 0) {
    memset(runningInstances,0,sizeof(void*)*MAX_NUM_DEVICES);
  }
  bool foundEmptySpot=false;
  for (uint8_t idx = 0 ; idx < numDevices; idx++ ) {
    if (runningInstances[idx] == NULL){
      runningInstances[idx] = this;
      foundEmptySpot=true;
      break;
    }
  }
  if (!foundEmptySpot)
    runningInstances[numDevices++]=this;
  this->name = (char*)dmalloc(strlen(name)+1);
  strcpy(this->name,name);
}

const char* ManagedDevice::GetName() {
  return name;
}

ManagedDevice::ManagedDevice(char* type)
:ManagedDevice(type,type,&BuildStatus)
{
}

ManagedDevice::~ManagedDevice() {
  for (uint8_t idx = 0 ; idx < numDevices; idx++ ) {
    if (runningInstances[idx] == this){
      ESP_LOGV(__FUNCTION__,"Removing %s from idx %d",GetName(),idx);
      runningInstances[idx]=NULL;
    }
  }
  ldfree((void*)eventBase);
}

EventHandlerDescriptor* ManagedDevice::BuildHandlerDescriptors(){
  return new EventHandlerDescriptor(eventBase,(char*)ManagedDevice::eventBase);
}

void ManagedDevice::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){

}

esp_err_t ManagedDevice::PostEvent(void* content, size_t len,int32_t event_id){
    return esp_event_post(eventBase,event_id,content,len,portMAX_DELAY);
}

void ManagedDevice::UpdateStatuses(){
  for (uint8_t idx = 0 ; idx < numDevices; idx++ ) {
    if (runningInstances[idx] && runningInstances[idx]->statusFnc){
      ESP_LOGV(__FUNCTION__,"Refreshing %s",runningInstances[idx]->GetName());
      runningInstances[idx]->status = runningInstances[idx]->statusFnc(runningInstances[idx]);
      ESP_LOGV(__FUNCTION__,"Refreshed %s",runningInstances[idx]->GetName());
    }
  }
}

cJSON* ManagedDevice::BuildStatus(void* instance){
  ManagedDevice* md = (ManagedDevice*) instance;
  if (md && (md->status == NULL)) {
    md->status = AppConfig::GetAppStatus()->GetJSONConfig(md->GetName());
    if (md->status && md->GetName())
      cJSON_AddStringToObject(md->status,"name",md->GetName());
    else
      ESP_LOGW(__FUNCTION__,"Missing status for %s", md->GetName()?md->GetName():"null");
  }
  return md->status;
}
