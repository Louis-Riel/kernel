#include "utils.h"
#include "esp_sleep.h"
#include "driver/periph_ctrl.h"
#include "esp_log.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card = NULL;
const char mount_point[] = "/sdcard";
uint8_t numSdCallers=0;

sdmmc_host_t* getSDHost(){
  return &host;
}

static const char* getErrorMsg(int32_t errCode){
  switch (errCode)
  {
    case FR_OK: return "(0) Succeeded"; break;
    case FR_DISK_ERR: return "(1) A hard error occurred in the low level disk I/O layer"; break;
    case FR_INT_ERR: return "(2) Assertion failed"; break;
    case FR_NOT_READY: return "(3) The physical drive cannot work"; break;
    case FR_NO_FILE: return "(4) Could not find the file"; break;
    case FR_NO_PATH: return "(5) Could not find the path"; break;
    case FR_INVALID_NAME: return "(6) The path name format is invalid"; break;
    case FR_DENIED: return "(7) Access denied due to prohibited access or directory full"; break;
    case FR_EXIST: return "(8) Access denied due to prohibited access"; break;
    case FR_INVALID_OBJECT: return "(9) The file/directory object is invalid"; break;
    case FR_WRITE_PROTECTED: return "(10) The physical drive is write protected"; break;
    case FR_INVALID_DRIVE: return "(11) The logical drive number is invalid"; break;
    case FR_NOT_ENABLED: return "(12) The volume has no work area"; break;
    case FR_NO_FILESYSTEM: return "(13) There is no valid FAT volume"; break;
    case FR_MKFS_ABORTED: return "(14) The f_mkfs() aborted due to any problem"; break;
    case FR_TIMEOUT: return "(15) Could not get a grant to access the volume within defined period"; break;
    case FR_LOCKED: return "(16) The operation is rejected according to the file sharing policy"; break;
    case FR_NOT_ENOUGH_CORE: return "(17) LFN working buffer could not be allocated"; break;
    case FR_TOO_MANY_OPEN_FILES: return "(18) Number of open files > FF_FS_LOCK"; break;
    case FR_INVALID_PARAMETER: return "(19) Given parameter is invalid"; break;
    default:
      break;
  }
  return "Invalid error code";
}

bool initSDMMCSDCard(){
  if (numSdCallers == 0){
    ESP_LOGI(__FUNCTION__, "Using SDMMC peripheral");
    periph_module_reset(PERIPH_SDMMC_MODULE);
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_cd=SDMMC_SLOT_NO_CD;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
      getAppState()->sdCard = (item_state_t)(item_state_t::ACTIVE|item_state_t::ERROR);
      if (ret == ESP_FAIL)
      {
        ESP_LOGE(__FUNCTION__, "Failed to mount filesystem. "
                              "If you want the card to be formatted, set format_if_mount_failed = true.");
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "Failed to initialize the card (%s). "
                              "Make sure SD card lines have pull-up resistors in place.",
                esp_err_to_name(ret));
      }
      return false;
    }

    getAppState()->sdCard = item_state_t::ACTIVE;
    ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
    if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
      sdmmc_card_print_info(stdout, card);

    f_mkdir("/converted");
    f_mkdir("/kml");
    f_mkdir("/sent");
  }
  numSdCallers++;
  return true;
}

bool deinitSPISDCard(){
  numSdCallers--;
  ESP_LOGD(__FUNCTION__, "SD callers %d", numSdCallers);
  if (numSdCallers == 0) {
    if (esp_vfs_fat_sdmmc_unmount() == ESP_OK)
    {
      getAppState()->sdCard = item_state_t::ACTIVE;
      ESP_LOGD(__FUNCTION__, "Unmounted SD Card");
    } else {
      getAppState()->sdCard = item_state_t::ERROR;
      ESP_LOGE(__FUNCTION__, "Failed to unmount SD Card");
      return false;
    }
    getAppState()->sdCard = (item_state_t)(item_state_t::ACTIVE|item_state_t::PAUSED);

    return spi_bus_free((spi_host_device_t)getSDHost()->slot) == ESP_OK;
  } else {
    ESP_LOGD(__FUNCTION__,"Postponing SD card umount");
    return ESP_OK;
  }
}

bool initSPISDCard()
{
  if (numSdCallers == 0) {
    ESP_LOGD(__FUNCTION__, "Using SPI peripheral");
    app_config_t* cfg = getAppConfig();
    esp_err_t ret=0;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = cfg->sdcard_config.MosiPin,
        .miso_io_num = cfg->sdcard_config.MisoPin,
        .sclk_io_num = cfg->sdcard_config.ClkPin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
        .flags = 0,
        .intr_flags = 0
    };


    sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
      device_config.gpio_cs = cfg->sdcard_config.Cspin;
      device_config.host_id = (spi_host_device_t)host.slot;


    if ((ret=spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, 1)) == ESP_OK) {
      if ((ret=esp_vfs_fat_sdspi_mount(mount_point, &host, &device_config, &mount_config, &card)) != ESP_OK)
      {
        spi_bus_free((spi_host_device_t)host.slot);
        getAppState()->sdCard = (item_state_t)(item_state_t::ACTIVE|item_state_t::ERROR);
        if (ret == ESP_FAIL)
        {
          ESP_LOGE(__FUNCTION__, "Failed to mount filesystem. "
                                "If you want the card to be formatted, set format_if_mount_failed = true.");
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Failed to initialize the card (%s). "
                                "Make sure SD card lines have pull-up resistors in place.",
                  esp_err_to_name(ret));
        }
        return false;
      }
      getAppState()->sdCard = (item_state_t)(item_state_t::ACTIVE);
      ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
      if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
        sdmmc_card_print_info(stdout, card);

      f_mkdir("/firmware");
      f_mkdir("/converted");
      f_mkdir("/kml");
      f_mkdir("/logs");
      f_mkdir("/sent");
    } else if (ret == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(__FUNCTION__,"Error initing SPI bus %s",esp_err_to_name(ret));
    } else {
      ESP_LOGE(__FUNCTION__,"Error initing SPI bus %s",esp_err_to_name(ret));
      return false;
    }
  }

  numSdCallers++;
  ESP_LOGD(__FUNCTION__, "SD callers %d", numSdCallers);
  if (numSdCallers==1){
      dumpLogs();
  }
  return true;
}

