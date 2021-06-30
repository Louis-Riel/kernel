#include "utils.h"
#include "esp_sleep.h"
#include "driver/periph_ctrl.h"
#include "esp_log.h"
#include "../components/esp_littlefs/include/esp_littlefs.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"
#include "logs.h"
#include "cJSON.h"
#include "errno.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card = NULL;
const char mount_point[] = "/sdcard";
int8_t numSdCallers = 0;
wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

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

static const char *getErrorMsg(int32_t errCode)
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

    AppConfig::GetAppStatus()->SetStateProperty("/sdcard/state",item_state_t::ACTIVE);
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
  if (numSdCallers >= 0)
    numSdCallers--;
  if (log)
    ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
  if (numSdCallers == 0)
  {
    esp_err_t ret;
    AppConfig* appState = AppConfig::GetAppStatus();
    if (AppConfig::GetAppStatus()->GetStateProperty("/spiff/state") & item_state_t::ACTIVE)
    {
      ret = esp_vfs_littlefs_unregister("storage");
      if (ret != ESP_OK)
      {
        if (log)
          ESP_LOGE(__FUNCTION__, "Failed in registering littlefs %s", esp_err_to_name(ret));
      }
      AppConfig::GetAppStatus()->SetStateProperty("/spiff/state",item_state_t::INACTIVE);
    }
    if (AppConfig::GetAppStatus()->GetStateProperty("/sdcard/state") & item_state_t::ACTIVE)
    {
      ESP_LOGV(__FUNCTION__, "Using SPI peripheral");
      if (esp_vfs_fat_sdmmc_unmount() == ESP_OK)
      {
        if (log)
          ESP_LOGD(__FUNCTION__, "Unmounted SD Card");
      }
      else
      {
        AppConfig::GetAppStatus()->SetStateProperty("/sdcard/state", item_state_t::ERROR);
        if (log)
          ESP_LOGE(__FUNCTION__, "Failed to unmount SD Card");
        return false;
      }
      spi_bus_free(SPI2_HOST);
      AppConfig::GetAppStatus()->SetStateProperty("/sdcard/state", item_state_t::INACTIVE);
    }
    return true;
  }
  else
  {
    if (log)
      ESP_LOGV(__FUNCTION__, "Postponing SD card umount");
    return ESP_OK;
  }
}

bool deinitSPISDCard()
{
  return deinitSPISDCard(true);
}

