//
// Created by maxim on 23.08.2024.
//

#pragma once

#include "BinaryData.h"
#include "TCPNetworkUtility.h"
#include "UDPNetworkUtility.h"
#include <functional>
#include <unordered_map>
#include <memory>

template<typename EndpointType>
class MessageHandler {
public:
    using MessageCallback = std::function<void(const EndpointType&, const std::vector<uint8_t>&)>;

    void registerHandler(short messageType, MessageCallback callback) {
        m_handlers[messageType] = std::move(callback);
    }

    void handleMessage(const EndpointType& endpoint, const std::vector<uint8_t>& data) {
        try {
            NetworkMessages::MessageTypeData typeData;
            typeData.deserialize(data);
            short messageType = typeData.Type;

            auto it = m_handlers.find(messageType);
            if (it != m_handlers.end()) {
                it->second(endpoint, data);
            } else {
                LOG_ERROR("No handler registered for message type: %d", messageType);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling message: %s", e.what());
        }
    }

private:

    std::unordered_map<short, MessageCallback> m_handlers;
};

// Specialization for TCP (using std::shared_ptr<NetworkUtility::Session> as endpoint)
using TCPMessageHandler = MessageHandler<std::shared_ptr<TCPNetworkUtility::Session>>;

// Specialization for UDP (using std::shared_ptr<UDPNetworkUtility::Session> as endpoint)
using UDPMessageHandler = MessageHandler<std::shared_ptr<UDPNetworkUtility::Session>>;
