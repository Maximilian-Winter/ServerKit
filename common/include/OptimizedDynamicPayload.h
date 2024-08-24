//
// Created by maxim on 24.08.2024.
//

#include <vector>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <utility>
#include <variant>
#include <fstream>
#include "BinaryData.h"

namespace Optimized
{
    using json = nlohmann::json;

    class OptimizedDynamicPayload : public NetworkMessages::BinaryData {
    public:
        using FieldValue = std::variant<std::string, int, float>;
        using byte = uint8_t;

        OptimizedDynamicPayload() = default;
        explicit OptimizedDynamicPayload(nlohmann::json  definition) : definition(std::move(definition)) {
            compileSerialize();
            compileDeserialize();
        }

        [[nodiscard]] std::vector<byte> serialize() override {
            return compiledSerialize(fields);
        }

        void deserialize(const std::vector<byte>& data) override {
            compiledDeserialize(data, fields);
        }

        template<typename T>
        void set(const std::string& key, const T& value) {
            fields[key] = value;
        }

        template<typename T>
        T get(const std::string& key) const {
            return std::get<T>(fields.at(key));
        }

    private:
        nlohmann::json definition;
        std::unordered_map<std::string, FieldValue> fields;
        std::function<std::vector<byte>(const std::unordered_map<std::string, FieldValue>&)> compiledSerialize;
        std::function<void(const std::vector<byte>&, std::unordered_map<std::string, FieldValue>&)> compiledDeserialize;

        void compileSerialize() {
            std::vector<std::pair<std::string, std::function<void(std::vector<byte>&, const FieldValue&)>>> serializers;

            for (const auto& [key, type] : definition["fields"].items()) {
                if (type == "string") {
                    serializers.emplace_back(key, [](std::vector<byte>& data, const FieldValue& value) {
                        NetworkMessages::BinaryData::append_bytes(data, std::get<std::string>(value));
                    });
                } else if (type == "int") {
                    serializers.emplace_back(key, [](std::vector<byte>& data, const FieldValue& value) {
                        NetworkMessages::BinaryData::append_bytes(data, std::get<int>(value));
                    });
                } else if (type == "float") {
                    serializers.emplace_back(key, [](std::vector<byte>& data, const FieldValue& value) {
                        NetworkMessages::BinaryData::append_bytes(data, std::get<float>(value));
                    });
                }
            }

            compiledSerialize = [serializers](const std::unordered_map<std::string, FieldValue>& field_data) {
                std::vector<byte> data;
                for (const auto& [key, serializer] : serializers) {
                    serializer(data, field_data.at(key));
                }
                return data;
            };
        }

        void compileDeserialize() {
            std::vector<std::pair<std::string, std::function<void(const std::vector<byte>&, size_t&, FieldValue&)>>> deserializers;

            for (const auto& [key, type] : definition["fields"].items()) {
                if (type == "string") {
                    deserializers.emplace_back(key, [](const std::vector<byte>& data, size_t& offset, FieldValue& value) {
                        value = NetworkMessages::BinaryData::read_bytes<std::string>(data, offset);
                    });
                } else if (type == "int") {
                    deserializers.emplace_back(key, [](const std::vector<byte>& data, size_t& offset, FieldValue& value) {
                        value = NetworkMessages::BinaryData::read_bytes<int>(data, offset);
                    });
                } else if (type == "float") {
                    deserializers.emplace_back(key, [](const std::vector<byte>& data, size_t& offset, FieldValue& value) {
                        value = NetworkMessages::BinaryData::read_bytes<float>(data, offset);
                    });
                }
            }

            compiledDeserialize = [deserializers](const std::vector<byte>& data, std::unordered_map<std::string, FieldValue>& field_data) {
                size_t offset = 0;
                for (const auto& [key, deserializer] : deserializers) {
                    FieldValue value;
                    deserializer(data, offset, value);
                    field_data[key] = value;
                }
            };
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

        static std::unique_ptr<NetworkMessages::BinaryMessage<OptimizedDynamicPayload>> createMessage(const std::string& name) {
            if (definitions.find(name) == definitions.end()) {
                throw std::runtime_error("Message definition not found: " + name);
            }

            const auto& def = definitions[name];
            short type = def["type"].get<short>();
            return std::make_unique<NetworkMessages::BinaryMessage<OptimizedDynamicPayload>>(type, OptimizedDynamicPayload(def));
        }

    private:
        static inline std::unordered_map<std::string, json> definitions;
    };

// Helper function to create and populate messages
    template<typename... Args>
    std::unique_ptr<NetworkMessages::BinaryMessage<OptimizedDynamicPayload>> createMessage(const std::string& name, Args&&... args) {
        auto msg = MessageFactory::createMessage(name);
        auto& payload = msg->getPayload();
        populatePayload(payload, std::forward<Args>(args)...);
        return msg;
    }

// Helper function to populate payload fields
    template<typename T, typename... Args>
    void populatePayload(OptimizedDynamicPayload& payload, const std::string& key, const T& value, Args&&... args) {
        payload.set(key, value);
        if constexpr (sizeof...(args) > 0) {
            populatePayload(payload, std::forward<Args>(args)...);
        }
    }

}
