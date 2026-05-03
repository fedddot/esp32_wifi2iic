#ifndef ESP32_UART_LOGGER_HPP
#define ESP32_UART_LOGGER_HPP

#include <string>
#include <stdexcept>
#include <cstring>
#include "driver/uart.h"

namespace mcu_server {
    class Esp32UartLogger {
    public:
        Esp32UartLogger(
            int baud_rate,
            uart_word_length_t data_bits,
            uart_parity_t parity,
            uart_stop_bits_t stop_bits,
            uart_port_t uart_num = UART_NUM_0
        ) : m_uart_num(uart_num) {
            if (s_reference_count) {
                throw std::runtime_error("Esp32UartLogger is already initialized");
            }
            const uart_config_t uart_config = {
                .baud_rate  = baud_rate,
                .data_bits  = data_bits,
                .parity     = parity,
                .stop_bits  = stop_bits,
                .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
                .source_clk = UART_SCLK_DEFAULT,
            };
            if (uart_param_config(m_uart_num, &uart_config) != ESP_OK) {
                throw std::runtime_error("Failed to configure UART parameters");
            }
            if (uart_driver_install(m_uart_num, 0, TX_BUF_SIZE, 0, nullptr, 0) != ESP_OK) {
                throw std::runtime_error("Failed to install UART driver");
            }
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
            uart_driver_delete(m_uart_num);
        }
        void log(const std::string& message) const {
            const std::string line = message + "\n";
            uart_write_bytes(m_uart_num, line.c_str(), line.size());
        }
    private:
        static constexpr int TX_BUF_SIZE = 1024;
        uart_port_t m_uart_num;
        static std::size_t s_reference_count;
    };

    inline std::size_t Esp32UartLogger::s_reference_count = 0;
} // namespace mcu_server

#endif // ESP32_UART_LOGGER_HPP