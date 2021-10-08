#include "utils.h"
#include "esp_sleep.h"
#include "driver/periph_ctrl.h"
#include "../components/esp_littlefs/include/esp_littlefs.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"
#include "logs.h"
#include "errno.h"
#include "freertos/semphr.h"
#include "esp32/rom/md5_hash.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "eventmgr.h"
#include "/home/riell/esp-4.3/esp-idf/components/pthread/include/esp_pthread.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192

const char* logPath = "/lfs/logs";

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card = NULL;
const char mount_point[] = "/sdcard";
int8_t numSdCallers = -1;
wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static SemaphoreHandle_t storageSema = xSemaphoreCreateMutex();

static uint32_t numOpenFiles = 0;

sdmmc_host_t *getSDHost()
{
  return &host;
}

bool startsWith(const char *str, const char *key)
{
  uint32_t sle = strlen(str);
  uint32_t kle = strlen(key);

  if ((sle == 0) || (kle == 0) || (sle < kle))
  {
    ESP_LOGV(__FUNCTION__, "%s in %s rejected because of bad len", key, str);
    return false;
  }

  for (int idx = 0; idx < kle; idx++)
  {
    if (str[idx] != key[idx])
    {
      ESP_LOGV(__FUNCTION__, "%s in %s rejected at idx %d", key, str, idx);
      return false;
    }
  }
  return true;
}

const char *getErrorMsg(int32_t errCode)
{
  switch (errCode)
  {
  case FR_OK:
    return "(0) Succeeded";
    break;
  case FR_DISK_ERR:
    return "(1) A hard error occurred in the low level disk I/O layer";
    break;
  case FR_INT_ERR:
    return "(2) Assertion failed";
    break;
  case FR_NOT_READY:
    return "(3) The physical drive cannot work";
    break;
  case FR_NO_FILE:
    return "(4) Could not find the file";
    break;
  case FR_NO_PATH:
    return "(5) Could not find the path";
    break;
  case FR_INVALID_NAME:
    return "(6) The path name format is invalid";
    break;
  case FR_DENIED:
    return "(7) Access denied due to prohibited access or directory full";
    break;
  case FR_EXIST:
    return "(8) Access denied due to prohibited access";
    break;
  case FR_INVALID_OBJECT:
    return "(9) The file/directory object is invalid";
    break;
  case FR_WRITE_PROTECTED:
    return "(10) The physical drive is write protected";
    break;
  case FR_INVALID_DRIVE:
    return "(11) The logical drive number is invalid";
    break;
  case FR_NOT_ENABLED:
    return "(12) The volume has no work area";
    break;
  case FR_NO_FILESYSTEM:
    return "(13) There is no valid FAT volume";
    break;
  case FR_MKFS_ABORTED:
    return "(14) The f_mkfs() aborted due to any problem";
    break;
  case FR_TIMEOUT:
    return "(15) Could not get a grant to access the volume within defined period";
    break;
  case FR_LOCKED:
    return "(16) The operation is rejected according to the file sharing policy";
    break;
  case FR_NOT_ENOUGH_CORE:
    return "(17) LFN working buffer could not be allocated";
    break;
  case FR_TOO_MANY_OPEN_FILES:
    return "(18) Number of open files > FF_FS_LOCK";
    break;
  case FR_INVALID_PARAMETER:
    return "(19) Given parameter is invalid";
    break;
  default:
    break;
  }
  return "Invalid error code";
}

