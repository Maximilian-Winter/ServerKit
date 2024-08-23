#include "HTTPMessage.h"
#include "HTTPServer.h"

int main() {
    HTTPServer server("chat_server_config.json");

    server.setRequestHandler(HTTPMessage::Method::GET_METHOD, "/", [](const HTTPMessage& request) {
        HTTPMessage response(HTTPMessage::Type::RESPONSE);
        response.setVersion("HTTP/1.1");
        response.setStatusCode(200);
        response.setStatusMessage("OK");
        response.addHeader("Content-Type", "text/plain");

        std::string body = "Hello, World!";
        HTTPBody httpBody;
        httpBody.setContent(body);
        response.setBody(httpBody);

        response.addHeader("Content-Length", std::to_string(body.length()));
        return response;
    });

    server.start();

    std::cin.get();

    server.stop();
    return 0;
}