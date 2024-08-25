#pragma once

#include <vector>
#include <string>
#include "BinaryData.h"

class HTTPBody : public NetworkMessages::HTTPBinaryData {
public:
    void setContent(const std::string& content) {
        m_content = content;
    }

    [[nodiscard]] const std::string& getContent() const {
        return m_content;
    }

    [[nodiscard]] FastVector::ByteVector serialize() override {
        FastVector::ByteVector data;
        append_bytes(data, m_content);
        return data;
    }

    void deserialize(const FastVector::ByteVector& data) override {
        int offset = 0;
        while (offset < data.size())
        {
            m_content += read_bytes(data, offset);
        }

    }

private:
    std::string m_content;
};