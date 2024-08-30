//
// Created by maxim on 29.08.2024.
//
#include <iostream>
#include <asio.hpp>

using asio::ip::udp;

class UDPStreamingClient {
public:
    UDPStreamingClient(asio::io_context& io_context, const std::string& host, const std::string& port)
        : socket_(io_context, udp::endpoint(udp::v4(), 0)),
          resolver_(io_context) {
        auto endpoints = resolver_.resolve(udp::v4(), host, port);
        server_endpoint_ = *endpoints.begin();
    }

    void start_receive() {
        // Send an initial message to the server to start the stream
        std::string init_msg = "Start streaming";
        socket_.send_to(asio::buffer(init_msg), server_endpoint_);

        receive();
    }

private:
    void receive() {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), server_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    std::cout << recv_buffer_[0] << std::flush;
                    receive();  // Continue receiving
                } else {
                    std::cout << "Error: " << ec.message() << std::endl;
                }
            });
    }

    udp::socket socket_;
    udp::resolver resolver_;
    udp::endpoint server_endpoint_;
    std::array<char, 1> recv_buffer_;
};

int main() {
    try {
        asio::io_context io_context;
        UDPStreamingClient client(io_context, "localhost", "12345");
        client.start_receive();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}