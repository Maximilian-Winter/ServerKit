#pragma once

#include "TCPServerBase.h"
#include "ChatMessage.h"
#include "MessageHandler.h"
#include <iostream>

class ChatServer : public TCPServerBase {
public:
    explicit ChatServer(const std::string& config_file) : TCPServerBase(config_file) {
        JSONPayload::MessageFactory::loadDefinitions("chat_messages.json");
        m_messageHandler.registerHandler(0, [this](const std::shared_ptr<TCPNetworkUtility::Session>& session, const FastVector::ByteVector& data) {
            handleChatMessage(session, data);
        });
    }

protected:
    void handleMessage(const std::shared_ptr<TCPNetworkUtility::Session>& session, const FastVector::ByteVector& data) override {
        m_messageHandler.handleMessage(session, data);
    }

private:
    void handleChatMessage(const std::shared_ptr<TCPNetworkUtility::Session>& session, const FastVector::ByteVector& data) {
        try {
            auto message = JSONPayload::MessageFactory::createMessage("ChatMessage");
            message->deserialize(data);

            const auto& payload = message->getPayload();
            LOG_INFO("Received message from %s (Session UUID: %s): %s",
                     payload.get<std::string>("username").c_str(),
                     session->getConnectionId().c_str(),
                     payload.get<std::string>("message").c_str());

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
        auto welcomeMessage = MessageFactory::createMessage("ChatMessage");
        welcomeMessage->getPayload().set("username", "Server");
        welcomeMessage->getPayload().set("message", "Welcome to the chat server!");
        session->write(welcomeMessage->serialize());
    }

    void onClientDisconnected(const std::shared_ptr<TCPNetworkUtility::Session>& session) override {
        TCPServerBase::onClientDisconnected(session);

        LOG_INFO("Client disconnected. Session UUID: %s", session->getConnectionUuid().c_str());

        // You could implement additional logic here, such as notifying other clients
        // that a user has left the chat.
    }

    TCPMessageHandler m_messageHandler;
};