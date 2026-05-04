#include <optional>

#include "esp_http_server.h"
#include "driver/gpio.h"

#include "http_parser.h"
#include "http_request_data_reader.hpp"
#include "http_response_data_writer.hpp"
#include "pb_message_reader.hpp"
#include "pb_message_writer.hpp"
#include "service.pb.h"

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

static esp_err_t write_data_cb(httpd_req_t *request);
static esp_err_t read_data_cb(httpd_req_t *request);
static void blink_loop();

extern "C" {
    void app_main(void) {
        try {
            mcu_server::NvsFlashGuard nvs_guard;
            mcu_server::WifiStationGuard network_guard(NETWORK_SSID, NETWORK_PASSWORD, nvs_guard);
            nanoipc::HttpServer server(SERVICE_PORT, true);
            server.register_handler("/iic", HTTP_GET, read_data_cb);
            server.register_handler("/iic", HTTP_POST, write_data_cb);
            while (true) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        } catch (...) {
            blink_loop();
        }
    }
}

inline esp_err_t write_data_cb(httpd_req_t *request) {
    nanoipc::HttpRequestDataReader request_data_reader(request);
    nanoipc::PbMessageReader<service_api_WifiI2CRelayWriteRequest> request_reader(&request_data_reader, service_api_WifiI2CRelayReadRequest_fields);
    nanoipc::HttpResponseDataWriter response_data_writer(request);
    nanoipc::PbMessageWriter<service_api_WifiI2CRelayWriteResponse> response_writer(&response_data_writer, service_api_WifiI2CRelayWriteResponse_fields);
    const auto api_request = request_reader.read();
    if (!api_request.has_value()) {
        service_api_WifiI2CRelayWriteResponse resp {
            .result = service_api_Result::service_api_Result_BAD_REQUEST
        };
        response_writer.write(resp);
        return ESP_OK;
    }
    service_api_WifiI2CRelayWriteResponse resp = {
        .result = service_api_Result::service_api_Result_FAILURE
    };
    response_writer.write(resp);
    return ESP_OK;
}

inline esp_err_t read_data_cb(httpd_req_t *request) {
    nanoipc::HttpRequestDataReader request_data_reader(request);
    nanoipc::PbMessageReader<service_api_WifiI2CRelayReadRequest> request_reader(&request_data_reader, service_api_WifiI2CRelayReadRequest_fields);
    nanoipc::HttpResponseDataWriter response_data_writer(request);
    nanoipc::PbMessageWriter<service_api_WifiI2CRelayReadResponse> response_writer(&response_data_writer, service_api_WifiI2CRelayReadResponse_fields);
    const auto api_request = request_reader.read();
    if (!api_request.has_value()) {
        service_api_WifiI2CRelayReadResponse resp {
            .result = service_api_Result::service_api_Result_BAD_REQUEST,
            .data = {
                .size = 0
            }
        };
        response_writer.write(resp);
        return ESP_OK;
    }
    service_api_WifiI2CRelayReadResponse resp = {
        .result = service_api_Result::service_api_Result_FAILURE,
        .data = {
            .size = 0
        }
    };
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