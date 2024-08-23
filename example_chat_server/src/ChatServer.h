#pragma once

#include "TCPServerBase.h"
#include "ChatMessage.h"
#include "MessageHandler.h"
#include <iostream>

class ChatServer : public TCPServerBase {
public:
    explicit ChatServer(const std::string& config_file) : TCPServerBase(config_file) {
        m_messageHandler.registerHandler(0, [this](const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::vector<uint8_t>& data) {
            handleChatMessage(session, data);
        });
    }

protected:
    void handleMessage(const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::vector<uint8_t>& data) override {
        m_messageHandler.handleMessage(session, data);
    }

private:
    void handleChatMessage(const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::vector<uint8_t>& data) {
        try {
            auto message = NetworkMessages::BinaryMessage<NetworkMessages::ChatMessage>(0, NetworkMessages::ChatMessage());
            message.deserialize(data);

            const auto& chatMessage = message.getPayload();
            LOG_INFO("Received message from %s (Session UUID: %s): %s",
                     chatMessage.username.c_str(),
                     session->getConnectionUuid().c_str(),
                     chatMessage.message.c_str());

            // Broadcast the message to all clients
            broadcastMessage(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling chat message: %s", e.what());
        }
    }

    void onClientConnected(const std::shared_ptr<TCPNetworkUtility::Session>& session) override {
        TCPServerBase::onClientConnected(session);

        LOG_INFO("New client connected. Session UUID: %s", session->getConnectionUuid().c_str());

        // Send a welcome message to the new client
        NetworkMessages::ChatMessage welcomeMessage("Server", "Welcome to the chat server!");
        auto binaryMessage = NetworkMessages::createMessage(0, welcomeMessage);
        session->write(binaryMessage->serialize());
    }

    void onClientDisconnected(const std::shared_ptr<TCPNetworkUtility::Session>& session) override {
        TCPServerBase::onClientDisconnected(session);

        LOG_INFO("Client disconnected. Session UUID: %s", session->getConnectionUuid().c_str());

        // You could implement additional logic here, such as notifying other clients
        // that a user has left the chat.
    }

    TCPMessageHandler m_messageHandler;
};