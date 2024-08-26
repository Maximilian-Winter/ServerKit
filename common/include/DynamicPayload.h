#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <utility>
#include <variant>
#include <fstream>
#include "BinaryData.h"

using json = nlohmann::json;
namespace JSONPayload
{
class DynamicPayload : public NetworkMessages::BinaryData {
public:
    using FieldValue = std::variant<std::string, int, float>;
    using byte = uint8_t;

    DynamicPayload() = default;
    DynamicPayload(
            std::function<ByteVector(const std::vector<FieldValue>&)> serializeFunc,
            std::function<void(const ByteVector&, std::vector<FieldValue>&)> deserializeFunc,
            size_t expectedFieldCount
    ) : compiledSerialize(std::move(serializeFunc)), compiledDeserialize(std::move(deserializeFunc)) {
        fields.reserve(expectedFieldCount);
    }

    [[nodiscard]] ByteVector serialize() const override {
        return compiledSerialize(fields);
    }

    void deserialize(const ByteVector& data, size_t& offset) override {
        compiledDeserialize(data, fields);
    }

    template<typename T>
    void set(T&& value) {
        fields.emplace_back(std::forward<T>(value));
    }

    template<typename T>
    const T& get(const int& key) const {
        return std::get<T>(fields.at(key));
    }

private:
    std::vector<FieldValue> fields;
    std::function<ByteVector(const std::vector<FieldValue>&)> compiledSerialize;
    std::function<void(const ByteVector&, std::vector<FieldValue>&)> compiledDeserialize;
};

class MessageFactory {
public:
    struct CompiledMessage {
        json definition;
        short type;
        std::function<FastVector::ByteVector(const std::vector<DynamicPayload::FieldValue>&)> compiledSerialize;
        std::function<void(const FastVector::ByteVector&, std::vector<DynamicPayload::FieldValue>&)> compiledDeserialize;
        uint32_t fieldCount;
    };

    static void loadDefinitions(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        json j;
        file >> j;

        for (auto&& [name, def] : j.items()) {
            auto compiledMessage = compileMessage(def);
            definitions.emplace(name, std::move(compiledMessage));
        }
    }

    static std::shared_ptr<NetworkMessages::BinaryMessage<DynamicPayload>> createMessage(const std::string& name) {
        auto it = definitions.find(name);
        if (it == definitions.end()) {
            throw std::runtime_error("Message definition not found: " + name);
        }

        const auto& compiledMessage = it->second;

        auto payload = DynamicPayload(compiledMessage.compiledSerialize, compiledMessage.compiledDeserialize, compiledMessage.fieldCount);
        return std::make_shared<NetworkMessages::BinaryMessage<DynamicPayload>>(compiledMessage.type, payload);
    }

private:
    static CompiledMessage compileMessage(const json& definition) {
        struct FieldInfo {
            int key;
            std::function<void(FastVector::ByteVector&, const DynamicPayload::FieldValue&)> serialize;
            std::function<void(const FastVector::ByteVector&, size_t&, DynamicPayload::FieldValue&)> deserialize;
        };
        short type = definition["type"].get<short>();
        std::vector<FieldInfo> fieldInfos;
        size_t totalSize = 0;
        int fieldIndex = 0;

        for (const auto& [key, def_type] : definition["fields"].items()) {
            if (def_type == "string") {
                fieldInfos.push_back({
                                             fieldIndex,
                                             [](FastVector::ByteVector& data, const DynamicPayload::FieldValue& value) {
                                                 NetworkMessages::BinaryData::append_bytes(data, std::get<std::string>(value));
                                             },
                                             [](const FastVector::ByteVector& data, size_t& offset, DynamicPayload::FieldValue& value) {
                                                 value = NetworkMessages::BinaryData::read_bytes<std::string>(data, offset);
                                             },

                                     });
                totalSize += 20;
            } else if (def_type == "int") {
                fieldInfos.push_back({
                                             fieldIndex,
                                             [](FastVector::ByteVector& data, const DynamicPayload::FieldValue& value) {
                                                 NetworkMessages::BinaryData::append_bytes(data, std::get<int>(value));
                                             },
                                             [](const FastVector::ByteVector& data, size_t& offset, DynamicPayload::FieldValue& value) {
                                                 value = NetworkMessages::BinaryData::read_bytes<int>(data, offset);
                                             },

                                     });
                totalSize += sizeof(int);
            } else if (def_type == "float") {
                fieldInfos.push_back({
                                             fieldIndex,
                                             [](FastVector::ByteVector& data, const DynamicPayload::FieldValue& value) {
                                                 NetworkMessages::BinaryData::append_bytes(data, std::get<float>(value));
                                             },
                                             [](const FastVector::ByteVector& data, size_t& offset, DynamicPayload::FieldValue& value) {
                                                 value = NetworkMessages::BinaryData::read_bytes<float>(data, offset);
                                             },

                                     });
                totalSize += sizeof(float);
            }
            fieldIndex++;
        }

        auto compiledSerialize = [fieldInfos, totalSize](const std::vector<DynamicPayload::FieldValue>& field_data) {
            FastVector::ByteVector data;
            data.reserve(totalSize);  // Pre-allocate for fixed-size fields
            for (const auto& fieldInfo : fieldInfos) {
                fieldInfo.serialize(data, field_data.at(fieldInfo.key));
            }
            return data;
        };

        auto compiledDeserialize = [fieldInfos](const FastVector::ByteVector& data, std::vector<DynamicPayload::FieldValue>& field_data) {
            size_t offset = 0;
            field_data.reserve(fieldInfos.size());
            for (const auto& fieldInfo : fieldInfos) {
                DynamicPayload::FieldValue value;
                fieldInfo.deserialize(data, offset, value);
                field_data.emplace_back(std::move(value));
            }
        };

        return CompiledMessage{definition, type, compiledSerialize, compiledDeserialize, static_cast<uint32_t>(fieldInfos.size())};
    }

    static inline std::unordered_map<std::string, CompiledMessage> definitions;
};

// Helper function to create and populate messages
template<typename... Args>
std::shared_ptr<NetworkMessages::BinaryMessage<DynamicPayload>> createMessage(const std::string& name, Args&&... args) {
    auto msg = MessageFactory::createMessage(name);
    auto& payload = msg->getPayload();
    populatePayload(payload, std::forward<Args>(args)...);
    return msg;
}

// Helper function to populate payload fields
template<typename T, typename... Args>
void populatePayload(DynamicPayload& payload, T&& value, Args&&... args) {
    payload.set(std::forward<T>(value));
    if constexpr (sizeof...(args) > 0) {
        populatePayload(payload, std::forward<Args>(args)...);
    }
}
}