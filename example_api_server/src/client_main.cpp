//
// Created by maxim on 23.08.2024.
//
#include "TCPClientBase.h"
#include "BinaryData.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <future>

using json = nlohmann::json;

// Define message types for our JSON API (same as in the server)
enum class MessageType : short {
    JsonRequest = 1,
    JsonResponse = 2,
    Error = 3
};

// Custom BinaryData classes for JSON messages (same as in the server)
class JsonMessage : public NetworkMessages::BinaryData {
public:
    json data;

    std::vector<byte> serialize() const override {
        std::string json_str = data.dump();
        std::vector<byte> result;
        NetworkMessages::BinaryData::append_bytes(result, json_str);
        return result;
    }

    void deserialize(const std::vector<byte>& data) override {
        size_t offset = 0;
        std::string json_str = NetworkMessages::BinaryData::read_bytes<std::string>(data, offset);
        this->data = json::parse(json_str);
    }

    [[nodiscard]] int ByteSize() const override {
        return 4 + data.dump().size(); // 4 bytes for string length + JSON string
    }
};

class JsonApiClient : public TCPClientBase {
public:
    explicit JsonApiClient(const std::string& config_file) : TCPClientBase(config_file) {}

    std::future<json> sendRequest(const json& request) {
        auto promise = std::make_shared<std::promise<json>>();
        auto future = promise->get_future();

        JsonMessage json_message;
        json_message.data = request;
        auto binary_message = NetworkMessages::createMessage(static_cast<short>(MessageType::JsonRequest), json_message);
        sendMessage(binary_message->serialize());

        m_pending_requests.push_back(promise);

        return future;
    }

protected:
    void handleMessage(const std::vector<uint8_t>& message) override {
        auto binary_message = std::make_unique<NetworkMessages::BinaryMessage<JsonMessage>>(0, JsonMessage());
        binary_message->deserialize(message);

        auto message_type = static_cast<MessageType>(binary_message->getMessageType());
        const auto& payload = binary_message->getPayload();

        switch (message_type) {
            case MessageType::JsonResponse:
                if (!m_pending_requests.empty()) {
                    m_pending_requests.front()->set_value(payload.data);
                    m_pending_requests.pop_front();
                }
                break;
            case MessageType::Error:
                if (!m_pending_requests.empty()) {
                    m_pending_requests.front()->set_exception(std::make_exception_ptr(std::runtime_error(payload.data["error"].get<std::string>())));
                    m_pending_requests.pop_front();
                }
                break;
            default:
                std::cerr << "Received unexpected message type: " << static_cast<int>(message_type) << std::endl;
                break;
        }
    }

private:
    std::deque<std::shared_ptr<std::promise<json>>> m_pending_requests;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        JsonApiClient client(argv[1]);
        client.connect();

        // Echo request
        json echo_request = {
                {"action", "echo"},
                {"message", "Hello, JSON API!"}
        };
        auto echo_future = client.sendRequest(echo_request);

        // Add request
        json add_request = {
                {"action", "add"},
                {"a", 5},
                {"b", 3}
        };
        auto add_future = client.sendRequest(add_request);

        // Wait for and print results
        try {
            json echo_response = echo_future.get();
            std::cout << "Echo response: " << echo_response.dump(2) << std::endl;

            json add_response = add_future.get();
            std::cout << "Add response: " << add_response.dump(2) << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        client.disconnect();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}