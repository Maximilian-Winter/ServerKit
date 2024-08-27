//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "BinaryData.h"
#include <string>
#include <utility>

namespace NetworkMessages {

    class ChatMessage : public BinaryData {
    public:
        std::string username;
        std::string message;

        ChatMessage() = default;
        ChatMessage(std::string username, std::string message)
                : username(std::move(username)), message(std::move(message)) {}

        [[nodiscard]] ByteVector serialize() const override {
            ByteVector data;
            append_bytes(data, username);
            append_bytes(data, message);
            return data;
        }

        void deserialize(const ByteVector& data, size_t& offset) override {
            username = read_bytes<std::string>(data, offset);
            message = read_bytes<std::string>(data, offset);
        }

    };

}  // namespace NetworkMessages