bool initSDMMCSDCard()
{
  if (numSdCallers == 0)
  {
    ESP_LOGD(__FUNCTION__, "Using SDMMC peripheral");

    //sdmmc_host_init();

    //periph_module_reset(PERIPH_SDMMC_MODULE);
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    //sdmmc_slot_config_t slot_config = {
    //    .gpio_cd = SDMMC_SLOT_NO_CD,
    //    .gpio_wp = SDMMC_SLOT_NO_WP,
    //    .width = 1,
    //    .flags = SDMMC_HOST_FLAG_1BIT,
    //};
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    //gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    //gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1-line modes
    //gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    //gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
    //gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1-line modes

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
      AppConfig::GetAppStatus()->SetStateProperty("/sdcard/state", (item_state_t)(item_state_t::ACTIVE | item_state_t::ERROR));
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

    AppConfig::GetAppStatus()->SetStateProperty("/sdcard/state", item_state_t::ACTIVE);
    ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card->max_freq_khz);
    if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
      sdmmc_card_print_info(stdout, card);

    f_mkdir("/converted");
    if (f_mkdir("/logs") != FR_OK)
      ESP_LOGD(__FUNCTION__, "Cannot create logs folder");
    f_mkdir("/kml");
    f_mkdir("/sent");
  }
  numSdCallers++;
  return true;
}

bool deinitSPISDCard(bool log)
{
  xSemaphoreTake(storageSema,portMAX_DELAY);
  if (numSdCallers >= 0)
    numSdCallers--;
  if (log)
    ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
  if (numSdCallers == 0)
  {
    esp_err_t ret = ESP_OK;
    AppConfig *appState = AppConfig::GetAppStatus();
    EventGroupHandle_t app_eg = getAppEG();
    if (xEventGroupGetBits(app_eg) & SPIFF_MOUNTED)
    {
      ret = esp_vfs_littlefs_unregister("storage");
      if (ret != ESP_OK)
      {
        if (log)
          ESP_LOGE(__FUNCTION__, "Failed in registering littlefs %s", esp_err_to_name(ret));
      } else {
          ESP_LOGD(__FUNCTION__, "lfs unmounted");
      }
      appState->SetStateProperty("/lfs/state", item_state_t::INACTIVE);
      xEventGroupClearBits(app_eg, SPIFF_MOUNTED);
    }
    if (xEventGroupGetBits(app_eg) & SDCARD_MOUNTED)
    {
      ESP_LOGV(__FUNCTION__, "Using SPI peripheral");
      if (esp_vfs_fat_sdmmc_unmount() == ESP_OK)
      {
        if (log)
          ESP_LOGD(__FUNCTION__, "Unmounted SD Card");
      }
      else
      {
        appState->SetStateProperty("/sdcard/state", item_state_t::ERROR);
        if (log)
          ESP_LOGE(__FUNCTION__, "Failed to unmount SD Card");
        xSemaphoreGive(storageSema);
        return false;
      }
      spi_bus_free(SPI2_HOST);
      xEventGroupClearBits(app_eg, SDCARD_MOUNTED);
      appState->SetStateProperty("/sdcard/state", item_state_t::INACTIVE);
    }
  }
  else
  {
    if (log)
      ESP_LOGV(__FUNCTION__, "Postponing SD card umount");
  }
  xSemaphoreGive(storageSema);
  return true;
}

bool deinitSPISDCard()
{
  return deinitSPISDCard(true);
}

struct dirFiles_t {
  const dirFiles_t* root;
  char* curDir;
  uint32_t* bytesFree;
};

void CleanupLFS(void* param) {
  dirFiles_t* files = (dirFiles_t*)param;
  ESP_LOGD(__FUNCTION__,"Looking for files to cleanup in %s, free:%d",files->curDir,*files->bytesFree);

  DIR *theFolder;
  FILE *theFile;
  struct dirent *fi;
  struct stat fileStat;
  char* cpath = (char*)dmalloc(300);

  if ((theFolder = openDir(files->curDir)) != NULL) {
    while (((fi = readDir(theFolder)) != NULL) && (*files->bytesFree < 1782579)) {
      if (fi->d_type == DT_DIR) {
        dirFiles_t* cfiles = (dirFiles_t *)dmalloc(sizeof(dirFiles_t));
        cfiles->curDir = (char*)dmalloc(300);
        sprintf(cfiles->curDir,"%s/%s",files->curDir,fi->d_name);
        cfiles->root = files->root;
        cfiles->bytesFree = files->bytesFree;
        CleanupLFS(cfiles);
      } else {
        sprintf(cpath,"%s/%s",files->curDir,fi->d_name);
        stat(cpath,&fileStat);
        if (unlink(cpath) == 0) {
          *files->bytesFree+=fileStat.st_size;
          ESP_LOGD(__FUNCTION__,"Deleted %s(%ld/%d)",cpath,fileStat.st_size,*files->bytesFree);
        } else {
          ESP_LOGE(__FUNCTION__,"Failed to delete %s(%ld)",cpath,fileStat.st_size);
        }  
      }
    }
    closeDir(theFolder);
  }

  if (files->root == files) {
    ldfree(files->bytesFree);
  }
  ldfree(cpath);
  ldfree((void*)files->curDir);
  ldfree(files);
}

