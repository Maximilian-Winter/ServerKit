#pragma once

#include <vector>
#include <string>
#include "BinaryData.h"

class HTTPBody : public NetworkMessages::BinaryData {
public:
    void setContent(const std::string& content) {
        m_content = content;
    }

    [[nodiscard]] const std::string& getContent() const {
        return m_content;
    }

    [[nodiscard]] std::vector<uint8_t> serialize() override {
        reset_byte_size();
        std::vector<uint8_t> data;
        append_bytes(data, m_content);
        return data;
    }

    void deserialize(const std::vector<uint8_t>& data) override {
        size_t offset = 0;
        m_content = read_bytes<std::string>(data, offset);
    }

private:
    std::string m_content;
};