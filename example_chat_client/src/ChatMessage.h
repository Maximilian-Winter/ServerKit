//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "BinaryData.h"
#include <string>

namespace NetworkMessages {

    class ChatMessage : public BinaryData {
    public:
        std::string username;
        std::string message;

        ChatMessage() = default;
        ChatMessage(const std::string& username, const std::string& message)
                : username(username), message(message) {}

        std::vector<byte> serialize() const override {
            std::vector<byte> data;
            append_bytes(data, username);
            append_bytes(data, message);
            return data;
        }

        void deserialize(const std::vector<byte>& data) override {
            size_t offset = 0;
            username = read_bytes(data, offset);
            message = read_bytes(data, offset);
        }

        int ByteSize() const override {
            return 4 + username.size() + 4 + message.size();
        }
    };

}  // namespace NetworkMessages
