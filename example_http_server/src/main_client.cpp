#include <iostream>
#include "HTTPMessage.h"
#include "HTTPClient.h"

int main() {
    HTTPClient client("chat_client_config.json");
    client.connect("127.0.0.1", "8080");
    try {
        auto future = client.get("http://example.com/chat");
        auto response = future.get();

        std::cout << "Response: HTTP/" << response.getVersion() << " "
                  << response.getStatusCode() << " " << response.getStatusMessage() << std::endl;

        for (const auto& [key, value] : response.getHeader().getHeaders()) {
            std::cout << key << ": " << value << std::endl;
        }

        std::cout << "Body: " << response.getBody().getContent() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    client.disconnect();
    return 0;
}