//
// Created by maxim on 23.08.2024.
//
#include "TCPServerBase.h"
#include "BinaryData.h"
#include <nlohmann/json.hpp>

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

class JsonApiServer : public TCPServerBase {
public:
    explicit JsonApiServer(const std::string& config_file) : TCPServerBase(config_file) {}

protected:
    void handleMessage(const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::vector<uint8_t>& message) override {
        auto binary_message = std::make_unique<NetworkMessages::BinaryMessage<JsonMessage>>(0, JsonMessage());
        binary_message->deserialize(message);

        auto message_type = static_cast<MessageType>(binary_message->getMessageType());
        const auto& payload = binary_message->getPayload();

        switch (message_type) {
            case MessageType::JsonRequest:
                handleJsonRequest(session, payload.data);
                break;
            default:
                sendErrorResponse(session, "Invalid message type");
                break;
        }
    }

private:
    void handleJsonRequest(const std::shared_ptr<TCPNetworkUtility::Session>& session, const json& request) {
        try {
            // Process the request based on its content
            if (request.contains("action")) {
                std::string action = request["action"];
                if (action == "echo") {
                    sendJsonResponse(session, request);
                } else if (action == "add") {
                    int a = request["a"];
                    int b = request["b"];
                    json response = {
                            {"result", a + b}
                    };
                    sendJsonResponse(session, response);
                } else {
                    sendErrorResponse(session, "Unknown action");
                }
            } else {
                sendErrorResponse(session, "Missing 'action' in request");
            }
        } catch (const std::exception& e) {
            sendErrorResponse(session, std::string("Error processing request: ") + e.what());
        }
    }

    void sendJsonResponse(const std::shared_ptr<TCPNetworkUtility::Session>& session, const json& response) {
        JsonMessage json_message;
        json_message.data = response;
        auto binary_message = NetworkMessages::createMessage(static_cast<short>(MessageType::JsonResponse), json_message);
        session->write(binary_message->serialize());
    }

    void sendErrorResponse(const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::string& error_message) {
        NetworkMessages::Error error;
        error.ErrorMessage = error_message;
        auto binary_message = NetworkMessages::createMessage(static_cast<short>(MessageType::Error), error);
        session->write(binary_message->serialize());
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        JsonApiServer server(argv[1]);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}