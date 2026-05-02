#include <optional>
#include <stdexcept>

#include "esp_http_server.h"
#include "driver/gpio.h"

#include "http_parser.h"
#include "wifi_station_guard.hpp"
#include "http_request_data_reader.hpp"
#include "http_request_data_writer.hpp"
#include "pb_message_reader.hpp"
#include "pb_message_writer.hpp"

#include "service.pb.h"

#ifndef NETWORK_SSID
#  error "NETWORK_SSID is not defined. Please pass it to the cmake command"
#endif

#ifndef NETWORK_PASSWORD
#  error "NETWORK_PASSWORD is not defined. Please pass it to the cmake command"
#endif

#ifndef SERVICE_PORT
#  error "SERVICE_PORT is not defined. Please pass it to the cmake command"
#endif

static httpd_handle_t start_webserver(const httpd_uri_t& get_handler) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SERVICE_PORT;
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

static bool decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);
static bool encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

static esp_err_t config_wifi_cb(httpd_req_t *request) {
    nanoipc::HttpRequestDataReader data_reader(request);
    nanoipc::HttpRequestDataWriter data_writer(request);
    std::string ssid;
    std::string pass;
    nanoipc::PbMessageReader<service_api_WifiConfigRequest> pb_reader(
        &data_reader,
        service_api_WifiConfigRequest_fields,
        [&ssid, &pass](service_api_WifiConfigRequest *request) {
            request->ssid.funcs.decode = decode_string;
            request->ssid.arg = &ssid;
            request->pass.funcs.decode = decode_string;
            request->pass.arg = &pass;
        }
    );
    nanoipc::PbMessageWriter<service_api_ServiceResponse> pb_writer(&data_writer, service_api_ServiceResponse_fields);

    const auto pb_msg_opt = pb_reader.read();
        if (!pb_msg_opt.has_value()) {
        service_api_ServiceResponse response {
            .result = service_api_Result::service_api_Result_BAD_REQUEST
        };
        pb_writer.write(response);
        return ESP_OK;
    }

    service_api_ServiceResponse response {
        .result = service_api_Result::service_api_Result_SUCCESS
    };
    pb_writer.write(response);
    return ESP_OK;
}

static void blink_loop();

extern "C" {
    void app_main(void) {
        try {
            mcu_server::NvsFlashGuard nvs_guard;
            mcu_server::WifiStationGuard network_guard(NETWORK_SSID, NETWORK_PASSWORD, nvs_guard);
            const auto get_handler = httpd_uri_t {
                .uri      = "/config/wifi",
                .method   = HTTP_POST,
                .handler  = config_wifi_cb,
                .user_ctx = nullptr
            };
            const auto server = start_webserver(get_handler);
            while (true) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        } catch (...) {
            blink_loop();
        }
    }
}

bool encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	if (!arg || !*arg) {
		throw std::runtime_error("encode_string called with null arg");
	}
	const auto str = static_cast<const std::string *>(*arg);
	if (!pb_encode_tag_for_field(stream, field)) {
		return false;
	}
	return pb_encode_string(stream, (const pb_byte_t *)(str->c_str()), str->size());
}

bool decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
	if (!arg) {
		throw std::runtime_error("encode_string called with null arg");
	}
	std::string *dst = *(std::string **)(arg);
    if (!dst) {
		throw std::runtime_error("destination buffer is not set");
	}
    enum { MAX_BUFFER_SIZE = 256UL };
	pb_byte_t buff[MAX_BUFFER_SIZE] = {'\0'};
	if (!pb_read(stream, buff, stream->bytes_left)) {
		return false;
	}
	*dst = std::string((const char *)buff);
    return true;
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
    int led_off_time = 1900;
    while (true) {
        gpio_set_level(GPIO_NUM_15, true);
        vTaskDelay(pdMS_TO_TICKS(led_on_time));
        gpio_set_level(GPIO_NUM_15, false);
        vTaskDelay(pdMS_TO_TICKS(led_off_time));
    }
}