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
        using ByteVector = std::vector<byte>;

        virtual ~BinaryData() = default;

        // Serialize the message to a byte vector
        [[nodiscard]] virtual ByteVector serialize() const = 0;

        // Deserialize from a byte vector
        virtual void deserialize(const ByteVector &data) = 0;


    protected:
        // Serialization helpers
        template<typename T>
        static void append_bytes(ByteVector &vec, const T &data)
        {
            if constexpr (std::is_same_v<T, ByteVector>) {
                append_byte_vector(vec, data);
            } else
            {
                auto bytes = to_bytes(data);
                vec.insert(vec.end(), bytes.begin(), bytes.end());
            }

        }

        template<typename T>
        static T read_bytes(const ByteVector& data, size_t& offset)
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

            T value = from_bytes<T>(ByteVector(start, end));
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
        static ByteVector to_bytes(const T& object) {
            if constexpr (std::is_same_v<T, std::string>) {
                return string_to_bytes(object);
            } else {
                static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
                ByteVector bytes(sizeof(T));
                T network_object = object;
                to_network_order(network_object);
                const byte* begin = reinterpret_cast<const byte*>(std::addressof(network_object));
                const byte* end = begin + sizeof(T);
                std::copy(begin, end, std::begin(bytes));
                return bytes;
            }
        }

        template<typename T>
        static T from_bytes(const ByteVector& bytes) {
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

        static void append_byte_vector(ByteVector &vec, const ByteVector &data)
        {
            vec.insert(vec.end(), data.begin(), data.end());
        }

        static ByteVector string_to_bytes(const std::string& str) {
            ByteVector bytes;
            bytes.reserve(sizeof(uint32_t) + str.size() * 4);  // Worst case: all 4-byte UTF-8

            uint32_t utf8_length = 0;
            for (char32_t c : str) {
                if (c <= 0x7F) {
                    utf8_length += 1;
                    bytes.push_back(static_cast<byte>(c));
                } else if (c <= 0x7FF) {
                    utf8_length += 2;
                    bytes.push_back(static_cast<byte>(0xC0 | (c >> 6)));
                    bytes.push_back(static_cast<byte>(0x80 | (c & 0x3F)));
                } else if (c <= 0xFFFF) {
                    utf8_length += 3;
                    bytes.push_back(static_cast<byte>(0xE0 | (c >> 12)));
                    bytes.push_back(static_cast<byte>(0x80 | ((c >> 6) & 0x3F)));
                    bytes.push_back(static_cast<byte>(0x80 | (c & 0x3F)));
                } else {
                    utf8_length += 4;
                    bytes.push_back(static_cast<byte>(0xF0 | (c >> 18)));
                    bytes.push_back(static_cast<byte>(0x80 | ((c >> 12) & 0x3F)));
                    bytes.push_back(static_cast<byte>(0x80 | ((c >> 6) & 0x3F)));
                    bytes.push_back(static_cast<byte>(0x80 | (c & 0x3F)));
                }
            }

            ByteVector result;
            result.reserve(sizeof(uint32_t) + utf8_length);
            append_bytes(result, utf8_length);
            result.insert(result.end(), bytes.begin(), bytes.end());
            return result;
        }

        static std::string read_string_from_bytes(const ByteVector& data, size_t& offset) {
            if (offset + sizeof(uint32_t) > data.size()) {
                throw std::runtime_error("Not enough data to read string length");
            }

            auto utf8_length = read_bytes<uint32_t>(data, offset);

            if (offset + utf8_length > data.size()) {
                throw std::runtime_error("Not enough data to read string content");
            }

            std::string result;
            result.reserve(utf8_length);  // Pre-allocate for efficiency
            const byte* cur = data.data() + offset;
            const byte* end = cur + utf8_length;

            while (cur < end) {
                if ((*cur & 0x80) == 0) {
                    result.push_back(static_cast<char>(*cur++));
                } else if ((*cur & 0xE0) == 0xC0) {
                    if (end - cur < 2) throw std::runtime_error("Invalid UTF-8 sequence");
                    char32_t codepoint = ((cur[0] & 0x1F) << 6) | (cur[1] & 0x3F);
                    result.push_back(static_cast<char>(codepoint));
                    cur += 2;
                } else if ((*cur & 0xF0) == 0xE0) {
                    if (end - cur < 3) throw std::runtime_error("Invalid UTF-8 sequence");
                    char32_t codepoint = ((cur[0] & 0x0F) << 12) | ((cur[1] & 0x3F) << 6) | (cur[2] & 0x3F);
                    result.push_back(static_cast<char>(codepoint));
                    cur += 3;
                } else if ((*cur & 0xF8) == 0xF0) {
                    if (end - cur < 4) throw std::runtime_error("Invalid UTF-8 sequence");
                    char32_t codepoint = ((cur[0] & 0x07) << 18) | ((cur[1] & 0x3F) << 12) |
                                         ((cur[2] & 0x3F) << 6) | (cur[3] & 0x3F);
                    result.push_back(static_cast<char>(codepoint));
                    cur += 4;
                } else {
                    throw std::runtime_error("Invalid UTF-8 sequence");
                }
            }

            offset += utf8_length;
            return result;
        }

        static std::string string_from_bytes(const ByteVector& bytes) {
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

        [[nodiscard]] ByteVector serialize() const override
        {
            ByteVector data;
            append_bytes(data, Type);
            return data;
        }

        void deserialize(const ByteVector &data) override
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

        [[nodiscard]] ByteVector serialize() const override
        {
            ByteVector data;
            MessageTypeData typeData;
            typeData.Type = messageType;
            auto messageTypeData = typeData.serialize();
            data.insert(data.end(), messageTypeData.begin(), messageTypeData.end());
            auto payloadData = messagePayload.serialize();
            data.insert(data.end(), payloadData.begin(), payloadData.end());
            return data;
        }

        void deserialize(const ByteVector &data) override
        {
            if (data.size() < sizeof(short))
            {
                throw std::runtime_error("Invalid data: too short to contain message type");
            }
            MessageTypeData typeData;
            typeData.deserialize(data);
            messageType = typeData.Type;
            ByteVector payloadData(data.begin() + sizeof(short), data.end());
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

        [[nodiscard]] ByteVector serialize() const override
        {
            ByteVector data;
            append_bytes(data, ErrorMessage);
            return data;
        }

        void deserialize(const ByteVector &data) override
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