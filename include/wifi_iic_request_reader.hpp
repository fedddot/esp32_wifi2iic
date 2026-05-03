#ifndef	WIFI_IIC_REQUEST_READER_HPP
#define	WIFI_IIC_REQUEST_READER_HPP

#include <cstddef>
#include <optional>
#include <stdexcept>

#include "esp_http_server.h"
#include "reader.hpp"
#include "pb_message_reader.hpp"
#include "http_request_data_reader.hpp"
#include "wifi_iic_relay.pb.h"

namespace nanoipc {
    template <std::size_t BUFFER_SIZE = 256UL>
	class WifiI2CRequestReader: public Reader<wifi_iic_relay_api_WifiI2CRelayRequest> {
	public:
		WifiI2CRequestReader(httpd_req_t *request): m_request(request) {
			if (!m_request) {
				throw std::invalid_argument("request cannot be null");
			}
		}
		WifiI2CRequestReader(const WifiI2CRequestReader&) = default;
		WifiI2CRequestReader& operator=(const WifiI2CRequestReader&) = default;

		std::optional<wifi_iic_relay_api_WifiI2CRelayRequest> read() override {
            HttpRequestDataReader<BUFFER_SIZE> reader(m_request);
            PbMessageReader<wifi_iic_relay_api_WifiI2CRelayRequest> pb_reader(
                &reader,
                wifi_iic_relay_api_WifiI2CRelayRequest_fields,
                [](wifi_iic_relay_api_WifiI2CRelayRequest *request) {
                    *request = wifi_iic_relay_api_WifiI2CRelayRequest_init_zero;
                }
            );
            return pb_reader.read();
		}
	private:
        httpd_req_t *m_request;
	};
}

#endif // WIFI_IIC_REQUEST_READER_HPP