esp_err_t setupLittlefs()
{
  const esp_vfs_littlefs_conf_t conf = {
      .base_path = "/lfs",
      .partition_label = "storage",
      .format_if_mount_failed = true,
      .dont_mount = false};

  esp_err_t ret = esp_vfs_littlefs_register(&conf);
  AppConfig *appState = AppConfig::GetAppStatus();
  ESP_LOGV(__FUNCTION__, "lfs mounted %d", ret);
  AppConfig *spiffState = appState->GetConfig("/lfs");
  if (ret != ESP_OK)
  {
    spiffState->SetStateProperty("state", item_state_t::ERROR);
    ESP_LOGE(__FUNCTION__, "Failed in registering littlefs %s", esp_err_to_name(ret));
    return ret;
  }

  size_t total_bytes;
  size_t used_bytes;
  if ((ret = esp_littlefs_info("storage", &total_bytes, &used_bytes)) != ESP_OK)
  {
    ESP_LOGE(__FUNCTION__, "Failed in getting info %s", esp_err_to_name(ret));
    return ret;
  }
  spiffState->SetStateProperty("state", item_state_t::ACTIVE);
  spiffState->SetIntProperty("total", total_bytes);
  spiffState->SetIntProperty("used", used_bytes);
  spiffState->SetIntProperty("free", total_bytes - used_bytes);
  if (total_bytes - used_bytes < 1782579) {
    dirFiles_t* dirFiles = (dirFiles_t*)dmalloc(sizeof(dirFiles_t));
    dirFiles->curDir = (char*)dmalloc(300);
    sprintf(dirFiles->curDir,"%s",logPath);
    dirFiles->root = dirFiles;
    dirFiles->bytesFree=(uint32_t*)dmalloc(sizeof(void*));
    *dirFiles->bytesFree = AppConfig::GetAppStatus()->GetIntProperty("/lfs/free");
    CleanupLFS(dirFiles);
  }
  free(spiffState);

  ESP_LOGV(__FUNCTION__, "Space: %d/%d", used_bytes, total_bytes);
  struct dirent *de;
  bool hasCsv = false;
  bool hasLogs = false;
  bool hasFw = false;
  bool hasCfg = false;
  bool hasStat = false;

  ESP_LOGV(__FUNCTION__, "Spiff is spiffy");
  EventGroupHandle_t app_eg = getAppEG();
  xEventGroupSetBits(app_eg, SPIFF_MOUNTED);
  numSdCallers = -1;

  DIR *root = openDir("/lfs");
  if (root == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot open lfs");
    return ESP_FAIL;
  }

  while ((de = readDir(root)) != NULL)
  {
    ESP_LOGV(__FUNCTION__, "%d %s", de->d_type, de->d_name);
    if (strcmp(de->d_name, "csv") == 0)
    {
      hasCsv = true;
    }
    if (strcmp(de->d_name, "logs") == 0)
    {
      hasLogs = true;
    }
    if (strcmp(de->d_name, "firmware") == 0)
    {
      hasFw = true;
    }
    if (strcmp(de->d_name, "config") == 0)
    {
      hasCfg = true;
    }
    if (strcmp(de->d_name, "status") == 0)
    {
      hasStat = true;
    }
  }
  if ((ret = closeDir(root)) != ESP_OK)
  {
    ESP_LOGE(__FUNCTION__, "failed to close root %s", esp_err_to_name(ret));
    return ret;
  }

  if (!hasCsv)
  {
    if (mkdir("/lfs/csv", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating csv folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "csv folder created");
  }
  if (!hasLogs)
  {
    if (mkdir("/lfs/logs", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating logs folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "logs folder created");
  }
  if (!hasFw)
  {
    if (mkdir("/lfs/firmware", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating firmware folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "firmware folder created");
  }

  if (!hasCfg)
  {
    if (mkdir("/lfs/config", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating config folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "config folder created");
  }

  if (!hasStat)
  {
    if (mkdir("/lfs/status", 0750) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed in creating status folder");
      return ESP_FAIL;
    }
    ESP_LOGD(__FUNCTION__, "status folder created");
  }

  return ESP_OK;
}

bool initSPISDCard(bool log)
{
  xSemaphoreTake(storageSema,portMAX_DELAY);
  ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
  if (numSdCallers <= 0)
  {
    AppConfig *appState = AppConfig::GetAppStatus();
    AppConfig *spiffState = appState->GetConfig("lfs");
    AppConfig *sdcState = appState->GetConfig("sdcard");
    EventGroupHandle_t app_eg = getAppEG();
    esp_err_t ret = ESP_FAIL;
    if (numSdCallers >= 0)
    {
      const esp_vfs_littlefs_conf_t conf = {
          .base_path = "/lfs",
          .partition_label = "storage",
          .format_if_mount_failed = true,
          .dont_mount = false};

      if (log)
        ESP_LOGV(__FUNCTION__, "Using SPI peripheral");

      if (!(xEventGroupGetBits(app_eg) & SPIFF_MOUNTED))
      {
        ret = esp_vfs_littlefs_register(&conf);
        xEventGroupSetBits(app_eg, SPIFF_MOUNTED);
        if (ret != ESP_OK)
        {
          if (log)
            ESP_LOGE(__FUNCTION__, "Failed in registering littlefs %s", esp_err_to_name(ret));
          numSdCallers = 0;
          xSemaphoreGive(storageSema);
          return ret;
        }
        if (log)
          ESP_LOGV(__FUNCTION__, "lfs mounted %d", ret);
      }
      else
      {
        ESP_LOGW(__FUNCTION__, "Cannot mount spiff, already mounted");
        numSdCallers = 1;
      }
      spiffState->SetStateProperty("state", item_state_t::ACTIVE);
    }
    numSdCallers=1;

    AppConfig *cfg = AppConfig::GetAppConfig()->GetConfig("/sdcard");
    if (cfg->HasProperty("MosiPin") &&
        cfg->HasProperty("MisoPin") &&
        cfg->HasProperty("ClkPin") &&
        cfg->HasProperty("Cspin"))
    {
      spi_bus_config_t bus_cfg = {
          .mosi_io_num = cfg->GetIntProperty("MosiPin"),
          .miso_io_num = cfg->GetIntProperty("MisoPin"),
          .sclk_io_num = cfg->GetIntProperty("ClkPin"),
          .quadwp_io_num = -1,
          .quadhd_io_num = -1,
          .max_transfer_sz = 4000,
          .flags = 0,
          .intr_flags = 0};

      sdspi_device_config_t slot_config = {
          .host_id = SPI2_HOST,
          .gpio_cs = cfg->GetPinNoProperty("Cspin"),
          .gpio_cd = SDSPI_SLOT_NO_CD,
          .gpio_wp = SDSPI_SLOT_NO_WP,
          .gpio_int = GPIO_NUM_NC};

      ESP_LOGV(__FUNCTION__, "sd state:%d", AppConfig::GetAppStatus()->GetStateProperty("/sdcard/state"));

      if (bus_cfg.miso_io_num &&
          bus_cfg.mosi_io_num &&
          bus_cfg.sclk_io_num &&
          (!(xEventGroupGetBits(app_eg) & SDCARD_ERROR)))
      {
        if ((ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, 1)) == ESP_OK)
        {
          if ((ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card)) != ESP_OK)
          {
            if (ret == ESP_FAIL)
            {
              if (log)
                ESP_LOGE(__FUNCTION__, "Failed to mount filesystem. "
                                       "If you want the card to be formatted, set format_if_mount_failed = true.");
            }
            else
            {
              if (log)
                ESP_LOGE(__FUNCTION__, "Failed to initialize the card (%s). "
                                       "Make sure SD card lines have pull-up resistors in place.",
                         esp_err_to_name(ret));
            }
            sdcState->SetStateProperty("state", item_state_t::ERROR);
            xEventGroupSetBits(app_eg, SDCARD_ERROR);
            spi_bus_free(SPI2_HOST);
            free(spiffState);
            free(sdcState);
            free(cfg);
            xSemaphoreGive(storageSema);
            return true;
          }

          if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
            sdmmc_card_print_info(stdout, card);
          if (log)
            ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
          sdcState->SetStateProperty("state", item_state_t::ACTIVE);
          xEventGroupSetBits(app_eg, SDCARD_MOUNTED);
        }
        else if (ret == ESP_ERR_INVALID_STATE)
        {
          xEventGroupClearBits(app_eg, SDCARD_MOUNTED);
          sdcState->SetStateProperty("state", item_state_t::ERROR);
          if (log)
            ESP_LOGW(__FUNCTION__, "Error initing SPI bus %s", esp_err_to_name(ret));
        }
        else
        {
          sdcState->SetStateProperty("state", item_state_t::ERROR);
          xEventGroupSetBits(app_eg, SDCARD_MOUNTED);
          if (log)
            ESP_LOGE(__FUNCTION__, "Error initing SPI bus %s", esp_err_to_name(ret));
          free(spiffState);
          free(sdcState);
          free(cfg);
          xSemaphoreGive(storageSema);
          return true;
        }
      }
    }
    else
    {
      ESP_LOGW(__FUNCTION__, "Cannot mount SD, Missingparams");
    }
    free(cfg);
    item_state_t state = spiffState->GetStateProperty("state");
    if (state == item_state_t::ACTIVE)
    {
      size_t total_bytes;
      size_t used_bytes;
      esp_err_t ret;
      if ((ret = esp_littlefs_info("storage", &total_bytes, &used_bytes)) == ESP_OK)
      {
        spiffState->SetIntProperty("total", total_bytes);
        spiffState->SetIntProperty("used", used_bytes);
        spiffState->SetIntProperty("free", total_bytes - used_bytes);
        if (total_bytes - used_bytes < 1782579) {
          dirFiles_t* dirFiles = (dirFiles_t*)dmalloc(sizeof(dirFiles_t));
          dirFiles->curDir = (char*)dmalloc(300);
          sprintf(dirFiles->curDir,"%s",logPath);
          dirFiles->root = dirFiles;
          dirFiles->bytesFree=(uint32_t*)dmalloc(sizeof(void*));
          *dirFiles->bytesFree = AppConfig::GetAppStatus()->GetIntProperty("/lfs/free");
          CreateBackgroundTask(CleanupLFS,"CleanupLFS",4096,(void*)dirFiles,tskIDLE_PRIORITY,NULL);
        }
      }
    }
    state = sdcState->GetStateProperty("state");
    if (state == item_state_t::ACTIVE)
    {
      FATFS *fs;
      DWORD fre_clust, fre_sect, tot_sect;

      esp_err_t res = f_getfree("0:", &fre_clust, &fs);
      if (res == ESP_OK)
      {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        sdcState->SetIntProperty("total", tot_sect / 2);
        sdcState->SetIntProperty("used", (tot_sect - fre_sect) / 2);
        sdcState->SetIntProperty("free", fre_sect / 2);
      }
    }
    free(spiffState);
    free(sdcState);
  }
  else
  {
    numSdCallers++;
    if (log)
      ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
  }
  xSemaphoreGive(storageSema);
  return true;
}

bool initSPISDCard()
{
  return initSPISDCard(true);
}

bool deleteFile(const char *fileName)
{
  ESP_LOGD(__FUNCTION__, "Deleting file %s", fileName);
  return unlink(fileName) == 0;
}

bool rmDashFR(const char *folderName)
{
  DIR *theFolder;
  struct dirent *fi;
  char fileName[300];
  bool isABadDay = false;

  if ((theFolder = openDir(folderName)) != NULL)
  {
    while (!isABadDay && ((fi = readDir(theFolder)) != NULL))
    {
      sprintf(fileName, "%s/%s", folderName, fi->d_name);
      if (fi->d_type == DT_DIR)
      {
        isABadDay |= !rmDashFR(fileName);
      }
      else
      {
        isABadDay |= !deleteFile(fileName);
      }
    }
    closeDir(theFolder);
    if (!isABadDay)
    {
      ESP_LOGD(__FUNCTION__, "Deleting folder %s", folderName);
      return rmdir(folderName) == 0;
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Cannot delete %s", folderName);
    }
  }
  return false;
}

bool moveFile(const char *src, const char *dest)
{
  int res;
  FILE *srcF = fOpen(src, "r");
  if (srcF != 0)
  {
    FILE *destF = fOpenCd(dest, "w", true);
    if (destF != NULL)
    {
      int ch = 0;
      while ((ch = fgetc(srcF)) != EOF)
      {
        fputc(ch, destF);
      }
      fClose(destF);
      fClose(srcF);
      if ((res = unlink(src)) == 0)
      {
        ESP_LOGD(__FUNCTION__, "moved %s to %s", src, dest);
        return true;
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "failed in deleting %s %s", src, getErrorMsg(res));
      }
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Cannot open dest %s", dest);
    }
  }
  else
  {
    ESP_LOGE(__FUNCTION__, "Cannot open source %s", src);
  }
  return false;
}

bool stringContains(const char *str, const char *val)
{
  if ((str == NULL) || (val == NULL))
    return false;

  uint32_t sl = strlen(str);
  uint32_t vl = strlen(val);
  uint32_t startPos = sl - vl;

  if ((vl == 0) || (sl == 0))
  {
    return false;
  }

  if (vl > sl)
    return false;

  for (uint32_t idx = startPos; idx < vl; idx++)
  {
    if (str[idx] != val[idx - startPos])
      return false;
  }
  return true;
}

bool endsWith(const char *str, const char *val)
{
  if ((str == NULL) || (val == NULL))
    return false;

  uint32_t sl = strlen(str);
  uint32_t vl = strlen(val);
  uint32_t startPos = sl - vl;

  if (vl > sl)
    return false;

  for (uint32_t idx = startPos; idx < sl; idx++)
  {
    if (str[idx] != val[idx - startPos])
      return false;
  }
  return true;
}

char *indexOf(const char *str, const char *key)
{
  if ((str == NULL) || (key == NULL) || (strlen(str) == 0) || (strlen(key) == 0))
  {
    return NULL;
  }
  uint32_t slen = strlen(str);
  uint32_t klen = strlen(key);
  uint32_t kidx = 0;
  char *keyPos = NULL;

  for (uint32_t idx = 0; idx < slen; idx++)
  {
    if (str[idx] == key[kidx])
    {
      if (keyPos == NULL)
      {
        keyPos = (char *)&str[idx];
      }
      if (kidx == (klen - 1))
      {
        return keyPos;
      }
      kidx++;
    }
    else
    {
      kidx = 0;
      keyPos = NULL;
    }
  }
  return NULL;
}

char *lastIndexOf(const char *str, const char *key)
{
  if ((str == NULL) || (key == NULL) || (strlen(str) == 0) || (strlen(key) == 0))
  {
    ESP_LOGV(__FUNCTION__, "Missing source or key");
    return NULL;
  }
  uint32_t slen = strlen(str);
  ESP_LOGV(__FUNCTION__, "Looking for %s in %s(%d)", key, str, slen);

  for (int32_t idx = slen - 1; idx >= 0; idx--)
  {
    if ((str[idx] == key[0]) && (indexOf(&str[idx], key) == &str[idx]))
    {
      ESP_LOGV(__FUNCTION__, "found %s in %s(%d) at %d", key, str, slen, idx);
      return (char *)&str[idx];
    }
  }
  ESP_LOGV(__FUNCTION__, "%s not in %s(%d)", key, str, slen);
  return NULL;
}

uint32_t GetNumOpenFiles()
{
  return numOpenFiles;
}

void flashTheThing(uint8_t *img, uint32_t totLen)
{
  esp_ota_handle_t update_handle = 0;
  const esp_partition_t *update_partition = NULL;

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running)
  {
    ESP_LOGW(__FUNCTION__, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
             configured->address, running->address);
    ESP_LOGW(__FUNCTION__, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }
  ESP_LOGD(__FUNCTION__, "Running partition type %d subtype %d (offset 0x%08x)",
           running->type, running->subtype, running->address);

  update_partition = esp_ota_get_next_update_partition(update_partition);

  bool isOnOta = false;
  if (update_partition->address == configured->address)
  {
    isOnOta = true;
    assert(update_partition != NULL);
    ESP_LOGD(__FUNCTION__, "Skipping partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
  }
  assert(update_partition != NULL);
  ESP_LOGD(__FUNCTION__, "Writing to partition subtype %d at offset 0x%x",
           update_partition->subtype, update_partition->address);

  if (initSPISDCard())
  {
    if (isOnOta)
    {
      ESP_LOGW(__FUNCTION__, "Wring partition to update from");
    }
    else
    {
      esp_err_t err = esp_ota_begin(update_partition, totLen, &update_handle);
      if (err == ESP_OK)
      {
        ESP_LOGD(__FUNCTION__, "esp_ota_begin succeeded %d ", totLen);
        err = esp_ota_write(update_handle, (const void *)img, totLen);
        if (err == ESP_OK)
        {
          ESP_LOGI(__FUNCTION__, "esp_ota_write succeeded");
          err = esp_ota_end(update_handle);
          if (err == ESP_OK)
          {
            err = esp_ota_set_boot_partition(update_partition);
            if (err == ESP_OK)
            {
              ESP_LOGI(__FUNCTION__, "esp_ota_set_boot_partition succeeded");
              moveFile("/lfs/firmware/tobe.bin.md5", "/lfs/firmware/current.bin.md5");
              if (strcmp(AppConfig::GetAppConfig()->GetStringProperty("clienttype"),"Tracker") == 0){
                deleteFile("/lfs/firmware/current.bin");
              }
              dumpTheLogs((void*)true);
              deinitSPISDCard();
              esp_system_abort("Flashing");
            }
            else
            {
              ESP_LOGE(__FUNCTION__, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
            }
          }
          else
          {
            ESP_LOGE(__FUNCTION__, "esp_ota_write failed");
          }
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "esp_ota_write failed");
        }
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "esp_ota_begin failed (%s)", esp_err_to_name(err));
      }
    }
  }
  else
  {
    ESP_LOGE(__FUNCTION__, "Failed in opeing /firmware/current.bin.md5");
  }
}

void UpgradeFirmware()
{
  initSPISDCard();
  struct stat md5St, fwSt;
  char md5fName[] = "/lfs/firmware/tobe.bin.md5";
  char fwfName[] = "/lfs/firmware/current.bin";
  size_t md5len = 0;
  if ((stat(md5fName, &md5St) == 0) && (stat(fwfName, &fwSt) == 0))
  {
    ESP_LOGD(__FUNCTION__, "We have a pending upgrade to process");
    char srvrmd5[33], localmd5[33];
    memset(srvrmd5,0,33);
    memset(localmd5,0,33);
    FILE *fmd5 = fOpen(md5fName, "r");
    FILE *ffw = NULL;
    size_t chunckLen = 0, fwlen = 0;
    if (fmd5)
    {
      if (((md5len = fRead(srvrmd5, sizeof(uint8_t), 33, fmd5)) >= 32) && (md5len <=33))
      {
        ESP_LOGD(__FUNCTION__, "FW MD5 %d bits read", md5len);
        if ((ffw = fOpen(fwfName, "r")) != NULL)
        {
          ESP_LOGD(__FUNCTION__, "FW %d bits to read", (int)fwSt.st_size);
          if (fwSt.st_size)
          {
            uint8_t *fwbits = (uint8_t *)dmalloc(fwSt.st_size);

            MD5Context md5_context;
            MD5Init(&md5_context);
            while ((chunckLen = fRead(fwbits + fwlen, sizeof(uint8_t), fwSt.st_size - fwlen, ffw)) > 0)
            {
              MD5Update(&md5_context, fwbits + fwlen, chunckLen);
              fwlen += chunckLen;
              ESP_LOGD(__FUNCTION__, "FW %d bits read", fwlen);
            }
            uint8_t fmd[16];
            MD5Final(fmd, &md5_context);

            for (uint8_t i = 0; i < 16; ++i)
            {
              sprintf(&localmd5[i * 2], "%02x", (unsigned int)fmd[i]);
            }

            fClose(fmd5);
            fClose(ffw);
            if (strcmp(localmd5, srvrmd5) == 0)
            {
              flashTheThing(fwbits, fwlen);
            }
            else
            {
              ESP_LOGE(__FUNCTION__, "MD5 Missmatch, no bueno %s!=%s", localmd5, srvrmd5);
            }
            ldfree(fwbits);
          }
          else
          {
            ESP_LOGE(__FUNCTION__, "Empty firmware file");
            unlink(fwfName);
            unlink(md5fName);
          }
        }
        else
        {
          ESP_LOGE(__FUNCTION__, "Cannot open firmware bin");
        }
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "Bad md5 len:%d.", md5len);
      }
      fClose(fmd5);
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Cannot open the md5");
    }
  }
  else
  {
    AppConfig* stat = AppConfig::GetAppStatus();
    ESP_LOGD(__FUNCTION__, "No FW to update, running %s built on %s", stat->GetStringProperty("/build/ver"), stat->GetStringProperty("/build/date"));
  }
  deinitSPISDCard();
}