bool initSPISDCard(bool log)
{
  AppConfig* appState = AppConfig::GetAppStatus();
  AppConfig* spiffState = appState->GetConfig("spiff");
  AppConfig* sdcState = appState->GetConfig("sdcard");
  bool sdon=false;
  if (numSdCallers <= 0)
  {
    numSdCallers = 1;
    log=true;
    esp_err_t ret;
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = "/lfs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false};

    if (log)
      ESP_LOGV(__FUNCTION__, "Using SPI peripheral");

    if (spiffState->GetStateProperty("state") & item_state_t::INACTIVE)
    {
      ret = esp_vfs_littlefs_register(&conf);
      if (log)
        ESP_LOGD(__FUNCTION__, "lfs mounted %d", ret);
      if (ret != ESP_OK)
      {
        if (log)
          ESP_LOGE(__FUNCTION__, "Failed in registering littlefs %s", esp_err_to_name(ret));
        numSdCallers=0;
        free(spiffState);
        free(sdcState);
        return ret;
      }
    }
    else
    {
      ESP_LOGW(__FUNCTION__, "Cannot mount spiff, already mounted");
      numSdCallers=1;
    }
    spiffState->SetStateProperty("state",item_state_t::ACTIVE);

    AppConfig *cfg = AppConfig::GetAppConfig()->GetConfig("/sdcard");
    if (cfg->HasProperty("MosiPin") &&
        cfg->HasProperty("MisoPin") &&
        cfg->HasProperty("ClkPin") &&
        cfg->HasProperty("Cspin")) {
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


      if (bus_cfg.miso_io_num && bus_cfg.mosi_io_num && bus_cfg.sclk_io_num)
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
            sdcState->SetStateProperty("state",item_state_t::ERROR);
            spi_bus_free(SPI2_HOST);
            free(spiffState);
            free(sdcState);
            free(cfg);
            return false;
          }

          if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
            sdmmc_card_print_info(stdout, card);
          if (log)
            ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
          sdcState->SetStateProperty("state",item_state_t::ACTIVE);
        }
        else if (ret == ESP_ERR_INVALID_STATE)
        {
          sdcState->SetStateProperty("state",item_state_t::ERROR);
          if (log)
            ESP_LOGW(__FUNCTION__, "Error initing SPI bus %s", esp_err_to_name(ret));
        }
        else
        {
          sdcState->SetStateProperty("state",item_state_t::ERROR);
          if (log)
            ESP_LOGE(__FUNCTION__, "Error initing SPI bus %s", esp_err_to_name(ret));
          free(spiffState);
          free(sdcState);
          free(cfg);
          return false;
        }
      }
      else
      {
        if (log)
        {
          ESP_LOGW(__FUNCTION__,"No SD Card bus_cfg.miso_io_num:%d bus_cfg.mosi_io_num:%d bus_cfg.sclk_io_num:%d",bus_cfg.miso_io_num, bus_cfg.mosi_io_num, bus_cfg.sclk_io_num);
        }
      }
    } else {
      ESP_LOGW(__FUNCTION__, "Cannot mount SD, Missingparams");
    }
    free(cfg);
  }
  else
  {
    numSdCallers++;
    if (log)
      ESP_LOGV(__FUNCTION__, "SD callers %d", numSdCallers);
    if (numSdCallers == 1)
    {
      dumpLogs();
    }
  }
  item_state_t state = spiffState->GetStateProperty("state");
  if (state == item_state_t::ACTIVE) {
    size_t total_bytes;
    size_t used_bytes;
    esp_err_t ret;
    if ((ret = esp_littlefs_info("storage", &total_bytes, &used_bytes)) == ESP_OK)
    {
      spiffState->SetIntProperty("total",total_bytes);
      spiffState->SetIntProperty("used",used_bytes);
      spiffState->SetIntProperty("free",total_bytes-used_bytes);
    }
  }
  state = sdcState->GetStateProperty("state");
  if (sdon) {
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    esp_err_t res = f_getfree("0:", &fre_clust, &fs);
    if (res == ESP_OK) {
      tot_sect = (fs->n_fatent - 2) * fs->csize;
      fre_sect = fre_clust * fs->csize;
      sdcState->SetIntProperty("total",tot_sect / 2);
      sdcState->SetIntProperty("used",(tot_sect-fre_sect) / 2);
      sdcState->SetIntProperty("free",fre_sect / 2);
    }
  }
  free(spiffState);
  free(sdcState);

  return true;
}

bool initSPISDCard()
{
  return initSPISDCard(true);
}

