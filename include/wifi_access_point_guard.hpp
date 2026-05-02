#ifndef WIFI_ACCESS_POINT_GUARD_HPP
#define WIFI_ACCESS_POINT_GUARD_HPP

#include <cstddef>
#include <stdexcept>
#include <string>

#include "esp_wifi_types_generic.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

namespace mcu_server {
    class WifiAccessPointGuard {
    public:
        WifiAccessPointGuard(
            const std::string& ssid = "ESP32-AP",
            const std::string& password = "EspPassWord"
        ) {
            if (s_reference_count) {
                throw std::runtime_error("WifiAccessPointGuard is already initialized");
            }
            esp_err_t ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }
            ESP_ERROR_CHECK(esp_netif_init());
            ESP_ERROR_CHECK(esp_event_loop_create_default());
            esp_netif_create_default_wifi_ap();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            wifi_config_t ap_config = {
                .ap = generate_ap_cfg(ssid, password)
            };
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
            ESP_ERROR_CHECK(esp_wifi_start());
            ++s_reference_count;
        }

        WifiAccessPointGuard(const WifiAccessPointGuard& other) {
            s_reference_count++;
        }
        WifiAccessPointGuard& operator=(const WifiAccessPointGuard&) = delete;
        virtual ~WifiAccessPointGuard() noexcept {
            --s_reference_count;
            if (s_reference_count) {
                return;
            }
            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_ERROR_CHECK(esp_wifi_deinit());
            ESP_ERROR_CHECK(esp_event_loop_delete_default());
            ESP_ERROR_CHECK(nvs_flash_erase());
            ESP_ERROR_CHECK(nvs_flash_deinit());
        }
    private:
        static std::size_t s_reference_count;
        static wifi_ap_config_t generate_ap_cfg(const std::string& ssid, const std::string& password) {
            auto config = wifi_ap_config_t {
                .ssid = {'\0'},
                .password = {'\0'},
                .ssid_len = 0,
                .channel = 0, // TODO: check it
                .authmode = WIFI_AUTH_WPA2_PSK,
                .ssid_hidden = 0,
                .max_connection = 1,
                .beacon_interval = 100,
                .csa_count = 3,
                .dtim_period = 1,
                .pairwise_cipher = WIFI_CIPHER_TYPE_NONE,
                .ftm_responder = false,
                .pmf_cfg = wifi_pmf_config_t {
                    .capable = false,
                    .required = false
                },
                .sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED,
                .transition_disable = 0,
                .sae_ext = 0,
                .bss_max_idle_cfg = wifi_bss_max_idle_config_t {
                    .period = 0,
                    .protected_keep_alive = false
                },
                .gtk_rekey_interval = 0
            };
            if (ssid.size() > sizeof(config.ssid)) {
                throw std::invalid_argument("received ssid is too long");
            }
            if (ssid.size() != ssid.copy((char *)(config.ssid), ssid.size())) {
                throw std::runtime_error("failed to create AP config: SSID copy failure");
            }
            config.ssid_len = ssid.size();
            if (password.size() > sizeof(config.password)) {
                throw std::invalid_argument("received password is too long");
            }
            if (password.size() != password.copy((char *)(config.password), password.size())) {
                throw std::runtime_error("failed to create AP config: Password copy failure");
            }
            return config;
        }
    };

    inline std::size_t WifiAccessPointGuard::s_reference_count = 0;
} // namespace mcu_server

#endif // WIFI_ACCESS_POINT_GUARD_HPP