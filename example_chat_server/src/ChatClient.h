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
        m_messageHandler.registerHandler(0, [this](const std::shared_ptr<TCPNetworkUtility::Session>& session, const FastVector::ByteVector& data) {
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
    void handleMessage(const FastVector::ByteVector& data) override {
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
    void handleChatMessage(const FastVector::ByteVector& data) {
        LOG_DEBUG("handleMessage called. Data size: %zu", data.size());
        try {
            auto message = NetworkMessages::BinaryMessage<NetworkMessages::ChatMessage>(0, NetworkMessages::ChatMessage());
            size_t offset = 0;
            message.deserialize(data, offset);

            const auto& chatMessage = message.getPayload();
            std::cout << chatMessage.username << ": " << chatMessage.message << std::endl;
            LOG_DEBUG("Message processed: %s: %s", chatMessage.username.c_str(), chatMessage.message.c_str());
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling chat message: %s", e.what());
        }
    }

    void sendChatMessage(const std::string& message) {
        NetworkMessages::ChatMessage chatMessage(m_username, message);
        auto binaryMessage = NetworkMessages::MessageFactory::createMessage(0, chatMessage);
        sendMessage(binaryMessage->serialize());
    }

    std::string m_username;
    TCPMessageHandler m_messageHandler;
};