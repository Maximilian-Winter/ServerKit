//
// Created by maxim on 24.08.2024.
//

#pragma once

#include <nlohmann/json.hpp>

#include "TCPNetworkUtility.h"

using json = nlohmann::json;


// Define message types for our JSON API
enum class MessageType : short {
    JsonRequest = 1,
    JsonResponse = 2,
    Error = 3
};

// Custom BinaryData classes for JSON messages
class JsonMessage : public NetworkMessages::BinaryData {
public:
    json json_data;

    std::vector<byte> serialize() override {
        std::string json_str = json_data.dump();
        std::vector<byte> result;
        NetworkMessages::BinaryData::append_bytes(result, json_str);
        return result;
    }

    void deserialize(const std::vector<byte>& data) override {
        size_t offset = 0;
        auto json_str = NetworkMessages::BinaryData::read_bytes<std::string>(data, offset);
        this->json_data = json::parse(json_str);
    }

};
