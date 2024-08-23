#pragma once

#include <string>
#include <utility>
#include <vector>
#include "BinaryData.h"
#include "HTTPUrl.h"
#include "HTTPHeader.h"
#include "HTTPBody.h"

class HTTPMessage : public NetworkMessages::HTTPBinaryData
{
public:


    HTTPMessage() : m_headers() {}

    void addHeader(const std::string& key, const std::string& value) { m_headers.addHeader(key, value); }
    void setHeader(HTTPHeader header) { m_headers = std::move(header); }
    void setBody(HTTPBody body) { m_body = std::move(body); }
    [[nodiscard]] HTTPHeader::Type getType() const { return m_headers.m_type; }
    [[nodiscard]] HTTPHeader::Method getMethod() const { return m_headers.m_method; }
    [[nodiscard]] const HTTPUrl& getUrl() const { return m_headers.m_url; }
    [[nodiscard]] const std::string& getVersion() const { return m_headers.m_version; }
    [[nodiscard]] int getStatusCode() const { return m_headers.m_statusCode; }
    [[nodiscard]] const std::string& getStatusMessage() const { return m_headers.m_statusMessage; }
    void setMethod(HTTPHeader::Method method) { m_headers.m_method = method; }
    void setUrl(const HTTPUrl& url) { m_headers.m_url = url; }
    void setVersion(const std::string& version) { m_headers.m_version = version; }
    void setStatusCode(int statusCode) { m_headers.m_statusCode = statusCode; }
    void setStatusMessage(const std::string& statusMessage) { m_headers.m_statusMessage = statusMessage; }

    [[nodiscard]] const HTTPHeader& getHeaders() const { return m_headers; }
    [[nodiscard]] const HTTPBody& getBody() const { return m_body; }

    [[nodiscard]] std::vector<uint8_t> serialize() override {
        reset_byte_size();
        std::vector<uint8_t> data;

        // Serialize headers
        auto headerData = m_headers.serialize();
        data.insert(data.end(), headerData.begin(), headerData.end());
        addToByteSize(m_headers.ByteSize());

        // Serialize body
        auto bodyData = m_body.serialize();
        data.insert(data.end(), bodyData.begin(), bodyData.end());
        addToByteSize(m_body.ByteSize());

        return data;
    }

    void deserialize(const std::vector<uint8_t>& data) override {
        size_t offset = 0;

        // Deserialize headers
        std::vector<uint8_t> headerData(data.begin() + offset, data.end());
        m_headers.deserialize(headerData);
        m_headers.serialize();
        offset += m_headers.ByteSize();

        // Deserialize body
        std::vector<uint8_t> bodyData(data.begin() + offset, data.end());
        m_body.deserialize(bodyData);
    }

private:

    HTTPHeader m_headers;
    HTTPBody m_body;

};