bool moveFile(char* src, char* dest){
  FRESULT res;
  FILE* srcF = fopen(src,"r");
  if (srcF != 0) {
    FILE* destF = fopen(dest,"w",true);
    if (destF != NULL) {
      int ch=0;
      while ((ch=fgetc(srcF))!=EOF){
        fputc(ch,destF);
      }
      fclose(destF);
      fclose(srcF);
      if ((res=f_unlink(&src[8]))==0){
        ESP_LOGD(__FUNCTION__,"moved %s to %s",src,dest);
        return true;
      } else {
        ESP_LOGE(__FUNCTION__,"failed in deleting %s %s",src,getErrorMsg(res));
      }
    } else {
      ESP_LOGE(__FUNCTION__,"Cannot open dest %s",dest);
    }
  } else {
    ESP_LOGE(__FUNCTION__,"Cannot open source %s",src);
  }
  return false;
}

bool stringContains(const char* str,const char* val) {
  if ((str == NULL) || (val == NULL))
    return false;

  uint32_t sl = strlen(str);
  uint32_t vl = strlen(val);
  uint32_t startPos = sl-vl;

  if ((vl == 0) || (sl == 0)) {
    return false;
  }

  if (vl>sl)
    return false;

  for (uint32_t idx = startPos; idx < vl; idx++) {
    if (str[idx] != val[idx-startPos])
      return false;
  }
  return true;
}

bool endsWith(const char* str,const char* val) {
  if ((str == NULL) || (val == NULL))
    return false;

  uint32_t sl = strlen(str);
  uint32_t vl = strlen(val);
  uint32_t startPos = sl-vl;

  if (vl>sl)
    return false;

  for (uint32_t idx = startPos; idx < sl; idx++) {
    if (str[idx] != val[idx-startPos])
      return false;
  }
  return true;
}

char* indexOf(const char* str, const char* key){
  if ((str == NULL) || (key == NULL) || (strlen(str)==0) || (strlen(key) == 0)){
    return NULL;
  }
  uint32_t slen = strlen(str);
  uint32_t klen = strlen(key);
  uint32_t kidx = 0;
  char* keyPos=NULL;

  for (uint32_t idx=0; idx < slen; idx++) {
    if (str[idx] == key[kidx]) {
      if (keyPos == NULL) {
        keyPos=(char*)&str[idx];
      }
      if (kidx == (klen-1)){
        return keyPos;
      }
      kidx++;
    } else {
      kidx=0;
      keyPos=NULL;
    }
  }
  return NULL;
}

FILE * fopen (const char * _name, const char * _type,bool createDir){
  if (createDir) {
    ESP_LOGV(__FUNCTION__,"Validating %s folder",_name);
    FF_DIR theFolder;
    char* folderName = (char*)malloc(530);
    strcpy(folderName,_name+7);
    char* closingMark = strrchr(folderName,'/');
    FRESULT res;
    if (closingMark>folderName){
      uint32_t folderNameLen = closingMark - folderName;
      memset(closingMark,0,2);
      ESP_LOGV(__FUNCTION__,"Folder is %s",folderName);
      while (closingMark > folderName) {
        if (f_opendir(&theFolder, folderName) != FR_OK) {
          ESP_LOGV(__FUNCTION__,"Folder %s does not exist",folderName);
          closingMark = strrchr(folderName,'/');
          if (closingMark==NULL) {
            break;
          }
          *closingMark=0;
        } else {
          f_closedir(&theFolder);
          if (strlen(folderName) == folderNameLen){
            break;
          }
          ESP_LOGV(__FUNCTION__,"Folder %s is the starting point.",folderName);
          *(closingMark)='/';
          do {
            if ((res=f_mkdir(folderName)) != FR_OK) {
              if (res == 8) {
                ESP_LOGW(__FUNCTION__,"Got an error creating %s - %s",folderName,getErrorMsg(res));
              } else {
                ESP_LOGE(__FUNCTION__,"Failed in creating %s - %s",folderName,getErrorMsg(res));
                break;
              }
            }
            ESP_LOGD(__FUNCTION__,"Created %s",folderName);
            closingMark = folderName+strlen(folderName);
            *closingMark = '/';
            if (closingMark == NULL) {
              break;
            }
          } while (*(closingMark+1) != 0);
          break;
        }
      }
    }
    free(folderName);
  }
  return ::fopen(_name,_type);
}
