#include <stdexcept>

#include "esp_http_server.h"

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

static esp_err_t get_handler_cb(httpd_req_t *req) {
    throw std::runtime_error("not implemented");
}

extern "C" {
    void app_main(void) {
        const auto get_handler = httpd_uri_t {
            .uri      = "/some/data",
            .method   = HTTP_GET,
            .handler  = get_handler_cb,
            .user_ctx = NULL
        };
        const auto server = start_webserver(get_handler);
    }
}