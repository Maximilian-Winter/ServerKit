#pragma once

#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include "BinaryData.h"

class HTTPHeader : public NetworkMessages::HTTPBinaryData {
public:
    enum class Type {
        REQUEST,
        RESPONSE
    };

    enum class Method {
        DEFAULT_METHOD,
        GET_METHOD,
        POST_METHOD,
        PUT_METHOD,
        DELETE_METHOD,
        HEAD_METHOD,
        OPTIONS_METHOD,
        PATCH_METHOD
    };

    Type m_type;
    Method m_method;
    HTTPUrl m_url;
    std::string m_version;
    int m_statusCode{};
    std::string m_statusMessage;

    HTTPHeader() : m_type(Type::REQUEST), m_method(Method::DEFAULT_METHOD) {} // Default constructor

    [[nodiscard]] Type getType() const { return m_type; }
    [[nodiscard]] Method getMethod() const { return m_method; }
    [[nodiscard]] const HTTPUrl& getUrl() const { return m_url; }
    [[nodiscard]] const std::string& getVersion() const { return m_version; }
    [[nodiscard]] int getStatusCode() const { return m_statusCode; }
    [[nodiscard]] const std::string& getStatusMessage() const { return m_statusMessage; }
    void setMethod(Method method) { m_method = method; }
    void setUrl(const HTTPUrl& url) { m_url = url; }
    void setVersion(const std::string& version) { m_version = version; }
    void setStatusCode(int statusCode) { m_statusCode = statusCode; }
    void setStatusMessage(const std::string& statusMessage) { m_statusMessage = statusMessage; }

    void setFirstLine(const std::string& line)
    {
        firstLine = line;
        parseStartLine(line);
    }
    void addHeader(const std::string& key, const std::string& value) {
        m_headers[key] = value;
    }

    [[nodiscard]] std::string getHeader(const std::string& key) const {
        auto it = m_headers.find(key);
        return (it != m_headers.end()) ? it->second : "";
    }

    [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeaders() const {
        return m_headers;
    }

    [[nodiscard]] std::vector<uint8_t> serialize() override {
        reset_byte_size();
        std::vector<uint8_t> data;
        std::string startLine;

        if (isHttpMethod(methodToString(m_method))) {
            startLine = methodToString(m_method) + " " + m_url.getPath() + " " + m_version;
        } else {
            startLine = m_version + " " + std::to_string(m_statusCode) + " " + m_statusMessage;
        }
        setFirstLine(startLine);

        append_bytes(data, firstLine);
        for (const auto& [key, value] : m_headers) {
            std::string header = key + ": " + value;
            append_bytes(data, header);
        }
        std::string str;
        append_bytes(data, str);
        return data;
    }

    void deserialize(const std::vector<uint8_t>& data) override {
        int offset = 0;
        std::string line;
        line = read_bytes(data, offset);
        setFirstLine(line); // This will now parse the type as well
        while ((line = read_bytes(data, offset)).empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 2, line.length() - colonPos - 4); // -4 to remove \r\n
                m_headers[key] = value;
            }
        }
    }

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

        if (isHttpMethod(first)) {
            m_type = Type::REQUEST;
            m_method = stringToMethod(first);
            m_url.parse(second);
            m_version = third;
        } else {
            m_type = Type::RESPONSE;
            m_version = first;
            m_statusCode = std::stoi(second);
            m_statusMessage = third;
        }
    }

private:
    std::unordered_map<std::string, std::string> m_headers;
    std::string firstLine;

    bool isHttpMethod(const std::string& str) const {
        static const std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH"};
        return std::find(methods.begin(), methods.end(), str) != methods.end();
    }
};