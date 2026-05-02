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

#include "service.pb.h"

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

static bool decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);
static bool encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

static esp_err_t config_wifi_cb(httpd_req_t *request) {
    auto ctx = static_cast<SetWifiContext *>(request->user_ctx);
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
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
        return ESP_ERR_INVALID_ARG;
    }
    ctx->sta_guard->emplace(ssid, pass, ctx->nvs_guard);

    if (const auto res = httpd_resp_set_status(request, HTTPD_200); res != ESP_OK) {
        return res;
    }
    if (const auto res = httpd_resp_set_type(request, HTTPD_TYPE_OCTET); res != ESP_OK) {
        return res;
    }
    service_api_ServiceResponse response {
        .result = service_api_Result::service_api_Result_SUCCESS
    };
    pb_writer.write(response);
    return ESP_OK;
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