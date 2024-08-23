#pragma once

#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include "BinaryData.h"

class HTTPHeader : public NetworkMessages::BinaryData {
public:
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
        for (const auto& [key, value] : m_headers) {
            std::string header = key + ": " + value + "\r\n";
            append_bytes(data, header);
        }
        append_bytes(data, std::string("\r\n"));
        return data;
    }

    void deserialize(const std::vector<uint8_t>& data) override {
        size_t offset = 0;
        std::string line;
        while ((line = read_bytes<std::string>(data, offset)) != "\r\n") {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 2, line.length() - colonPos - 4); // -4 to remove \r\n
                m_headers[key] = value;
            }
        }
    }

private:
    std::unordered_map<std::string, std::string> m_headers;
};