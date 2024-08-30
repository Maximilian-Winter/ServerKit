#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <random>

#include "Logger.h"
#include "ByteVector.h"

class UDPNetworkUtility {
public:
    class Endpoint {
    public:
        Endpoint(asio::io_context& io_context, const asio::ip::udp::endpoint& endpoint)
            : socket_(io_context, endpoint) {}

        asio::ip::udp::socket& socket() { return socket_; }
        const asio::ip::udp::endpoint& endpoint() const { return socket_.local_endpoint(); }

    private:
        asio::ip::udp::socket socket_;
    };

    static std::shared_ptr<Endpoint> createEndpoint(asio::io_context& io_context, const std::string& address, unsigned short port) {
        asio::ip::udp::endpoint endpoint(asio::ip::make_address(address), port);
        return std::make_shared<Endpoint>(io_context, endpoint);
    }

    static void sendTo(std::shared_ptr<Endpoint> sender, const asio::ip::udp::endpoint& recipient, const FastVector::ByteVector& message) {
        sender->socket().async_send_to(asio::buffer(message), recipient,
            [](std::error_code ec, std::size_t /*bytes_sent*/) {
                if (ec) {
                    LOG_ERROR("Error sending UDP message: %s", ec.message().c_str());
                }
            });
    }

    static void receiveFrom(std::shared_ptr<Endpoint> receiver, 
                            std::function<void(const asio::ip::udp::endpoint&, const FastVector::ByteVector&)> callback) {
        auto buffer = std::make_shared<FastVector::ByteVector>(512); // Max UDP packet size
        auto sender_endpoint = std::make_shared<asio::ip::udp::endpoint>();

        receiver->socket().async_receive_from(asio::buffer(*buffer), *sender_endpoint,
            [receiver, buffer, sender_endpoint, callback](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec) {
                    buffer->resize(bytes_recvd);
                    callback(*sender_endpoint, *buffer);
                    receiveFrom(receiver, callback); // Continue receiving
                } else {
                    LOG_ERROR("Error receiving UDP message: %s", ec.message().c_str());
                }
            });
    }

    static std::string generateUuid() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<> dis(0, 255);

        std::array<unsigned char, 16> bytes;
        for (unsigned char& byte : bytes) {
            byte = dis(gen);
        }

        // Set version to 4
        bytes[6] = (bytes[6] & 0x0F) | 0x40;
        // Set variant to 1
        bytes[8] = (bytes[8] & 0x3F) | 0x80;

        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                ss << '-';
            }
            ss << std::setw(2) << static_cast<int>(bytes[i]);
        }

        return ss.str();
    }
};