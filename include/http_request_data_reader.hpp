#ifndef	HTTP_REQUEST_DATA_READER_HPP
#define	HTTP_REQUEST_DATA_READER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "esp_http_server.h"
#include "reader.hpp"

namespace nanoipc {
    template <std::size_t BUFFER_SIZE = 256UL>
	class HttpRequestDataReader: public Reader<std::vector<std::uint8_t>> {
	public:
		HttpRequestDataReader(httpd_req_t *request): m_request(request) {
			if (!m_request) {
				throw std::invalid_argument("request cannot be null");
			}
		}
		HttpRequestDataReader(const HttpRequestDataReader&) = default;
		HttpRequestDataReader& operator=(const HttpRequestDataReader&) = default;

		std::optional<std::vector<std::uint8_t>> read() override {
            std::vector<std::uint8_t> data_buffer(BUFFER_SIZE);
            const auto read_result = httpd_req_recv(m_request, reinterpret_cast<char *>(data_buffer.data()), data_buffer.size());
            if (read_result < 0) {
                if (read_result == HTTPD_SOCK_ERR_TIMEOUT) {
                    return std::nullopt;
                } else {
                    throw std::runtime_error("failed to read request data");
                }
            }
            data_buffer.resize(read_result);
            return data_buffer;
		}
	private:
        httpd_req_t *m_request;
	};
}

#endif // HTTP_REQUEST_DATA_READER_HPP