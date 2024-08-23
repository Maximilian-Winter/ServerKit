#include "HTTPMessage.h"
#include "HTTPServer.h"

int main() {
    HTTPServer server("chat_server_config.json");

    server.setRequestHandler(HTTPHeader::Method::GET_METHOD, "/", [](const HTTPMessage& request) {
        HTTPMessage response;
        response.setVersion("HTTP/1.1");
        response.setStatusCode(200);
        response.setStatusMessage("OK");
        response.addHeader("Content-Type", "text/plain");

        std::string body = "Hello, World!";
        HTTPBody httpBody;
        httpBody.setContent(body);
        response.setBody(httpBody);
        httpBody.serialize();
        response.addHeader("Content-Length", std::to_string(httpBody.ByteSize()));
        return response;
    });

    server.start();

    std::cin.get();

    server.stop();
    return 0;
}