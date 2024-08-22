//
// Created by maxim on 19.08.2024.
//

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <utility>
#include <queue>
#include <memory>
#include <type_traits>
#include <stdexcept>

#include <cstdint>

namespace NetworkMessages
{
    class BinaryData
    {
    public:
        using byte = std::uint8_t;

        virtual ~BinaryData() = default;

        // Serialize the message to a byte vector
        [[nodiscard]] virtual std::vector<byte> serialize() const = 0;

        // Deserialize from a byte vector
        virtual void deserialize(const std::vector<byte> &data) = 0;

        virtual int ByteSize() const = 0;

        template<typename T>
        static void to_network_order(T& value) {
            if (!is_little_endian()) {
                char* p = reinterpret_cast<char*>(&value);
                std::reverse(p, p + sizeof(T));
            }
        }

        template<typename T>
        static void from_network_order(T& value) {
            if (!is_little_endian()) {
                char* p = reinterpret_cast<char*>(&value);
                std::reverse(p, p + sizeof(T));
            }
        }

        static bool is_little_endian() {
            static const int32_t i = 1;
            return *reinterpret_cast<const int8_t*>(&i) == 1;
        }
    protected:
        // Serialization helpers
        template<typename T>
        static std::vector<byte> to_bytes(const T& object) {
            if constexpr (std::is_same_v<T, std::string>) {
                return string_to_bytes(object);
            } else {
                static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
                std::vector<byte> bytes(sizeof(T));
                T network_object = object;
                to_network_order(network_object);
                const byte* begin = reinterpret_cast<const byte*>(std::addressof(network_object));
                const byte* end = begin + sizeof(T);
                std::copy(begin, end, std::begin(bytes));
                return bytes;
            }
        }

        template<typename T>
        static T from_bytes(const std::vector<byte>& bytes) {
            if constexpr (std::is_same_v<T, std::string>) {
                return string_from_bytes(bytes);
            } else {
                static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
                T object;
                std::memcpy(&object, bytes.data(), sizeof(T));
                from_network_order(object);
                return object;
            }
        }


        static void append_bytes(std::vector<byte> &vec, const std::vector<byte> &data)
        {
            vec.insert(vec.end(), data.begin(), data.end());
        }

        template<typename T>
        static void append_bytes(std::vector<byte> &vec, const T &data)
        {
            auto bytes = to_bytes(data);
            vec.insert(vec.end(), bytes.begin(), bytes.end());
        }

        template<typename T>
        static T read_bytes(const std::vector<byte> &data, size_t &offset)
        {
            if (offset + sizeof(T) > data.size())
            {
                throw std::runtime_error("Not enough data to read");
            }
            T value = from_bytes<T>(std::vector<byte>(data.begin() + offset, data.begin() + offset + sizeof(T)));
            from_network_order(value);  // Convert from network byte order to host byte order
            offset += sizeof(T);
            return value;
        }

        static std::string read_bytes(const std::vector<byte>& data, size_t& offset) {
            if (offset + sizeof(uint32_t) > data.size()) {
                throw std::runtime_error("Not enough data to read string length");
            }

            uint32_t network_length;
            std::memcpy(&network_length, &data[offset], sizeof(uint32_t));
            from_network_order(network_length);
            offset += sizeof(uint32_t);

            if (offset + network_length > data.size()) {
                throw std::runtime_error("Not enough data to read string content");
            }

            std::string value(data.begin() + offset, data.begin() + offset + network_length);
            offset += network_length;
            return value;
        }



    private:

        static std::vector<byte> string_to_bytes(const std::string& str) {
            std::vector<byte> bytes;
            uint32_t length = static_cast<uint32_t>(str.length());
            bytes.reserve(sizeof(uint32_t) + length);

            // Directly append the length in network byte order
            uint32_t network_length = length;
            to_network_order(network_length);
            const byte* length_bytes = reinterpret_cast<const byte*>(&network_length);
            bytes.insert(bytes.end(), length_bytes, length_bytes + sizeof(uint32_t));

            // Append the string content
            bytes.insert(bytes.end(), str.begin(), str.end());
            return bytes;
        }

        static std::string string_from_bytes(const std::vector<byte>& bytes) {
            if (bytes.size() < sizeof(uint32_t)) {
                throw std::runtime_error("Invalid byte array for string deserialization");
            }

            // Read the length
            uint32_t network_length;
            std::memcpy(&network_length, bytes.data(), sizeof(uint32_t));
            from_network_order(network_length);

            if (bytes.size() < sizeof(uint32_t) + network_length) {
                throw std::runtime_error("Byte array too short for string deserialization");
            }

            // Extract the string content
            return std::string(bytes.begin() + sizeof(uint32_t), bytes.begin() + sizeof(uint32_t) + network_length);
        }
    };

    class MessageTypeData : BinaryData
    {
    public:
        short Type{};

        [[nodiscard]] std::vector<byte> serialize() const override
        {
            std::vector<byte> data;
            append_bytes(data, Type);
            return data;
        }

        void deserialize(const std::vector<byte> &data) override
        {
            size_t offset = 0;
            Type = read_bytes<short>(data, offset);
        }


        int ByteSize() const override
        {
            return 2;
        }

    };

    template<typename T>
    class BinaryMessage : public BinaryData
    {
    public:
        BinaryMessage(short messageType, const T &payload)
                : messageType(messageType), messagePayload(payload)
        {
            static_assert(std::is_base_of_v<BinaryData, T>, "T must inherit from BinaryData");
        }

        std::vector<byte> serialize() const override
        {
            std::vector<byte> data;
            MessageTypeData typeData;
            typeData.Type = messageType;
            auto messageTypeData = typeData.serialize();
            data.insert(data.end(), messageTypeData.begin(), messageTypeData.end());
            auto payloadData = messagePayload.serialize();
            data.insert(data.end(), payloadData.begin(), payloadData.end());
            return data;
        }

        void deserialize(const std::vector<byte> &data) override
        {
            if (data.size() < sizeof(short))
            {
                throw std::runtime_error("Invalid data: too short to contain message type");
            }
            MessageTypeData typeData;
            typeData.deserialize(data);
            messageType = typeData.Type;
            std::vector<byte> payloadData(data.begin() + sizeof(short), data.end());
            messagePayload.deserialize(payloadData);
        }
        int ByteSize() const override
        {
            return 2 + messagePayload.ByteSize();
        }

        short getMessageType() const
        { return messageType; }

        const T &getPayload() const
        { return messagePayload; }

    private:
        short messageType;  // Stored in network byte order
        T messagePayload;
    };

    class Error : BinaryData
    {
    public:
        std::string ErrorMessage;
        int ByteSize() const override
        {
            return 4 + (int)ErrorMessage.size();
        }
        std::vector<byte> serialize() const override
        {
            std::vector<byte> data;
            append_bytes(data, ErrorMessage);

            return data;
        }

        void deserialize(const std::vector<byte> &data) override
        {
            size_t offset = 0;
            ErrorMessage = read_bytes(data, offset);
        }
    };
    template<typename T>
    std::unique_ptr<BinaryMessage<T>> createMessage(short type, const T &payload)
    {
        return std::make_unique<BinaryMessage<T>>(static_cast<short>(type), payload);
    }
}