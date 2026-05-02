#ifndef	HTTP_REQUEST_DATA_WRITER_HPP
#define	HTTP_REQUEST_DATA_WRITER_HPP

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "esp_http_server.h"
#include "writer.hpp"

namespace nanoipc {
	class CobsFrameWriter: public Writer<std::vector<std::uint8_t>> {
	public:
		CobsFrameWriter(httpd_req_t *request): m_request(request) {
			if (!m_request) {
				throw std::invalid_argument("request cannot be null");
			}
		}
		CobsFrameWriter(const CobsFrameWriter&) = default;
		CobsFrameWriter& operator=(const CobsFrameWriter&) = default;

		void write(const std::vector<std::uint8_t>& data) const override {
			if (httpd_resp_send(m_request, reinterpret_cast<const char *>(data.data()), data.size()) != ESP_OK) {
                throw std::runtime_error("failed to send response data");
            }
		}

	private:
		httpd_req_t *m_request;
	};
}

#endif // HTTP_REQUEST_DATA_WRITER_HPP
