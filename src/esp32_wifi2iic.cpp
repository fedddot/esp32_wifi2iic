#include <cstdint>
#include <cstring>
#include <optional>

#include "esp_err.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

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

#ifndef I2C_SDA_GPIO
#  error "I2C_SDA_GPIO is not defined. Please pass it to the cmake command"
#endif

#ifndef I2C_SCL_GPIO
#  error "I2C_SCL_GPIO is not defined. Please pass it to the cmake command"
#endif

#ifndef I2C_SCL_SPEED_HZ
#  define I2C_SCL_SPEED_HZ 400000
#endif

static i2c_master_bus_handle_t s_i2c_bus_handle = nullptr;

static esp_err_t write_data_cb(httpd_req_t *request);
static esp_err_t read_data_cb(httpd_req_t *request);
static esp_err_t init_iic();
static esp_err_t deinit_iic();
static esp_err_t read_iic_data(const uint8_t address, uint8_t *dst, size_t nbytes, const uint16_t timeout_ms);
static esp_err_t write_iic_data(const uint8_t address, const uint8_t *src, size_t nbytes, const uint16_t timeout_ms);
static esp_err_t write_read_iic_data(const uint8_t address, const uint8_t *write_src, size_t write_nbytes, uint8_t *read_dst, size_t read_nbytes, const uint16_t timeout_ms);

static void blink_loop();

extern "C" {
    void app_main(void) {
        try {
            mcu_server::NvsFlashGuard nvs_guard;
            mcu_server::WifiStationGuard network_guard(NETWORK_SSID, NETWORK_PASSWORD, nvs_guard);

            if (init_iic() != ESP_OK) {
                throw std::runtime_error("Failed to initialize I2C bus");
            }

            nanoipc::HttpServer server(SERVICE_PORT, true);
            server.register_handler("/iic", HTTP_GET, read_data_cb);
            server.register_handler("/iic", HTTP_POST, write_data_cb);
            while (true) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        } catch (...) {
            blink_loop();
        }
        deinit_iic();
    }
}

inline esp_err_t write_data_cb(httpd_req_t *request) {
    nanoipc::HttpRequestDataReader request_data_reader(request);
    nanoipc::PbMessageReader<service_api_WifiI2CRelayWriteRequest> request_reader(&request_data_reader, service_api_WifiI2CRelayWriteRequest_fields);
    nanoipc::HttpResponseDataWriter response_data_writer(request);
    nanoipc::PbMessageWriter<service_api_WifiI2CRelayResponse> response_writer(&response_data_writer, service_api_WifiI2CRelayResponse_fields);
    const auto api_request = request_reader.read();
    if (!api_request.has_value()) {
        service_api_WifiI2CRelayResponse resp {
            .result = service_api_Result::service_api_Result_BAD_REQUEST
        };
        response_writer.write(resp);
        return ESP_OK;
    }
    const auto write_result = write_iic_data(
        (uint8_t)(0xFF & api_request->address),
        api_request->write_data.bytes,
        api_request->write_data.size,
        api_request->timeout_ms
    );
    service_api_WifiI2CRelayResponse resp = service_api_WifiI2CRelayResponse_init_zero;
    switch (write_result) {
    case ESP_OK:
        resp.result = service_api_Result::service_api_Result_SUCCESS;
        break;
    case ESP_ERR_TIMEOUT:
        resp.result = service_api_Result::service_api_Result_TIMEOUT;
        break;
    default:
        resp.result = service_api_Result::service_api_Result_FAILURE;
    }
    response_writer.write(resp);
    return ESP_OK;
}

