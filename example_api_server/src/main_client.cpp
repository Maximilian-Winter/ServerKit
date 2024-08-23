//
// Created by maxim on 23.08.2024.
//
#include <nlohmann/json.hpp>
#include <iostream>
#include <future>
#include <mutex>

#include "TCPClientBase.h"
#include "BinaryData.h"
#include "JsonMessage.h"

class JsonApiClient : public TCPClientBase {
public:
    explicit JsonApiClient(const std::string& config_file) : TCPClientBase(config_file) {}

    std::future<json> sendRequest(const json& request) {
        auto promise = std::make_shared<std::promise<json>>();
        auto future = promise->get_future();

        JsonMessage json_message;
        json_message.json_data = request;
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

        {
            std::lock_guard<std::mutex> lock(pending_requests_mutex);
            switch (message_type) {
                case MessageType::JsonResponse:
                    if (!m_pending_requests.empty()) {
                        m_pending_requests.front()->set_value(payload.json_data);
                        m_pending_requests.pop_front();
                    }
                    break;
                case MessageType::Error:
                    if (!m_pending_requests.empty()) {
                        m_pending_requests.front()->set_exception(std::make_exception_ptr(std::runtime_error(payload.json_data["error"].get<std::string>())));
                        m_pending_requests.pop_front();
                    }
                    break;
                default:
                    std::cerr << "Received unexpected message type: " << static_cast<int>(message_type) << std::endl;
                    break;
            }
        }
    }

private:
    std::mutex pending_requests_mutex;
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
        std::cout << "API client started. Press Enter to send request." << std::endl;
        std::cin.get();
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