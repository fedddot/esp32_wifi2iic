#include <stdexcept>

#include "esp_http_server.h"
#include "wifi_access_point_guard.hpp"
#include "wifi_station_guard.hpp"

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

extern "C" {
    void app_main(void) {
        const std::string& ssid = "ESP32-AP";
        const std::string& password = "EspPassWord";
        mcu_server::NvsFlashGuard nvs_guard;
        mcu_server::WifiAccessPointGuard ap_guard(ssid, password, nvs_guard);

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