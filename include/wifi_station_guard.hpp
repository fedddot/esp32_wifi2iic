#ifndef WIFI_STATION_GUARD_HPP
#define WIFI_STATION_GUARD_HPP

#include <string>
#include <stdexcept>
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"

#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "nvs_flash_guard.hpp"

namespace mcu_server {
    class WifiStationGuard {
    public:
        WifiStationGuard(const std::string& ssid, const std::string& password, const NvsFlashGuard& nvs_guard): m_nvs_guard(nvs_guard), m_netif(nullptr) {
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

            if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, WifiStationGuard::event_handler, nullptr, nullptr) != ESP_OK) {
                throw std::runtime_error("Failed to register Wi-Fi event handler");
            }
            if (esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, WifiStationGuard::event_handler, nullptr, nullptr) != ESP_OK) {
                throw std::runtime_error("Failed to register IP event handler");
            }

            wifi_config_t wifi_config = {};
            std::memset(&wifi_config, 0, sizeof(wifi_config));
            ssid.copy((char*)wifi_config.sta.ssid, ssid.size());
            password.copy((char*)wifi_config.sta.password, password.size());
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;
            if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
                throw std::runtime_error("Failed to set Wi-Fi mode");
            }
            if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
                throw std::runtime_error("Failed to set Wi-Fi configuration");
            }
            if (esp_wifi_set_ps(wifi_ps_type_t::WIFI_PS_NONE) != ESP_OK) {
                throw std::runtime_error("Failed to set Wi-Fi power save mode");
            }
            if (esp_wifi_start() != ESP_OK) {
                throw std::runtime_error("Failed to start Wi-Fi");
            }
            ++s_reference_count;
        }
        WifiStationGuard(const WifiStationGuard& other) : m_nvs_guard(other.m_nvs_guard), m_netif(other.m_netif) {
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
            esp_netif_destroy_default_wifi(m_netif);
            m_netif = nullptr;
        }
    private:
        NvsFlashGuard m_nvs_guard;
        esp_netif_t *m_netif;
        static std::size_t s_reference_count;

        static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
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