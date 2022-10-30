#include "./route.h"
#include "sdkconfig.h"

esp_err_t TheRest::checkTheSum(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
#if CONFIG_ENABLE_VALIDATED_REQUESTS == 1
    size_t hlen = httpd_req_get_hdr_value_len(req, "The-Hash");
    if (hlen > 1) {
        auto* theHash = (char*)dmalloc(65);
        if (!theHash) {
            ESP_LOGE(__FUNCTION__,"Can't allocate hash");
            return ESP_FAIL;
        }
        if (httpd_req_get_hdr_value_str(req, "The-Hash", theHash, 65) == ESP_OK) {
            //ESP_LOGI(__FUNCTION__,"Hash:%s(%s)",req->uri, theHash);
            // mbedtls_sha256_context ctx;
            // mbedtls_sha256_init(&ctx);
            // mbedtls_sha256_update(&ctx, reinterpret_cast<const uint8_t*>("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"), 64);
            // uint8_t digest[32];
            // mbedtls_sha256_finish(&ctx, digest);
            // mbedtls_sha256_free(&ctx);
            // unsigned long t1 = micros();

            // for (int i = 0; i < sizeof(digest); ++i) {
            //     Serial.printf("%02x", digest[i]);
            // }
        } else {
            //ret=ESP_FAIL;
            ESP_LOGE(__FUNCTION__,"%s got bad hash:%d",req->uri, hlen);
            //httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"You are not worthy with this sillyness");
        }
        ldfree(theHash);
    } else {
        //ret=ESP_FAIL;
        //ESP_LOGW(__FUNCTION__,"%s got no hash",req->uri);
        //httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"You are not worthy with this sillyness");
    }
#endif
    return ret;
}
