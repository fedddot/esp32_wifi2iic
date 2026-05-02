#ifndef NVS_FLASH_GUARD_HPP
#define NVS_FLASH_GUARD_HPP

#include <cstddef>
#include <stdexcept>

#include "nvs_flash.h"

namespace mcu_server {
    class NvsFlashGuard {
    public:
        NvsFlashGuard() {
            if (s_reference_count) {
                throw std::runtime_error("NvsFlashGuard is already initialized");
            }
            esp_err_t ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }
            ++s_reference_count;
        }

        NvsFlashGuard(const NvsFlashGuard& other) {
            s_reference_count++;
        }
        NvsFlashGuard& operator=(const NvsFlashGuard&) = delete;
        virtual ~NvsFlashGuard() noexcept {
            --s_reference_count;
            if (s_reference_count) {
                return;
            }
            ESP_ERROR_CHECK(nvs_flash_erase());
            ESP_ERROR_CHECK(nvs_flash_deinit());
        }
    private:
        static std::size_t s_reference_count;
    };

    inline std::size_t NvsFlashGuard::s_reference_count = 0;
} // namespace mcu_server

#endif // NVS_FLASH_GUARD_HPP