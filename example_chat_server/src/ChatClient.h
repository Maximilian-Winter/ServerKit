//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "TCPClientBase.h"
#include "ChatMessage.h"
#include <iostream>
#include <thread>

class ChatClient : public TCPClientBase {
public:
    ChatClient(const std::string& config_file)
            : TCPClientBase(config_file), m_username()
            {
                m_username = m_config.get<std::string> ("user_name", "Unknown");
            }

    void run() {
        connect();
        while (true) {
            std::string input;
            std::getline(std::cin, input);
            if (input == "quit") {
                disconnect();
                break;
            }
            if (!input.empty()) {
                m_thread_pool->get_io_context().post([this, input]() {
                    sendChatMessage(input);
                });
            }
        }
    }

protected:
    void handleMessage(const std::vector<uint8_t>& data) override {
        try {
            auto message = NetworkMessages::BinaryMessage<NetworkMessages::ChatMessage>(0, NetworkMessages::ChatMessage());
            message.deserialize(data);

            const auto& chatMessage = message.getPayload();
            std::cout << chatMessage.username << ": " << chatMessage.message << std::endl;
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling message: %s", e.what());
        }
    }

    void onConnected() override {
        TCPClientBase::onConnected();
        std::cout << "Connected to chat server. Type your messages or 'quit' to exit." << std::endl;
    }

    void onDisconnected() override {
        TCPClientBase::onDisconnected();
        std::cout << "Disconnected from chat server." << std::endl;
    }

    void onConnectionError(const std::error_code& ec) override {
        TCPClientBase::onConnectionError(ec);
        std::cout << "Failed to connect to chat server: " << ec.message() << std::endl;
    }

private:
    void sendChatMessage(const std::string& message) {
        NetworkMessages::ChatMessage chatMessage(m_username, message);
        auto binaryMessage = NetworkMessages::createMessage(0, chatMessage);
        sendMessage(binaryMessage->serialize());
    }

    std::string m_username;
};
