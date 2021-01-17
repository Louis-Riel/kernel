#include "utils.h"
#include "esp_sleep.h"
#include "driver/periph_ctrl.h"
#include "esp_log.h"
#include "../components/esp_littlefs/include/esp_littlefs.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "logs.h"
#include "cJSON.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card = NULL;
const char mount_point[] = "/sdcard";
int8_t numSdCallers=0;

sdmmc_host_t* getSDHost(){
  return &host;
}

bool startsWith(const char* str,const char* key){
    uint32_t sle =strlen(str);
    uint32_t kle =strlen(key);

    if ((sle == 0) || (kle==0) || (sle<kle)) {
        ESP_LOGV(__FUNCTION__,"%s in %s rejected because of bad len",key,str);
        return false;
    }

    for (int idx=0; idx < kle; idx++){
        if (str[idx] != key[idx]){
            ESP_LOGV(__FUNCTION__,"%s in %s rejected at idx %d",key,str,idx);
            return false;
        }
    }
    return true;
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
    if (f_mkdir("/logs") != FR_OK) ESP_LOGD(__FUNCTION__,"Cannot create logs folder");
    f_mkdir("/kml");
    f_mkdir("/sent");
  }
  numSdCallers++;
  return true;
}

bool deinitSPISDCard(){
  if (numSdCallers >= 0)
    numSdCallers--;
  ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
  if (numSdCallers == 0) {
    esp_err_t ret=esp_vfs_littlefs_unregister("storage");
    if (ret != ESP_OK) {
      ESP_LOGE(__FUNCTION__,"Failed in registering littlefs %s", esp_err_to_name(ret));
    }
    ESP_LOGV(__FUNCTION__, "Using SPI peripheral");    
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

    return true;
  } else {
    ESP_LOGV(__FUNCTION__,"Postponing SD card umount");
    return ESP_OK;
  }
}

bool initSPISDCard()
{
  if (numSdCallers <= 0) {
    esp_err_t ret;
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = "/lfs",
        .partition_label = "storage",
        .format_if_mount_failed = true
    };
    
    ESP_LOGV(__FUNCTION__, "Using SPI peripheral");
    app_config_t* cfg = getAppConfig();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = cfg->sdcard_config.MosiPin.value,
        .miso_io_num = cfg->sdcard_config.MisoPin.value,
        .sclk_io_num = cfg->sdcard_config.ClkPin.value,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
        .flags = 0,
        .intr_flags = 0
    };

    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = cfg->sdcard_config.MisoPin.value;
    slot_config.gpio_mosi = cfg->sdcard_config.MosiPin.value;
    slot_config.gpio_sck  = cfg->sdcard_config.ClkPin.value;
    slot_config.gpio_cs   = cfg->sdcard_config.Cspin.value;

    //if ((ret=spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, 1)) == ESP_OK) {
      if ((ret=esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card)) != ESP_OK)
      {
        //spi_bus_free((spi_host_device_t)host.slot);
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
        esp_vfs_fat_sdmmc_unmount();
        return false;
      }
      getAppState()->sdCard = (item_state_t)(item_state_t::ACTIVE);
      ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
      if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
        sdmmc_card_print_info(stdout, card);

      //f_mkdir("/firmware");
      //f_mkdir("/converted");
      //f_mkdir("/kml");
      f_mkdir("/logs");
      //f_mkdir("/sent");
      ret=esp_vfs_littlefs_register(&conf);
      if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__,"Failed in registering littlefs %s", esp_err_to_name(ret));
        return ret;
      }
    //} else if (ret == ESP_ERR_INVALID_STATE) {
    //  ESP_LOGW(__FUNCTION__,"Error initing SPI bus %s",esp_err_to_name(ret));
    //} else {
    //  ESP_LOGE(__FUNCTION__,"Error initing SPI bus %s",esp_err_to_name(ret));
    //  return false;
    //}
    numSdCallers=1;
  } else {
    numSdCallers++;
    ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
    if (numSdCallers==1){
        dumpLogs();
    }
  }

  return true;
}

bool moveFile(char* src, char* dest){
  int res;
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
      if ((res=unlink(src))==0){
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

static uint8_t* img = NULL;
static uint32_t ilen=0; 

uint8_t* loadImage(bool reset,uint32_t* iLen) {
  if ((reset || (img == NULL)) && initSPISDCard()){
    const char* fwf = "/lfs/firmware/current.bin";
    FILE* fw;
    if ((fw = fopen(fwf,"r",true)) != NULL) {
      img = (uint8_t*)heap_caps_malloc(1500000,MALLOC_CAP_SPIRAM);
      uint8_t* buf = (uint8_t*)malloc(F_BUF_SIZE);
      uint32_t len=0;
      *iLen = 0;//fread((void*)img,1,1500000,fw);
      while (!feof(fw)){
          if ((len=fread(buf,1,F_BUF_SIZE,fw))>0) {
              memcpy(img+*iLen,buf,len);
              *iLen+= len;
              ESP_LOGV(__FUNCTION__,"firmware readin %d bytes",*iLen);
          }
      }
      ilen=*iLen;
      ESP_LOGV(__FUNCTION__,"firmware read %d bytes",ilen);
      deinitSPISDCard();
    }
  }
  *iLen=ilen;
  return img;
}

FILE * fopen (const char * _name, const char * _type,bool createDir){
  if (!_name || (strlen(_name)==0)) {
    ESP_LOGE(__FUNCTION__,"No file name given");
    return NULL;
  }

  ESP_LOGV(__FUNCTION__,"Opening %s's folder with perm %s create folder %d",_name,_type,createDir);
  if (createDir) {
    ESP_LOGV(__FUNCTION__,"Validating %s's folder",_name);
    DIR* theFolder;
    char* folderName = (char*)malloc(530);
    strcpy(folderName,_name);
    char* closingMark = strrchr(folderName,'/');
    int res;
    if (closingMark>folderName){
      *closingMark=0;
      ESP_LOGV(__FUNCTION__,"Folder is %s",folderName);
      uint32_t flen=strlen(folderName);
      if ((theFolder = opendir(folderName)) == NULL) {
        ESP_LOGV(__FUNCTION__,"Folder %s does not exist",folderName);
        closingMark=strchr(folderName+1,'/');
        *closingMark=0;
        while (strlen(folderName) <= flen){
          ESP_LOGV(__FUNCTION__,"Checking Folder %s",folderName);
          if ((res=mkdir(folderName,0755)) != 0) {
            if (res != EEXIST) {
              char buf[255];
              ESP_LOGV(__FUNCTION__,"Folder %s can not be created,hopefully because it exists %s",folderName,esp_err_to_name_r(res,buf,255));
            } else {
              ESP_LOGD(__FUNCTION__,"Folder %s created",folderName);
            }
          }
          if (closingMark){
            *(closingMark)='/';
            closingMark = strchr(closingMark+1,'/');
            if (closingMark) {
              *closingMark=0;
            }
          }
        }
      } else {
        ESP_LOGV(__FUNCTION__,"Folder %s does exist",folderName);
        closedir(theFolder);
      }
      closingMark = strrchr(folderName,'/');
    }
    free(folderName);
  }
  return ::fopen(_name,_type);
}
