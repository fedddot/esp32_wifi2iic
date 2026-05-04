#ifndef	HTTP_SERVER_HPP
#define	HTTP_SERVER_HPP

#include <cstdint>
#include <stdexcept>

#include "esp_err.h"
#include "esp_http_server.h"

namespace nanoipc {
	class HttpServer {
	public:
        HttpServer(const std::uint16_t port, const bool keep_alive): server(nullptr) {
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.server_port = port;
            config.task_priority = tskIDLE_PRIORITY + 10;
            config.keep_alive_enable = keep_alive;
            config.keep_alive_idle = 5;
            config.keep_alive_interval = 1;
            config.keep_alive_count = 3;
            config.lru_purge_enable = true;
            if ((httpd_start(&server, &config) != ESP_OK) || (server == nullptr)) {
                throw std::runtime_error("Failed to start HTTP server");
            }
		}
		HttpServer(const HttpServer&) = delete;
		HttpServer& operator=(const HttpServer&) = delete;
        virtual ~HttpServer() noexcept {
            httpd_stop(server);
        }
        void register_handler(const std::string& uri, const httpd_method_t method, esp_err_t (* handler)(httpd_req_t *)) {
            const auto uri_handler = httpd_uri_t {
                .uri      = uri.c_str(),
                .method   = method,
                .handler  = handler,
                .user_ctx = nullptr
            };
            if (httpd_register_uri_handler(server, &uri_handler) != ESP_OK) {
                throw std::runtime_error("Failed to register URI handler");
            }
        }
	private:
        httpd_handle_t server;
	};
}

#endif // HTTP_SERVER_HPP