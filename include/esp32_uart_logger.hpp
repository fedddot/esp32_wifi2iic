#ifndef ESP32_UART_LOGGER_HPP
#define ESP32_UART_LOGGER_HPP

#include <string>
#include <stdexcept>
#include <cstring>

namespace mcu_server {
    class Esp32UartLogger {
    public:
        Esp32UartLogger() {
            if (s_reference_count) {
                throw std::runtime_error("Esp32UartLogger is already initialized");
            }
            // TODO: init UART Logger:
            // receive in the args the Baud, data bits, parity, stop bits
            ++s_reference_count;
        }
        Esp32UartLogger(const Esp32UartLogger&) {
            ++s_reference_count;
        }
        Esp32UartLogger& operator=(const Esp32UartLogger&) = delete;
        ~Esp32UartLogger() noexcept {
            --s_reference_count;
            if (s_reference_count) {
                return;
            }
            // TODO: deinit UART Logger
        }
        void log(const std::string& message) const {
            // TODO: wtrite the message ended with \n to the UART
        }
    private:
        static std::size_t s_reference_count;
    };

    inline std::size_t Esp32UartLogger::s_reference_count = 0;
} // namespace mcu_server

#endif // ESP32_UART_LOGGER_HPP