inline esp_err_t read_data_cb(httpd_req_t *request) {
    nanoipc::HttpRequestDataReader request_data_reader(request);
    nanoipc::PbMessageReader<service_api_WifiI2CRelayReadRequest> request_reader(&request_data_reader, service_api_WifiI2CRelayReadRequest_fields);
    nanoipc::HttpResponseDataWriter response_data_writer(request);
    nanoipc::PbMessageWriter<service_api_WifiI2CRelayResponse> response_writer(&response_data_writer, service_api_WifiI2CRelayResponse_fields);

    service_api_WifiI2CRelayResponse resp = service_api_WifiI2CRelayResponse_init_zero;
    const auto api_request = request_reader.read();
    if (!api_request.has_value()) {
        resp.result = service_api_Result::service_api_Result_BAD_REQUEST;
        resp.data.size = 0;
        response_writer.write(resp);
        return ESP_OK;
    }
    
    if (api_request->read_size > sizeof(resp.data.bytes)) {
        resp.result = service_api_Result::service_api_Result_BAD_REQUEST;
        resp.data.size = 0;
        response_writer.write(resp);
        return ESP_OK;
    }

    esp_err_t iic_result;
    if (api_request->write_data.size == 0) {
        // Plain read: no register/command prefix required.
        iic_result = read_iic_data(
            (uint8_t)(0xFF & api_request->address),
            resp.data.bytes,
            api_request->read_size,
            api_request->timeout_ms
        );
    } else {
        // Write-then-read: transmit register/command bytes, then read back.
        iic_result = write_read_iic_data(
            (uint8_t)(0xFF & api_request->address),
            api_request->write_data.bytes,
            api_request->write_data.size,
            resp.data.bytes,
            api_request->read_size,
            api_request->timeout_ms
        );
    }
    switch (iic_result) {
    case ESP_OK:
        resp.result = service_api_Result::service_api_Result_SUCCESS;
        resp.data.size = api_request->read_size;
        break;
    case ESP_ERR_TIMEOUT:
        resp.result = service_api_Result::service_api_Result_TIMEOUT;
        resp.data.size = 0;
        break;
    default:
        resp.result = service_api_Result::service_api_Result_FAILURE;
        resp.data.size = 0;
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

static esp_err_t init_iic() {
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = (gpio_num_t)I2C_SDA_GPIO;
    bus_config.scl_io_num = (gpio_num_t)I2C_SCL_GPIO;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&bus_config, &s_i2c_bus_handle);
}

static esp_err_t deinit_iic() {
    if (s_i2c_bus_handle == nullptr) {
        return ESP_OK;
    }
    const auto result = i2c_del_master_bus(s_i2c_bus_handle);
    s_i2c_bus_handle = nullptr;
    return result;
}

static esp_err_t read_iic_data(const uint8_t address, uint8_t *dst, size_t nbytes, const uint16_t timeout_ms) {
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = address;
    dev_config.scl_speed_hz = I2C_SCL_SPEED_HZ;
    i2c_master_dev_handle_t dev_handle;
    auto result = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_config, &dev_handle);
    if (result != ESP_OK) {
        return result;
    }
    result = i2c_master_receive(dev_handle, dst, nbytes, (int)timeout_ms);
    i2c_master_bus_rm_device(dev_handle);
    return result;
}

static esp_err_t write_iic_data(const uint8_t address, const uint8_t *src, size_t nbytes, const uint16_t timeout_ms) {
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = address;
    dev_config.scl_speed_hz = I2C_SCL_SPEED_HZ;
    i2c_master_dev_handle_t dev_handle;
    auto result = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_config, &dev_handle);
    if (result != ESP_OK) {
        return result;
    }
    result = i2c_master_transmit(dev_handle, src, nbytes, (int)timeout_ms);
    i2c_master_bus_rm_device(dev_handle);
    return result;
}

static esp_err_t write_read_iic_data(const uint8_t address, const uint8_t *write_src, size_t write_nbytes, uint8_t *read_dst, size_t read_nbytes, const uint16_t timeout_ms) {
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = address;
    dev_config.scl_speed_hz = I2C_SCL_SPEED_HZ;
    i2c_master_dev_handle_t dev_handle;
    auto result = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_config, &dev_handle);
    if (result != ESP_OK) {
        return result;
    }
    result = i2c_master_transmit_receive(dev_handle, write_src, write_nbytes, read_dst, read_nbytes, (int)timeout_ms);
    i2c_master_bus_rm_device(dev_handle);
    return result;
}