bool moveFile(char *src, char *dest)
{
  int res;
  FILE *srcF = fOpen(src, "r");
  if (srcF != 0)
  {
    FILE *destF = fopen(dest, "w", true);
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

static uint8_t *img = NULL;
static uint32_t ilen = 0;

uint8_t *loadImage(bool reset, uint32_t *iLen)
{
  if ((reset || (img == NULL)) && initSPISDCard())
  {
    const char *fwf = "/lfs/firmware/current.bin";
    ESP_LOGV(__FUNCTION__, "Reading %s", fwf);
    FILE *fw;
    if ((fw = fopen(fwf, "r", true)) != NULL)
    {
      ESP_LOGV(__FUNCTION__, "Opened %s", fwf);
      img = (uint8_t *)heap_caps_malloc(1500000, MALLOC_CAP_SPIRAM);
      ESP_LOGV(__FUNCTION__, "Allocated %d SPI RAM", 1500000);
      uint8_t *buf = (uint8_t *)dmalloc(F_BUF_SIZE);
      ESP_LOGV(__FUNCTION__, "Allocated %d buffer", F_BUF_SIZE);
      uint32_t len = 0;
      *iLen = 0; //fread((void*)img,1,1500000,fw);
      while (!feof(fw))
      {
        if ((len = fread(buf, 1, F_BUF_SIZE, fw)) > 0)
        {
          memcpy(img + *iLen, buf, len);
          *iLen += len;
          ESP_LOGV(__FUNCTION__, "firmware readin %d bytes", *iLen);
        }
      }
      ilen = *iLen;
      ESP_LOGV(__FUNCTION__, "firmware read %d bytes", ilen);
      deinitSPISDCard();
    }
    else
    {
      ESP_LOGW(__FUNCTION__, "Cannot read %s", fwf);
    }
  }
  *iLen = ilen;
  return img;
}

FILE *fopen(const char *_name, const char *_type, bool createDir)
{
  return fopen(_name, _type, createDir, LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE);
}

FILE *fopen(const char *_name, const char *_type, bool createDir, bool log)
{
  if (!_name || (strlen(_name) == 0))
  {
    if (log)
      ESP_LOGE(__FUNCTION__, "No file name given");
    return NULL;
  }
  char buf[255];

  if (log)
    ESP_LOGV(__FUNCTION__, "Opening %s's folder with perm %s create folder %d", _name, _type, createDir);
  if (createDir)
  {
    if (log)
      ESP_LOGV(__FUNCTION__, "Validating %s's folder", _name);
    DIR *theFolder;
    char *folderName = (char *)dmalloc(530);
    strcpy(folderName, _name);
    char *closingMark = strrchr(folderName, '/');
    int res;
    if (closingMark > folderName)
    {
      *closingMark = 0;
      if (log)
        ESP_LOGV(__FUNCTION__, "Folder is %s", folderName);
      uint32_t flen = strlen(folderName);
      if ((theFolder = opendir(folderName)) == NULL)
      {
        if (log)
          ESP_LOGV(__FUNCTION__, "Folder %s does not exist", folderName);
        closingMark = strchr(folderName + 1, '/');
        *closingMark = 0;
        while (strlen(folderName) <= flen)
        {
          if (log)
            ESP_LOGV(__FUNCTION__, "Checking Folder %s", folderName);
          if ((res = mkdir(folderName, 0755)) != 0)
          {
            if (res != EEXIST)
            {
              if (log)
                ESP_LOGV(__FUNCTION__, "Folder %s can not be created,hopefully because it exists %s", folderName, esp_err_to_name_r(res, buf, 255));
            }
            else
            {
              if (log)
                ESP_LOGD(__FUNCTION__, "Folder %s created", folderName);
            }
          }
          if (closingMark == NULL)
          {
            break;
          }
          if (closingMark)
          {
            *(closingMark) = '/';
            closingMark = strchr(closingMark + 1, '/');
            if (closingMark)
            {
              *closingMark = 0;
            }
          }
        }
      }
      else
      {
        if (log)
          ESP_LOGV(__FUNCTION__, "Folder %s does exist", folderName);
        closedir(theFolder);
      }
      closingMark = strrchr(folderName, '/');
    }
    ldfree(folderName);
  }
  return fOpen(_name, _type);
}

FILE *fOpen(const char *_name, const char *_type)
{
  FILE *ret = ::fopen(_name, _type);
  if (ret != NULL)
  {
    AppConfig::SignalStateChange(state_change_t::MAIN);
    numOpenFiles++;
  }
  else
  {
    ESP_LOGE(__FUNCTION__, "Error in fopen for %s:%s", _name, esp_err_to_name(errno));
  }
  return ret;
}

int fClose(FILE *f)
{
  int ret = EOF;
  if (f != NULL)
  {
    ret = ::fclose(f);
    if (ret == 0)
    {
      AppConfig::SignalStateChange(state_change_t::MAIN);
      numOpenFiles--;
    }
  }
  return ret;
}

uint32_t GetNumOpenFiles()
{
  return numOpenFiles;
}
