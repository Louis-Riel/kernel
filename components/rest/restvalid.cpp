#include "./route.h"
#include "sdkconfig.h"
#include "pwdmgr.h"

esp_err_t TheRest::checkTheSum(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
#if CONFIG_ENABLE_VALIDATED_REQUESTS == 1
    if (req->user_ctx != nullptr) {
        ret = ((PasswordManager*)req->user_ctx)->CheckTheSum(req);
    } else {
        ESP_LOGW(__FUNCTION__,"No handler defined");
    }
#endif
    return ret;
}
