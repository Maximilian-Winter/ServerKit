#pragma once

#include <string>
#include <utility>
#include <vector>
#include "BinaryData.h"
#include "HTTPUrl.h"
#include "HTTPHeader.h"
#include "HTTPBody.h"

class HTTPMessage
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

    [[nodiscard]] const HTTPHeader& getHeader() const { return m_headers; }
    [[nodiscard]] const HTTPBody& getBody() const { return m_body; }

    [[nodiscard]] FastVector::ByteVector serialize()  {
        FastVector::ByteVector data;

        // Serialize headers
        auto headerData = m_headers.serialize();
        data.insert(data.end(), headerData.begin(), headerData.end());

        // Serialize body
        auto bodyData = m_body.serialize();
        data.insert(data.end(), bodyData.begin(), bodyData.end());

        return data;
    }


private:

    HTTPHeader m_headers;
    HTTPBody m_body;

};