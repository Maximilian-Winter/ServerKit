#pragma once

#include "TCPClientBase.h"
#include "ChatMessage.h"
#include "MessageHandler.h"
#include <iostream>
#include <thread>

class ChatClient : public TCPClientBase {
public:
    explicit ChatClient(const std::string& config_file)
            : TCPClientBase(config_file), m_username()
    {
        m_username = m_config.get<std::string>("user_name", "Unknown");
        MessageFactory::loadDefinitions("chat_messages.json");
        m_messageHandler.registerHandler(0, [this](const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::vector<uint8_t>& data) {
            handleChatMessage(data);
        });
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
        m_messageHandler.handleMessage(m_session, data);
    }

    void onConnected() override {
        TCPClientBase::onConnected();

        std::cout << "Type your messages or 'quit' to exit." << std::endl;
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
    void handleChatMessage(const std::vector<uint8_t>& data) {
        try {
            auto message = MessageFactory::createMessage("ChatMessage");
            message->deserialize(data);

            const auto& payload = message->getPayload();
            std::cout << payload.get<std::string>("username") << ": " << payload.get<std::string>("message") << std::endl;
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling chat message: %s", e.what());
        }
    }

    void sendChatMessage(const std::string& message) {
        auto chatMessage = MessageFactory::createMessage("ChatMessage");
        chatMessage->getPayload().set("username", m_username);
        chatMessage->getPayload().set("message", message);
        sendMessage(chatMessage->serialize());
    }

    std::string m_username;
    TCPMessageHandler m_messageHandler;
};