#include <stdexcept>

#include "esp_http_server.h"
#include "esp_wifi_types_generic.h"
#include "wifi_access_point_guard.hpp"

static httpd_handle_t start_webserver(const httpd_uri_t& get_handler) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        throw std::runtime_error("Failed to start HTTP server");
    }
    if (httpd_register_uri_handler(server, &get_handler) != ESP_OK) {
        httpd_stop(server);
        throw std::runtime_error("Failed to register URI handler");
    }
    return server;
}

static esp_err_t get_handler_cb(httpd_req_t *request) {
    if (const auto res = httpd_resp_set_status(request, HTTPD_200); res != ESP_OK) {
        return res;
    }
    if (const auto res = httpd_resp_set_type(request, HTTPD_TYPE_OCTET); res != ESP_OK) {
        return res;
    }
    const std::string report_body = "hey there";
    return httpd_resp_send(request, report_body.data(), report_body.size());
}

static wifi_ap_config_t generate_ap_cfg(const std::string& ssid, const std::string& password) {
    auto config = wifi_ap_config_t {
        .ssid = {'\0'},
        .password = {'\0'},
        .ssid_len = 0,
        .channel = 0, // TODO: check it
        .authmode = WIFI_AUTH_WPA2_PSK,
        .ssid_hidden = 0,
        .max_connection = 1,
        .beacon_interval = 100,
        .csa_count = 3,
        .dtim_period = 1,
        .pairwise_cipher = WIFI_CIPHER_TYPE_NONE,
        .ftm_responder = false,
        .pmf_cfg = wifi_pmf_config_t {
            .capable = false,
            .required = false
        },
        .sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED,
        .transition_disable = 0,
        .sae_ext = 0,
        .bss_max_idle_cfg = wifi_bss_max_idle_config_t {
            .period = 0,
            .protected_keep_alive = false
        },
        .gtk_rekey_interval = 0
    };
    if (ssid.size() > sizeof(config.ssid)) {
        throw std::invalid_argument("received ssid is too long");
    }
    if (ssid.size() != ssid.copy((char *)(config.ssid), ssid.size())) {
        throw std::runtime_error("failed to create AP config: SSID copy failure");
    }
    config.ssid_len = ssid.size();
    if (password.size() > sizeof(config.password)) {
        throw std::invalid_argument("received password is too long");
    }
    if (password.size() != password.copy((char *)(config.password), password.size())) {
        throw std::runtime_error("failed to create AP config: Password copy failure");
    }
    return config;
}

extern "C" {
    void app_main(void) {
        const std::string& ssid = "ESP32-AP";
        const std::string& password = "EspPassWord";
        mcu_server::WifiAccessPointGuard ap_guard(ssid, password);

        const auto get_handler = httpd_uri_t {
            .uri      = "/some/data",
            .method   = HTTP_GET,
            .handler  = get_handler_cb,
            .user_ctx = NULL
        };
        const auto server = start_webserver(get_handler);
        while (true) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}