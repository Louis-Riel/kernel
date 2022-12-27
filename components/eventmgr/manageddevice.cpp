#include "eventmgr.h"
#include "../../main/utils.h"
#include "../../components/pins/pins.h"
#include "../../components/servo/servo.h"
#include "../../components/apa102/apa102.h"
#include "../../components/camera/include/camera.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define ERROR_MIN_TIME 5*1000*60

uint8_t ManagedDevice::numDevices=0;
uint32_t ManagedDevice::numErrors=0;
uint64_t ManagedDevice::lastErrorTs=0;
ManagedDevice* ManagedDevice::runningInstances[MAX_NUM_DEVICES];
cJSON* ManagedDevice::configTemplates=NULL;


ManagedDevice::ManagedDevice(const char* type)
:ManagedDevice(type,NULL,NULL,NULL)
{
}

ManagedDevice::ManagedDevice(const char *type,const char* name)
:ManagedDevice(type, name,NULL,NULL)
{

}

ManagedDevice::ManagedDevice(const char *type,const char *name, bool (*hcFnc)(void*),bool (*commandFnc)(ManagedDevice* instance, cJSON *))
:ManagedDevice(type, name,NULL,NULL,NULL)
{

}

ManagedDevice::ManagedDevice(const char *type,const char *name, bool (*hcFnc)(void*),bool (*commandFnc)(ManagedDevice* instance, cJSON *),void(*processEventFnc)(ManagedDevice*, postedEvent_t*))
:eventBase((esp_event_base_t)type)
,handlerDescriptors(NULL)
,processEventFnc(processEventFnc)
,status(NULL)
,hcFnc(hcFnc == NULL ? &HealthCheck : hcFnc)
,commandFnc(commandFnc)
,name(strdup(name == NULL ? type : name))
{
  GetConfigTemplates();
  if(!ValidateDevices())
  {
      ESP_LOGE(__FUNCTION__,"Too many devices");
      return;
  }
  status=BuildStatus(this);

  if (numDevices == 0) {
    memset(runningInstances,0,sizeof(void*)*MAX_NUM_DEVICES);
    runningInstances[numDevices++]=this;
  } else {
    bool foundEmptySpot=false;
    for (uint8_t idx = 0 ; idx < numDevices; idx++ ) {
      if (runningInstances[idx] == NULL){
        runningInstances[idx] = this;
        foundEmptySpot=true;
        break;
      }
    }
    if (!foundEmptySpot){
      runningInstances[numDevices++]=this;
    }
  }

  if (this->name == NULL) {
    ESP_LOGE(__FUNCTION__,"Device name is null for %s", type);
  } else {
    ESP_LOGI(__FUNCTION__,"Created device %s/%s",type,this->name);
  }
}

cJSON* ManagedDevice::GetConfigTemplates(){
  if (!configTemplates) {
    configTemplates = cJSON_CreateArray();
    cJSON_AddItemToArray(configTemplates, AnalogPin::BuildConfigTemplate());
    cJSON_AddItemToArray(configTemplates, Pin::BuildConfigTemplate());
    cJSON_AddItemToArray(configTemplates, Servo::BuildConfigTemplate());
    cJSON_AddItemToArray(configTemplates, Apa102::BuildConfigTemplate());
    cJSON_AddItemToArray(configTemplates, Camera::BuildConfigTemplate());
  }
  return configTemplates;
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
  ldfree((void*)name);
}

bool ManagedDevice::ProcessCommand(cJSON *command){
  if (commandFnc != NULL)
    return commandFnc(this,command);
  return false;
}


EventHandlerDescriptor* ManagedDevice::BuildHandlerDescriptors(){
  return new EventHandlerDescriptor(eventBase,(char*)ManagedDevice::eventBase);
}

void ManagedDevice::ProcessEvent(postedEvent_t* postedEvent){
  if (processEventFnc != NULL)
    processEventFnc(this,postedEvent);
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

ManagedDevice** ManagedDevice::GetRunningInstances(){
  return runningInstances;
}

uint32_t ManagedDevice::GetNumRunningInstances(){
  return numDevices;
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
      if (runningInstances[idx]->hcFnc && !runningInstances[idx]->hcFnc(runningInstances[idx])){
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
  if (!heap_caps_check_integrity_all(true)) {
      ESP_LOGE(__FUNCTION__,"bcaps integrity error");
      esp_restart();
  }

  if (!ValidateDevices()){
    if (numErrors > 5){
      esp_restart();
    }
  } else if (lastErrorTs) {
    if ((esp_timer_get_time() - lastErrorTs) > ERROR_MIN_TIME) {
      numErrors = 0;
    }
  }
}

bool ManagedDevice::HealthCheck(void* instance){
  if (instance == NULL) {
    ESP_LOGE(__FUNCTION__,"Missing instance to validate");
    return false;
  }
  ManagedDevice* dev = (ManagedDevice*)instance;
  if (cJSON_IsInvalid(dev->status)){
    ESP_LOGE(__FUNCTION__,"Invalid status for %s",dev->name);
    return false;
  }

  ESP_LOGV(dev->name,"All good");
  return true;
}

cJSON* ManagedDevice::BuildConfigTemplate(){
  cJSON* cfg = cJSON_CreateObject();
  cJSON_AddStringToObject(cfg,"class","");
  return cfg;
}
