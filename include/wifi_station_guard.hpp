#ifndef WIFI_STATION_GUARD_HPP
#define WIFI_STATION_GUARD_HPP

#include <string>
#include <stdexcept>
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"

#include "nvs_flash_guard.hpp"

namespace mcu_server {
    class WifiStationGuard {
    public:
        WifiStationGuard(const std::string& ssid, const std::string& password, const NvsFlashGuard& nvs_guard) : m_nvs_guard(nvs_guard) {
            if (s_reference_count) {
                throw std::runtime_error("WifiStationGuard is already initialized");
            }
            ESP_ERROR_CHECK(esp_netif_init());
            ESP_ERROR_CHECK(esp_event_loop_create_default());
            esp_netif_create_default_wifi_sta();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            wifi_config_t sta_config = {
                .sta = generate_sta_config(ssid, password)
            };
            sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
            ESP_ERROR_CHECK(esp_wifi_start());
            ESP_ERROR_CHECK(esp_wifi_connect());
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
        static std::size_t s_reference_count;

        static wifi_sta_config_t generate_sta_config(const std::string& ssid, const std::string& password) {
            auto config = wifi_sta_config_t {};
            if (ssid.size() > sizeof(config.ssid)) {
                throw std::invalid_argument("received ssid is too long");
            }
            if (ssid.size() != ssid.copy((char *)(config.ssid), ssid.size())) {
                throw std::runtime_error("failed to create STA config: SSID copy failure");
            }
            if (password.size() > sizeof(config.password)) {
                throw std::invalid_argument("received password is too long");
            }
            if (password.size() != password.copy((char *)(config.password), password.size())) {
                throw std::runtime_error("failed to create STA config: Password copy failure");
            }
            return config;
        }
    };

    inline std::size_t WifiStationGuard::s_reference_count = 0;
} // namespace mcu_server

#endif // WIFI_STATION_GUARD_HPP
