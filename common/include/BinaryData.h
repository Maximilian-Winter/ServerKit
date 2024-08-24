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
        [[nodiscard]] virtual std::vector<byte> serialize() = 0;

        // Deserialize from a byte vector
        virtual void deserialize(const std::vector<byte> &data) = 0;


    protected:
        // Serialization helpers
        template<typename T>
        static void append_bytes(std::vector<byte> &vec, const T &data)
        {
            if constexpr (std::is_same_v<T, std::vector<byte>>) {
                append_byte_vector(vec, data);
            } else
            {
                auto bytes = to_bytes(data);
                vec.insert(vec.end(), bytes.begin(), bytes.end());
            }

        }

        template<typename T>
        static T read_bytes(const std::vector<byte>& data, size_t& offset)
        {
            if constexpr (std::is_same_v<T, std::string>) {
                return read_string_from_bytes(data, offset);
            }
            if (offset + sizeof(T) > data.size())
            {
                throw std::runtime_error("Not enough data to read");
            }

            // Check if the offset and sizeof(T) are too large for the system's ptrdiff_t
            if (offset + sizeof(T) > static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()))
            {
                throw std::runtime_error("Data size too large for this system");
            }

            // Use ptrdiff_t for iterator arithmetic
            auto start = data.begin() + static_cast<std::ptrdiff_t>(offset);
            auto end = start + static_cast<std::ptrdiff_t>(sizeof(T));

            T value = from_bytes<T>(std::vector<byte>(start, end));
            from_network_order(value);  // Convert from network byte order to host byte order
            offset += sizeof(T);
            return value;
        }

    private:
        // Convert network order helpers
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

        static void append_byte_vector(std::vector<byte> &vec, const std::vector<byte> &data)
        {
            vec.insert(vec.end(), data.begin(), data.end());
        }

        static std::vector<byte> string_to_bytes(const std::string& str) {
            std::vector<byte> bytes;
            size_t utf8_length = 0;

            // First pass: calculate UTF-8 length
            for (char32_t c : str) {
                if (c <= 0x7F) utf8_length += 1;
                else if (c <= 0x7FF) utf8_length += 2;
                else if (c <= 0xFFFF) utf8_length += 3;
                else utf8_length += 4;
            }

            // Reserve space for length and UTF-8 data
            bytes.reserve(sizeof(uint32_t) + utf8_length);

            // Store the length of the UTF-8 string
            auto network_length = static_cast<uint32_t>(utf8_length);
            to_network_order(network_length);
            const byte* length_bytes = reinterpret_cast<const byte*>(&network_length);
            bytes.insert(bytes.end(), length_bytes, length_bytes + sizeof(uint32_t));

            // Second pass: encode to UTF-8
            for (char32_t c : str) {
                if (c <= 0x7F) {
                    bytes.push_back(static_cast<byte>(c));
                } else if (c <= 0x7FF) {
                    bytes.push_back(static_cast<byte>(0xC0 | (c >> 6)));
                    bytes.push_back(static_cast<byte>(0x80 | (c & 0x3F)));
                } else if (c <= 0xFFFF) {
                    bytes.push_back(static_cast<byte>(0xE0 | (c >> 12)));
                    bytes.push_back(static_cast<byte>(0x80 | ((c >> 6) & 0x3F)));
                    bytes.push_back(static_cast<byte>(0x80 | (c & 0x3F)));
                } else {
                    bytes.push_back(static_cast<byte>(0xF0 | (c >> 18)));
                    bytes.push_back(static_cast<byte>(0x80 | ((c >> 12) & 0x3F)));
                    bytes.push_back(static_cast<byte>(0x80 | ((c >> 6) & 0x3F)));
                    bytes.push_back(static_cast<byte>(0x80 | (c & 0x3F)));
                }
            }

            return bytes;
        }

        static std::string read_string_from_bytes(const std::vector<byte>& data, size_t& offset) {
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

            std::string result;
            size_t end = offset + network_length;
            while (offset < end) {
                char32_t codepoint = 0;
                byte first_byte = data[offset++];

                if ((first_byte & 0x80) == 0) {
                    codepoint = first_byte;
                } else if ((first_byte & 0xE0) == 0xC0) {
                    if (offset >= end) throw std::runtime_error("Invalid UTF-8 sequence");
                    codepoint = ((first_byte & 0x1F) << 6) | (data[offset++] & 0x3F);
                } else if ((first_byte & 0xF0) == 0xE0) {
                    if (offset + 1 >= end) throw std::runtime_error("Invalid UTF-8 sequence");
                    codepoint = ((first_byte & 0x0F) << 12) |
                                ((data[offset++] & 0x3F) << 6) |
                                (data[offset++] & 0x3F);
                } else if ((first_byte & 0xF8) == 0xF0) {
                    if (offset + 2 >= end) throw std::runtime_error("Invalid UTF-8 sequence");
                    codepoint = ((first_byte & 0x07) << 18) |
                                ((data[offset++] & 0x3F) << 12) |
                                ((data[offset++] & 0x3F) << 6) |
                                (data[offset++] & 0x3F);
                } else {
                    throw std::runtime_error("Invalid UTF-8 sequence");
                }

                result += static_cast<char>(codepoint);
            }

            return result;
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
            return {bytes.begin() + sizeof(uint32_t), bytes.begin() + sizeof(uint32_t) + network_length};
        }
    };

    class MessageTypeData : BinaryData
    {
    public:
        short Type{};

        [[nodiscard]] std::vector<byte> serialize() override
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

        [[nodiscard]] std::vector<byte> serialize() override
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

        [[nodiscard]] short getMessageType() const
        { return messageType; }

         T &getPayload()
        { return messagePayload; }

    private:
        short messageType;  // Stored in network byte order
        T messagePayload;
    };

    class Error : BinaryData
    {
    public:
        std::string ErrorMessage;

        [[nodiscard]] std::vector<byte> serialize() override
        {
            std::vector<byte> data;
            append_bytes(data, ErrorMessage);
            return data;
        }

        void deserialize(const std::vector<byte> &data) override
        {
            size_t offset = 0;
            ErrorMessage = read_bytes<std::string>(data, offset);
        }

    };

    template<typename T>
    std::unique_ptr<BinaryMessage<T>> createMessage(short type, const T &payload)
    {
        return std::make_unique<BinaryMessage<T>>(type, payload);
    }
}