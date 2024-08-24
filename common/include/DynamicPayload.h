//
// Created by maxim on 24.08.2024.
//

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <utility>
#include <variant>
#include <fstream>
#include "BinaryData.h"

using json = nlohmann::json;

class DynamicPayload : public NetworkMessages::BinaryData {
public:
    using FieldValue = std::variant<std::string, int, float>;

    DynamicPayload() = default;
    explicit DynamicPayload(json definition) : definition(std::move(definition)) {}

    [[nodiscard]] std::vector<byte> serialize() override {
        std::vector<byte> data;
        for (const auto& [key, type] : definition["fields"].items()) {
            if (fields.find(key) != fields.end()) {
                std::visit([&](auto&& arg) {
                    this->append_bytes(data, arg);
                }, fields[key]);
            }
        }
        return data;
    }

    void deserialize(const std::vector<byte>& data) override {
        size_t offset = 0;
        for (const auto& [key, type] : definition["fields"].items()) {
            if (type == "string") {
                fields[key] = read_bytes<std::string>(data, offset);
            } else if (type == "int") {
                fields[key] = read_bytes<int>(data, offset);
            } else if (type == "float") {
                fields[key] = read_bytes<float>(data, offset);
            }
        }
    }

    template<typename T>
    void set(const std::string& key, T& value) {
        fields[key] = value;
    }

    template<typename T>
    T get(const std::string& key) const {
        return std::get<T>(fields.at(key));
    }

private:
    json definition;
    std::unordered_map<std::string, FieldValue> fields;

    // Helper method to append bytes for variant types
    static void append_bytes(std::vector<byte>& vec, const FieldValue& value) {
        std::visit([&](auto&& arg) {
            BinaryData::append_bytes(vec, arg);
        }, value);
    }
};


class MessageFactory {
public:
    static void loadDefinitions(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        json j;
        file >> j;

        for (const auto& [name, def] : j.items()) {
            definitions[name] = def;
        }
    }

    static std::unique_ptr<NetworkMessages::BinaryMessage<DynamicPayload>> createMessage(const std::string& name) {
        if (definitions.find(name) == definitions.end()) {
            throw std::runtime_error("Message definition not found: " + name);
        }

        const auto& def = definitions[name];
        short type = def["type"].get<short>();
        return std::make_unique<NetworkMessages::BinaryMessage<DynamicPayload>>(type, DynamicPayload(def));
    }

private:
    static inline std::unordered_map<std::string, json> definitions;
};

// Helper function to create and populate messages
template<typename... Args>
std::unique_ptr<NetworkMessages::BinaryMessage<DynamicPayload>> createMessage(const std::string& name, Args&&... args) {
    auto msg = MessageFactory::createMessage(name);
    auto& payload = msg->getPayload();
    populatePayload(payload, std::forward<Args>(args)...);
    return msg;
}

// Helper function to populate payload fields
template<typename T, typename... Args>
void populatePayload(DynamicPayload& payload, const std::string& key, const T& value, Args&&... args) {
    payload.set(key, value);
    if constexpr (sizeof...(args) > 0) {
        populatePayload(payload, std::forward<Args>(args)...);
    }
}
