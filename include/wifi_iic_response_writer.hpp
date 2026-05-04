#ifndef	WIFI_IIC_RESPONSE_WRITER_HPP
#define	WIFI_IIC_RESPONSE_WRITER_HPP

#include <stdexcept>

#include "esp_http_server.h"
#include "writer.hpp"
#include "pb_message_writer.hpp"
#include "http_response_data_writer.hpp"
#include "service.pb.h"

namespace nanoipc {
	class WifiI2CResponseWriter: public Writer<service_api_WifiI2CRelayResponse> {
	public:
		WifiI2CResponseWriter(httpd_req_t *request): m_request(request) {
			if (!m_request) {
				throw std::invalid_argument("request cannot be null");
			}
		}
		WifiI2CResponseWriter(const WifiI2CResponseWriter&) = default;
		WifiI2CResponseWriter& operator=(const WifiI2CResponseWriter&) = default;

		void write(const service_api_WifiI2CRelayResponse& response) const override {
            HttpResponseDataWriter writer(m_request);
            PbMessageWriter<service_api_WifiI2CRelayResponse> pb_writer(
                &writer,
                service_api_WifiI2CRelayResponse_fields
            );
            pb_writer.write(response);
		}
	private:
        httpd_req_t *m_request;
	};
}

#endif // WIFI_IIC_RESPONSE_WRITER_HPP