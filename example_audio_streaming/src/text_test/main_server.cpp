#include <iostream>
#include <string>
#include <asio.hpp>
#include <chrono>
#include <thread>

using asio::ip::udp;

class UDPStreamingServer {
public:
    UDPStreamingServer(asio::io_context& io_context, short port)
        : socket_(io_context, udp::endpoint(udp::v4(), port)),
          timer_(io_context),
          message_("Hello, this is a streaming message from the server!"),
          current_pos_(0) {
    }

    void start() {
        std::cout << "Server started. Waiting for a client..." << std::endl;
        start_receive();
    }

private:
    void start_receive() {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), remote_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    std::cout << "Received request from client. Starting stream..." << std::endl;
                    stream_next_char();
                } else {
                    start_receive();
                }
            });
    }

    void stream_next_char() {
        if (current_pos_ < message_.length()) {
            // Store the current character in a member variable
            current_char_ = message_[current_pos_];
            socket_.async_send_to(
                asio::buffer(&current_char_, 1), remote_endpoint_,
                [this](std::error_code ec, std::size_t bytes_sent) {
                    if (!ec) {
                        std::cout << "Sent: " << current_char_ << std::endl;
                        current_pos_++;
                        timer_.expires_after(std::chrono::milliseconds(500));
                        timer_.async_wait([this](std::error_code ec) {
                            if (!ec) {
                                stream_next_char();
                            }
                        });
                    }
                });
        } else {
            std::cout << "Finished streaming message. Waiting for next client..." << std::endl;
            current_pos_ = 0;
            start_receive();
        }
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 1024> recv_buffer_;
    asio::steady_timer timer_;
    std::string message_;
    size_t current_pos_;
    char current_char_; // Added to store the current character
};

int main() {
    try {
        asio::io_context io_context;
        UDPStreamingServer server(io_context, 12345);
        server.start();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}