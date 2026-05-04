#include <array>
#include <cstring>
#include <optional>
#include <stdexcept>

#include "esp_http_server.h"
#include "driver/gpio.h"

#include "http_parser.h"
#include "pb.h"
#include "service.pb.h"
#include "wifi_iic_request_reader.hpp"
#include "wifi_iic_response_writer.hpp"

#include "wifi_station_guard.hpp"

#include "http_server.hpp"

#ifndef NETWORK_SSID
#  error "NETWORK_SSID is not defined. Please pass it to the cmake command"
#endif

#ifndef NETWORK_PASSWORD
#  error "NETWORK_PASSWORD is not defined. Please pass it to the cmake command"
#endif

#ifndef SERVICE_PORT
#  error "SERVICE_PORT is not defined. Please pass it to the cmake command"
#endif

static httpd_handle_t start_webserver(const httpd_uri_t& read_handler, const httpd_uri_t& write_handler);
static esp_err_t write_data_cb(httpd_req_t *request);
static esp_err_t read_data_cb(httpd_req_t *request);
static void blink_loop();

extern "C" {
    void app_main(void) {
        try {
            mcu_server::NvsFlashGuard nvs_guard;
            mcu_server::WifiStationGuard network_guard(NETWORK_SSID, NETWORK_PASSWORD, nvs_guard);
            const auto read_handler = httpd_uri_t {
                .uri      = "/iic",
                .method   = HTTP_GET,
                .handler  = read_data_cb,
                .user_ctx = nullptr
            };            
            const auto write_handler = httpd_uri_t {
                .uri      = "/iic",
                .method   = HTTP_POST,
                .handler  = write_data_cb,
                .user_ctx = nullptr
            };
            const auto server = start_webserver(read_handler, write_handler);
            while (true) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        } catch (...) {
            blink_loop();
        }
    }
}

inline esp_err_t write_data_cb(httpd_req_t *request) {
    nanoipc::WifiI2CRequestReader request_reader(request);
    nanoipc::WifiI2CResponseWriter response_writer(request);
    
    const auto api_request = request_reader.read();
    if (!api_request.has_value()) {
        service_api_WifiI2CRelayResponse resp {
            .which_response = service_api_WifiI2CRelayResponse_undefined_response_tag,
            .response = {
                .undefined_response = service_api_Result::service_api_Result_BAD_REQUEST
            }
        };
        response_writer.write(resp);
        return ESP_OK;
    }
    service_api_WifiI2CRelayResponse resp = service_api_WifiI2CRelayResponse_init_zero;
    switch (api_request->which_request) {
    case service_api_WifiI2CRelayRequest_write_request_tag:
        resp.which_response = service_api_WifiI2CRelayResponse_write_response_tag;
        resp.response.write_response.result = service_api_Result::service_api_Result_FAILURE;
        break;
    default:
        resp.which_response = service_api_WifiI2CRelayResponse_undefined_response_tag;
        resp.response.undefined_response = service_api_Result::service_api_Result_BAD_REQUEST;
        break;
    }
    response_writer.write(resp);
    return ESP_OK;
}

inline esp_err_t read_data_cb(httpd_req_t *request) {
    nanoipc::WifiI2CRequestReader request_reader(request);
    nanoipc::WifiI2CResponseWriter response_writer(request);
    
    const auto api_request = request_reader.read();
    if (!api_request.has_value()) {
        service_api_WifiI2CRelayResponse resp {
            .which_response = service_api_WifiI2CRelayResponse_undefined_response_tag,
            .response = {
                .undefined_response = service_api_Result::service_api_Result_BAD_REQUEST
            }
        };
        response_writer.write(resp);
        return ESP_OK;
    }
    service_api_WifiI2CRelayResponse resp = service_api_WifiI2CRelayResponse_init_zero;
    const std::array<pb_byte_t, 2> response_data = {0x55, 0xAA};
    switch (api_request->which_request) {
    case service_api_WifiI2CRelayRequest_read_request_tag:
        resp.which_response = service_api_WifiI2CRelayResponse_read_response_tag;
        resp.response.read_response.result = service_api_Result::service_api_Result_FAILURE;
        resp.response.read_response.data.size = response_data.size();
        std::memcpy(resp.response.read_response.data.bytes, response_data.data(), response_data.size());
        break;
    default:
        resp.which_response = service_api_WifiI2CRelayResponse_undefined_response_tag;
        resp.response.undefined_response = service_api_Result::service_api_Result_BAD_REQUEST;
        break;
    }
    response_writer.write(resp);
    return ESP_OK;
}

inline void blink_loop() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_15);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    int led_on_time = 100;
    int led_off_time = 100;
    while (true) {
        gpio_set_level(GPIO_NUM_15, true);
        vTaskDelay(pdMS_TO_TICKS(led_on_time));
        gpio_set_level(GPIO_NUM_15, false);
        vTaskDelay(pdMS_TO_TICKS(led_off_time));
    }
}