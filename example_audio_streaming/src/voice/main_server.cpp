//
// Created by maxim on 29.08.2024.
//
#include <iostream>
#include <string>
#include <vector>
#include <asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

using asio::ip::udp;

class VoiceChatServer : public std::enable_shared_from_this<VoiceChatServer> {
public:
    VoiceChatServer(asio::io_context& io_context, short port)
        : socket_(io_context, udp::endpoint(udp::v4(), port)),
          io_context_(io_context) {}

    void start() {
        std::cout << "Voice Chat Server started. Waiting for clients..." << std::endl;
        start_receive();
    }

private:
    void start_receive() {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), remote_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    handle_receive(bytes_recvd);
                    start_receive();
                } else {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                    start_receive();
                }
            });
    }

    void handle_receive(std::size_t bytes_recvd) {
        std::string client_key = remote_endpoint_.address().to_string() + ":" +
                                 std::to_string(remote_endpoint_.port());

        if (clients_.find(client_key) == clients_.end()) {
            std::cout << "New client connected: " << client_key << std::endl;
            clients_[client_key] = remote_endpoint_;
        }

        // Broadcast received audio to all other clients
        for (const auto& client : clients_) {
            if (client.first != client_key) {
                socket_.async_send_to(
                    asio::buffer(recv_buffer_, bytes_recvd), client.second,
                    [this](std::error_code ec, std::size_t bytes_sent) {
                        if (ec) {
                            std::cerr << "Send error: " << ec.message() << std::endl;
                        }
                    });
            }
        }
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 1024> recv_buffer_;
    asio::io_context& io_context_;
    std::unordered_map<std::string, udp::endpoint> clients_;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    try {
        asio::io_context io_context;
        auto server = std::make_shared<VoiceChatServer>(io_context, std::stoi(argv[1]));
        server->start();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}