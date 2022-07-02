#include "eventmgr.h"
#include "../../main/utils.h"

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
:ManagedDevice(type, name, statusFnc, hcFnc, NULL)
{

}

ManagedDevice::ManagedDevice(const char *type,const char *name, cJSON *(*statusFnc)(void *),bool (hcFnc)(void*),bool (*commandFnc)(ManagedDevice* instance, cJSON *))
:ManagedDevice(type, name, statusFnc, hcFnc, commandFnc, NULL)
{

}

ManagedDevice::ManagedDevice(const char *type,const char *name, cJSON *(*statusFnc)(void *),bool (hcFnc)(void*),bool (*commandFnc)(ManagedDevice* instance, cJSON *),cJSON* (*configFnc)(ManagedDevice* instance))
:eventBase((esp_event_base_t)dmalloc(strlen(type)+1))
,handlerDescriptors(NULL)
,statusFnc(statusFnc == NULL ? &BuildStatus : statusFnc)
,hcFnc(hcFnc == NULL ? &HealthCheck : hcFnc)
,commandFnc(commandFnc)
,configFnc(configFnc == NULL ? &buildConfigTemplate : configFnc)
,status(statusFnc(this))
,configTemplate(configFnc(this))
{
  if(!ValidateDevices())
  {
      ESP_LOGE(__FUNCTION__,"Too many devices");
      return;
  }
  if(!name)
  {
      name=type;
  }
  this->name=strdup(name);
  runningInstanes[numDevices++]=this;
  ESP_LOGI(__FUNCTION__,"Created device %s",name);

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

cJSON* ManagedDevice::getConfigTemplate(){
  return configTemplate;
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

bool ManagedDevice::ProcessCommand(cJSON *command){
  if (commandFnc != NULL)
    return commandFnc(this,command);
  return false;
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

ManagedDevice* ManagedDevice::GetByName(const char* name){
  for (int idx = 0; idx < numDevices; idx++) {
    if (runningInstances[idx] && runningInstances[idx]->status && (strcmp(runningInstances[idx]->GetName(),name) == 0)) {
      return runningInstances[idx];
    }
  }
  return NULL;
}

ManagedDevice** ManagedDevice::GetRunningInstanes(){
  return runningInstances;
}

uint32_t ManagedDevice::GetNumRunningInstances(){
  return numDevices;
}


void ManagedDevice::UpdateStatuses(){
  for (uint8_t idx = 0 ; idx < numDevices; idx++ ) {
    if (runningInstances[idx] && runningInstances[idx]->statusFnc){
      runningInstances[idx]->status = runningInstances[idx]->statusFnc(runningInstances[idx]);
      ESP_LOGV(__FUNCTION__,"Refreshed %s",runningInstances[idx]->GetName());
    }
  }
}

cJSON* ManagedDevice::BuildStatus(void* instance){
  ManagedDevice* md = (ManagedDevice*) instance;
  if (md) {
    if (md->status == NULL) {
      md->status = AppConfig::GetAppStatus()->GetJSONConfig(md->GetName());
      cJSON_AddStringToObject(md->status,"name",md->GetName());
      cJSON_AddStringToObject(md->status,"class",md->eventBase);
    }
    return md->status;
  }
  ESP_LOGE(__FUNCTION__,"Missing instance");
  return NULL;
}

bool ManagedDevice::ValidateDevices(){
  bool hasIssues = false;
  for (uint32_t idx = 0; idx < MAX_NUM_DEVICES; idx++) {
    if (runningInstances[idx]) {
      size_t stacksz = heap_caps_get_free_size(MALLOC_CAP_32BIT);
      if (!runningInstances[idx]->hcFnc(runningInstances[idx])){
        ESP_LOGW(__FUNCTION__,"HC Failed for %s",runningInstances[idx]->GetName());
        hasIssues = true;
        numErrors++;
        lastErrorTs = esp_timer_get_time();
      }
      size_t diff = heap_caps_get_free_size(MALLOC_CAP_32BIT) - stacksz;
      if (diff > 0) {
          ESP_LOGW(__FUNCTION__,"hc:%s %d bytes memleak",runningInstances[idx]->name,diff);
      }
    }
  }
  return !hasIssues;
}

void ManagedDevice::RunHealthCheck(void* param) {
  if (!ValidateDevices()){
    dumpLogs();
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

  if (cJSON_IsInvalid(dev->status)){
    ESP_LOGE(__FUNCTION__,"Invalid status for %s",dev->name);
    return false;
  }

  ESP_LOGV(dev->name,"All good");
  return true;
}

cJSON* ManagedDevice::buildConfigTemplate(ManagedDevice* instance){
  if (instance == NULL) {
    ESP_LOGE(__FUNCTION__,"Missing instance to validate");
    return NULL;
  }
  cJSON* cfg = cJSON_CreateObject();
  cJSON_AddStringToObject(cfg,"name",instance->GetName());
  cJSON_AddStringToObject(cfg,"class",instance->eventBase);
  return cfg;
}
