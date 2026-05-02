#include <optional>
#include <stdexcept>

#include "esp_http_server.h"
#include "http_parser.h"
#include "wifi_access_point_guard.hpp"
#include "wifi_station_guard.hpp"
#include "http_request_data_reader.hpp"
#include "http_request_data_writer.hpp"
#include "pb_message_reader.hpp"
#include "pb_message_writer.hpp"

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

struct SetWifiContext {
    std::optional<mcu_server::WifiStationGuard>* sta_guard;
    mcu_server::NvsFlashGuard nvs_guard;
};

static esp_err_t config_wifi_cb(httpd_req_t *request) {
    auto ctx = static_cast<SetWifiContext *>(request->user_ctx);
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    nanoipc::HttpRequestDataReader data_reader(request);
    nanoipc::HttpRequestDataWriter data_writer(request);
    nanoipc::PbMessageReader<std::string> pb_reader(&data_reader, nullptr);
    nanoipc::PbMessageWriter<std::string> pb_writer(&data_writer, nullptr);
    const auto pb_msg_opt = pb_reader.read();
    if (!pb_msg_opt.has_value()) {
        return ESP_ERR_INVALID_ARG;
    }
    // const auto ssid = pb_msg_opt.value()["ssid"].asString();
    // const auto password = pb_msg_opt.value()["password"].asString();
    // ctx->sta_guard->emplace(ssid, password, ctx->nvs_guard);

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
        std::optional<mcu_server::WifiStationGuard> sta_guard;
        SetWifiContext set_wifi_ctx {
            .sta_guard = &sta_guard,
            .nvs_guard = mcu_server::NvsFlashGuard(nvs_guard)
        };

        const auto get_handler = httpd_uri_t {
            .uri      = "/config/wifi",
            .method   = HTTP_POST,
            .handler  = config_wifi_cb,
            .user_ctx = &set_wifi_ctx
        };
        const auto server = start_webserver(get_handler);
        while (true) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}