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
        explicit OptimizedDynamicPayload(const nlohmann::json& definition) {
            compileSerializeDeserialize(definition);
        }

        [[nodiscard]] ByteVector serialize() const override {
            return compiledSerialize(fields);
        }

        void deserialize(const ByteVector& data) override {
            compiledDeserialize(data, fields);
        }

        template<typename T>
        void set(const std::string& key, T&& value) {
            fields[key] = std::forward<T>(value);
        }

        template<typename T>
        const T& get(const std::string& key) const {
            return std::get<T>(fields.at(key));
        }

    private:
        std::unordered_map<std::string, FieldValue> fields;
        std::function<ByteVector(const std::unordered_map<std::string, FieldValue>&)> compiledSerialize;
        std::function<void(const ByteVector&, std::unordered_map<std::string, FieldValue>&)> compiledDeserialize;

        void compileSerializeDeserialize(const nlohmann::json& definition) {
            struct FieldInfo {
                std::string key;
                std::function<void(ByteVector&, const FieldValue&)> serialize;
                std::function<void(const ByteVector&, size_t&, FieldValue&)> deserialize;
                size_t size;
            };

            std::vector<FieldInfo> fieldInfos;
            size_t totalSize = 0;

            for (const auto& [key, type] : definition["fields"].items()) {
                if (type == "string") {
                    fieldInfos.push_back({
                                                 key,
                                                 [](ByteVector& data, const FieldValue& value) {
                                                     NetworkMessages::BinaryData::append_bytes(data, std::get<std::string>(value));
                                                 },
                                                 [](const ByteVector& data, size_t& offset, FieldValue& value) {
                                                     value = NetworkMessages::BinaryData::read_bytes<std::string>(data, offset);
                                                 },
                                                 0  // Variable size for strings
                                         });
                } else if (type == "int") {
                    fieldInfos.push_back({
                                                 key,
                                                 [](ByteVector& data, const FieldValue& value) {
                                                     NetworkMessages::BinaryData::append_bytes(data, std::get<int>(value));
                                                 },
                                                 [](const ByteVector& data, size_t& offset, FieldValue& value) {
                                                     value = NetworkMessages::BinaryData::read_bytes<int>(data, offset);
                                                 },
                                                 sizeof(int)
                                         });
                    totalSize += sizeof(int);
                } else if (type == "float") {
                    fieldInfos.push_back({
                                                 key,
                                                 [](ByteVector& data, const FieldValue& value) {
                                                     NetworkMessages::BinaryData::append_bytes(data, std::get<float>(value));
                                                 },
                                                 [](const ByteVector& data, size_t& offset, FieldValue& value) {
                                                     value = NetworkMessages::BinaryData::read_bytes<float>(data, offset);
                                                 },
                                                 sizeof(float)
                                         });
                    totalSize += sizeof(float);
                }
            }

            compiledSerialize = [fieldInfos, totalSize](const std::unordered_map<std::string, FieldValue>& field_data) {
                ByteVector data;
                data.reserve(totalSize);  // Pre-allocate for fixed-size fields
                for (const auto& fieldInfo : fieldInfos) {
                    fieldInfo.serialize(data, field_data.at(fieldInfo.key));
                }
                return data;
            };

            compiledDeserialize = [fieldInfos](const ByteVector& data, std::unordered_map<std::string, FieldValue>& field_data) {
                size_t offset = 0;
                for (const auto& fieldInfo : fieldInfos) {
                    FieldValue value;
                    fieldInfo.deserialize(data, offset, value);
                    field_data[fieldInfo.key] = std::move(value);
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

            for (auto&& [name, def] : j.items()) {
                definitions.emplace(name, std::move(def));
            }
        }

        static std::unique_ptr<NetworkMessages::BinaryMessage<OptimizedDynamicPayload>> createMessage(const std::string& name) {
            auto it = definitions.find(name);
            if (it == definitions.end()) {
                throw std::runtime_error("Message definition not found: " + name);
            }

            const auto& def = it->second;
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
    void populatePayload(OptimizedDynamicPayload& payload, std::string key, T&& value, Args&&... args) {
        payload.set(std::move(key), std::forward<T>(value));
        if constexpr (sizeof...(args) > 0) {
            populatePayload(payload, std::forward<Args>(args)...);
        }
    }
}