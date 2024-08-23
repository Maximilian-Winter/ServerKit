#pragma once

#include <string>
#include <vector>
#include "BinaryData.h"
#include "HTTPUrl.h"
#include "HTTPHeader.h"
#include "HTTPBody.h"

class HTTPMessage : public NetworkMessages::BinaryData
{
public:
    enum class Type {
        REQUEST,
        RESPONSE
    };

    enum class Method {
        GET_METHOD,
        POST_METHOD,
        PUT_METHOD,
        DELETE_METHOD,
        HEAD_METHOD,
        OPTIONS_METHOD,
        PATCH_METHOD
    };

    explicit HTTPMessage(Type type) : m_type(type) {}

    void setMethod(Method method) { m_method = method; }
    void setUrl(const HTTPUrl& url) { m_url = url; }
    void setVersion(const std::string& version) { m_version = version; }
    void setStatusCode(int statusCode) { m_statusCode = statusCode; }
    void setStatusMessage(const std::string& statusMessage) { m_statusMessage = statusMessage; }
    void addHeader(const std::string& key, const std::string& value) { m_headers.addHeader(key, value); }
    void setBody(const HTTPBody& body) { m_body = body; }

    [[nodiscard]] Type getType() const { return m_type; }
    [[nodiscard]] Method getMethod() const { return m_method; }
    [[nodiscard]] const HTTPUrl& getUrl() const { return m_url; }
    [[nodiscard]] const std::string& getVersion() const { return m_version; }
    [[nodiscard]] int getStatusCode() const { return m_statusCode; }
    [[nodiscard]] const std::string& getStatusMessage() const { return m_statusMessage; }
    [[nodiscard]] const HTTPHeader& getHeaders() const { return m_headers; }
    [[nodiscard]] const HTTPBody& getBody() const { return m_body; }

    [[nodiscard]] std::vector<uint8_t> serialize() override {
        reset_byte_size();
        std::vector<uint8_t> data;

        // Serialize start line
        std::string startLine;
        if (m_type == Type::REQUEST) {
            startLine = methodToString(m_method) + " " + m_url.getPath() + " " + m_version + "\r\n";
        } else {
            startLine = m_version + " " + std::to_string(m_statusCode) + " " + m_statusMessage + "\r\n";
        }
        append_bytes(data, startLine);

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

        // Deserialize start line
        std::string startLine = read_bytes<std::string>(data, offset);
        parseStartLine(startLine);

        // Deserialize headers
        std::vector<uint8_t> headerData(data.begin() + offset, data.end());
        m_headers.deserialize(headerData);
        offset += m_headers.ByteSize();

        // Deserialize body
        std::vector<uint8_t> bodyData(data.begin() + offset, data.end());
        m_body.deserialize(bodyData);
    }

private:
    Type m_type;
    Method m_method;
    HTTPUrl m_url;
    std::string m_version;
    int m_statusCode{};
    std::string m_statusMessage;
    HTTPHeader m_headers;
    HTTPBody m_body;

    static std::string methodToString(Method method) {
        switch (method) {
            case Method::GET_METHOD: return "GET";
            case Method::POST_METHOD: return "POST";
            case Method::PUT_METHOD: return "PUT";
            case Method::DELETE_METHOD: return "DELETE";
            case Method::HEAD_METHOD: return "HEAD";
            case Method::OPTIONS_METHOD: return "OPTIONS";
            case Method::PATCH_METHOD: return "PATCH";
            default: return "UNKNOWN";
        }
    }

    static Method stringToMethod(const std::string& method) {
        if (method == "GET") return Method::GET_METHOD;
        if (method == "POST") return Method::POST_METHOD;
        if (method == "PUT") return Method::PUT_METHOD;
        if (method == "DELETE") return Method::DELETE_METHOD;
        if (method == "HEAD") return Method::HEAD_METHOD;
        if (method == "OPTIONS") return Method::OPTIONS_METHOD;
        if (method == "PATCH") return Method::PATCH_METHOD;
        throw std::runtime_error("Unknown HTTP method: " + method);
    }

    void parseStartLine(const std::string& startLine) {
        std::istringstream iss(startLine);
        std::string first, second, third;
        iss >> first >> second >> third;

        if (m_type == Type::REQUEST) {
            m_method = stringToMethod(first);
            m_url.parse(second);
            m_version = third;
        } else {
            m_version = first;
            m_statusCode = std::stoi(second);
            m_statusMessage = third;
        }
    }
};