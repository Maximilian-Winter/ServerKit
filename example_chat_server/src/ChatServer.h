//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "ServerBase.h"
#include "ChatMessage.h"
#include <iostream>

class ChatServer : public ServerBase {
public:
    ChatServer(const std::string& config_file) : ServerBase(config_file) {}

protected:
    void handleMessage(const std::shared_ptr<NetworkUtility::Session>& session, const std::vector<uint8_t>& data) override {
        try {
            auto message = NetworkMessages::BinaryMessage<NetworkMessages::ChatMessage>(0, NetworkMessages::ChatMessage());
            message.deserialize(data);

            const auto& chatMessage = message.getPayload();
            LOG_INFO("Received message from %s: %s", chatMessage.username.c_str(), chatMessage.message.c_str());

            // Broadcast the message to all clients
            broadcastMessage(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling message: %s", e.what());
        }
    }

    void onClientConnected(const std::shared_ptr<NetworkUtility::Session>& session) override {
        ServerBase::onClientConnected(session);

        // Send a welcome message to the new client
        NetworkMessages::ChatMessage welcomeMessage("Server", "Welcome to the chat server!");
        auto binaryMessage = NetworkMessages::createMessage(0, welcomeMessage);
        session->write(binaryMessage->serialize());
    }

    void onClientDisconnected(const std::shared_ptr<NetworkUtility::Session>& session) override {
        ServerBase::onClientDisconnected(session);

        // You could implement additional logic here, such as notifying other clients
        // that a user has left the chat.
    }
};
