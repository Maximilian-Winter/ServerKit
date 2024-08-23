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

        [[nodiscard]] std::vector<byte> serialize() override {
            reset_byte_size();
            std::vector<byte> data;
            append_bytes(data, username);
            append_bytes(data, message);
            return data;
        }

        void deserialize(const std::vector<byte>& data) override {
            size_t offset = 0;
            username = read_bytes<std::string>(data, offset);
            message = read_bytes<std::string>(data, offset);
        }

    };

}  // namespace NetworkMessages
