#ifndef WIFI_STATION_GUARD_HPP
#define WIFI_STATION_GUARD_HPP

#include <string>
#include <stdexcept>
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"

#include "nvs_flash_guard.hpp"
#include "esp32_uart_logger.hpp"

namespace mcu_server {
    class WifiStationGuard {
    public:
        WifiStationGuard(const std::string& ssid, const std::string& password, const NvsFlashGuard& nvs_guard, const Esp32UartLogger& uart_logger): m_nvs_guard(nvs_guard), m_uart_logger(uart_logger), m_netif(nullptr) {
            if (s_reference_count) {
                throw std::runtime_error("WifiStationGuard is already initialized");
            }

            if (esp_netif_init() != ESP_OK) {
                throw std::runtime_error("Failed to initialize network interface");
            }
            if (esp_event_loop_create_default() != ESP_OK) {
                throw std::runtime_error("Failed to create default event loop");
            }
            m_netif = esp_netif_create_default_wifi_sta();
            if (m_netif == nullptr) {
                throw std::runtime_error("Failed to create default Wi-Fi station");
            }
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            if (esp_wifi_init(&cfg) != ESP_OK) {
                throw std::runtime_error("Failed to initialize Wi-Fi");
            }

            if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, WifiStationGuard::event_handler, &m_uart_logger, nullptr) != ESP_OK) {
                throw std::runtime_error("Failed to register Wi-Fi event handler");
            }
            if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, WifiStationGuard::event_handler, &m_uart_logger, nullptr) != ESP_OK) {
                throw std::runtime_error("Failed to register IP event handler");
            }

            wifi_config_t wifi_config = {};
            std::memset(&wifi_config, 0, sizeof(wifi_config));
            ssid.copy((char*)wifi_config.sta.ssid, ssid.size());
            password.copy((char*)wifi_config.sta.password, password.size());
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_start();
            ++s_reference_count;
        }
        WifiStationGuard(const WifiStationGuard&) {
            ++s_reference_count;
        }
        WifiStationGuard& operator=(const WifiStationGuard&) = delete;
        ~WifiStationGuard() noexcept {
            --s_reference_count;
            if (s_reference_count) {
                return;
            }
            esp_wifi_disconnect();
            esp_wifi_stop();
            esp_wifi_deinit();
            esp_event_loop_delete_default();
        }
    private:
        NvsFlashGuard m_nvs_guard;
        Esp32UartLogger m_uart_logger;
        esp_netif_t *m_netif;
        static std::size_t s_reference_count;

        static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
            Esp32UartLogger *uart_logger = static_cast<Esp32UartLogger *>(arg);
            uart_logger->log("Wi-Fi event received: " + std::string(event_base) + ", " + std::to_string(event_id));
            if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
                esp_wifi_connect();
            } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
                esp_wifi_connect(); // Reconnect
            } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
                // Connected successfully — nothing to do
            }
        }
    };

    inline std::size_t WifiStationGuard::s_reference_count = 0;
} // namespace mcu_server

#endif // WIFI_STATION_GUARD_HPP