void DisplayMemInfo(){
  return;
	ESP_LOGD(__FUNCTION__,"heap_caps_get_free_size: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
	ESP_LOGD(__FUNCTION__,"heap_caps_get_minimum_free_size: %d", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
	ESP_LOGD(__FUNCTION__,"heap_caps_get_largest_free_block: %d", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
	ESP_LOGD(__FUNCTION__,"heap total size: %zu, heap free head %d, can use minimum is %d\n",heap_caps_get_total_size(MALLOC_CAP_DEFAULT), esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
  volatile UBaseType_t numTasks = uxTaskGetNumberOfTasks();
  uint32_t totalRunTime;
  TaskStatus_t *statuses = (TaskStatus_t *)dmalloc(numTasks * sizeof(TaskStatus_t));
  numTasks = uxTaskGetSystemState(statuses, numTasks, &totalRunTime);
  char* tname = pcTaskGetTaskName(NULL);

  if (totalRunTime > 0)
  {
      for (uint32_t taskNo = 0; taskNo < numTasks; taskNo++)
      {
        if (strcmp(tname,statuses[taskNo].pcTaskName)==0){
          ESP_LOGD(__FUNCTION__, "TaskNumber:%d,Name:%s,Prio:%d,Runtime:%d,Core:%d,State:%d, StackFree:%d,Pct:%f", 
                statuses[taskNo].xTaskNumber,
                statuses[taskNo].pcTaskName,
                statuses[taskNo].uxCurrentPriority,
                statuses[taskNo].ulRunTimeCounter,
                statuses[taskNo].xCoreID > 100 ? -1 : statuses[taskNo].xCoreID,
                statuses[taskNo].eCurrentState,
                statuses[taskNo].usStackHighWaterMark * 4,
                ((double)statuses[taskNo].ulRunTimeCounter / totalRunTime) * 100.0);
          break;
        }
      }
  }
  ldfree(statuses);
}