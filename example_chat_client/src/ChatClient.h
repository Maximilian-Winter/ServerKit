//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "ClientBase.h"
#include "ChatMessage.h"
#include <iostream>
#include <thread>

class ChatClient : public ClientBase {
public:
    ChatClient(const std::string& config_file, const std::string& username)
            : ClientBase(config_file), m_username(username) {}

    void run() {
        connect();

        std::thread input_thread([this]() {
            std::string input;
            while (m_connected) {
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
        });

        m_thread_pool->run();
        if (input_thread.joinable()) {
            input_thread.join();
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
        ClientBase::onConnected();
        std::cout << "Connected to chat server. Type your messages or 'quit' to exit." << std::endl;
    }

    void onDisconnected() override {
        ClientBase::onDisconnected();
        std::cout << "Disconnected from chat server." << std::endl;
    }

    void onConnectionError(const std::error_code& ec) override {
        ClientBase::onConnectionError(ec);
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
