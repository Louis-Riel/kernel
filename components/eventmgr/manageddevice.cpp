#include "eventmgr.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define ERROR_MIN_TIME 5*1000*60

uint8_t ManagedDevice::numDevices=0;
uint32_t ManagedDevice::numErrors=0;
uint64_t ManagedDevice::lastErrorTs=0;
ManagedDevice* ManagedDevice::runningInstances[MAX_NUM_DEVICES];

ManagedDevice::ManagedDevice(const char* type)
:ManagedDevice(type,type,&BuildStatus)
{
}

ManagedDevice::ManagedDevice(const char *type,const char* name,cJSON* (*statusFnc)(void*))
:ManagedDevice(type, name, statusFnc, &HealthCheck)
{

}

ManagedDevice::ManagedDevice(const char *type,const char *name, cJSON *(*statusFnc)(void *),bool (hcFnc)(void*))
:eventBase((esp_event_base_t)dmalloc(strlen(type)+1))
,handlerDescriptors(NULL)
,statusFnc(statusFnc)
,hcFnc(hcFnc)
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
  ESP_LOGV(__FUNCTION__,"Posting %s(%d) with a message %d bytes long",eventBase,event_id,len);
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
  if (instance == NULL) {
    ESP_LOGE(__FUNCTION__,"Missing instance");
  }
  ManagedDevice* md = (ManagedDevice*) instance;
  if (md && (md->status == NULL)) {
    md->status = AppConfig::GetAppStatus()->GetJSONConfig(md->GetName());
    if (md->status && md->GetName()){
      cJSON_AddStringToObject(md->status,"name",md->GetName());
      return md->status;
    } else{
      ESP_LOGE(__FUNCTION__,"Missing name");
    }
  }
  return md->status;
}

bool ManagedDevice::ValidateDevices(){
  bool hasIssues = false;
  for (uint32_t idx = 0; idx < MAX_NUM_DEVICES; idx++) {
    if (runningInstances[idx]) {
      if (!runningInstances[idx]->hcFnc(runningInstances[idx])){
        hasIssues = true;
        numErrors++;
        lastErrorTs = esp_timer_get_time();
      }
    }
  }
  return !hasIssues;
}

void ManagedDevice::RunHealthCheck(void* param) {
  if (!ValidateDevices()){
    dumpTheLogs((void*)true);
    if (numErrors > 5)
      esp_restart();
  } else if (lastErrorTs) {
    if ((esp_timer_get_time() - lastErrorTs) > ERROR_MIN_TIME) {
      numErrors = 0;
    }
  }
}

bool ManagedDevice::HealthCheck(void* instance){
  if (instance == NULL) {
    ESP_LOGE(__FUNCTION__,"Missing instance to validate");
  }
  ManagedDevice* dev = (ManagedDevice*)instance;
  if (!heap_caps_check_integrity_all(true)) {
      ESP_LOGE(dev->name,"bcaps integrity error");
      return false;
  }
  uint32_t freeMem = esp_get_free_heap_size();
  if (freeMem < 800000) {
    ESP_LOGE(__FUNCTION__,"Running low on mem: %d",freeMem);
    return false;
  }
  ESP_LOGV(dev->name,"All good");
  return true;
}