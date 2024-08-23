//
// Created by maxim on 23.08.2024.
//

#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>

#include "Logger.h"
#include "Utilities.h"

class UDPNetworkUtility {
public:
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        explicit Connection(asio::io_context& io_context)
                : socket_(io_context), strand_(asio::make_strand(io_context)), read_buffer_(65507) {}

        static std::shared_ptr<Connection> create(asio::io_context& io_context) {
            return std::make_shared<Connection>(io_context);
        }

        asio::ip::udp::socket& socket() { return socket_; }

        void send_to(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& endpoint) {
            LOG_DEBUG("Connection::send_to called. Message size: %zu", message.size());

            asio::post(strand_, [this, message, endpoint]() {
                bool write_in_progress = !write_queue_.empty();
                write_queue_.emplace_back(message, endpoint);
                if (!write_in_progress) {
                    do_write();
                }
            });
        }

        void receive(const std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>& callback) {
            LOG_DEBUG("Connection::receive called");
            asio::post(strand_, [this, callback]() mutable {
                do_receive(callback);
            });
        }

        void close() {
            asio::post(strand_, [this, self = shared_from_this()]() {
                if (!socket_.is_open()) {
                    return;  // Socket is already closed
                }

                std::error_code ec;

                // Cancel any pending asynchronous operations
                socket_.cancel(ec);
                if (ec) {
                    LOG_ERROR("Error cancelling pending operations: %s", ec.message().c_str());
                }

                // Shutdown the socket
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                if (ec && ec != asio::error::not_connected) {
                    LOG_ERROR("Error shutting down socket: %s", ec.message().c_str());
                }

                // Close the socket
                socket_.close(ec);
                if (ec) {
                    LOG_ERROR("Error closing socket: %s", ec.message().c_str());
                }
            });
        }

    private:
        void do_write() {
            auto& [message, endpoint] = write_queue_.front();
            socket_.async_send_to(asio::buffer(message), endpoint,
                                  asio::bind_executor(strand_, [this, self = shared_from_this()](std::error_code ec, std::size_t length) {
                                      if (!ec) {
                                          LOG_DEBUG("UDP Write completed. Length: %zu", length);
                                          write_queue_.pop_front();
                                          if (!write_queue_.empty()) {
                                              do_write();
                                          }
                                      } else {
                                          LOG_ERROR("Error in UDP write: %s", ec.message().c_str());
                                      }
                                  }));
        }

        void do_receive(const std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>& callback) {
            socket_.async_receive_from(asio::buffer(read_buffer_), sender_endpoint_,
                                       asio::bind_executor(strand_, [this, self = shared_from_this(), callback]
                                               (std::error_code ec, std::size_t length) {
                                           if (!ec) {
                                               LOG_DEBUG("UDP Read message size: %zu", length);

                                               // Execute callback outside of strand to prevent potential deadlocks
                                               asio::post([callback, message = std::vector<uint8_t>(read_buffer_.begin(), read_buffer_.begin() + length), sender_endpoint = sender_endpoint_]() {
                                                   LOG_DEBUG("Executing UDP read callback");
                                                   try {
                                                       callback(message, sender_endpoint);
                                                   } catch (const std::exception& e) {
                                                       LOG_ERROR("Exception in UDP read callback: %s", e.what());
                                                   }
                                               });

                                               // Continue reading
                                               do_receive(callback);
                                           } else {
                                               LOG_ERROR("Error in UDP receive: %s", ec.message().c_str());
                                           }
                                       }));
        }

        asio::ip::udp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        std::vector<uint8_t> read_buffer_;
        asio::ip::udp::endpoint sender_endpoint_;
        std::deque<std::pair<std::vector<uint8_t>, asio::ip::udp::endpoint>> write_queue_;
    };

    class Session : public std::enable_shared_from_this<Session> {
    public:
        explicit Session(std::shared_ptr<Connection> connection)
                : connection_(std::move(connection)), connection_uuid(Utilities::generateUuid()) {}

        void start(const std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>& messageHandler) {
            if (!connection_) {
                LOG_ERROR("Attempting to start UDP session with null connection");
                return;
            }
            connection_->receive(messageHandler);
        }

        void send_to(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& endpoint) {
            if (connection_) {
                connection_->send_to(message, endpoint);
            } else {
                LOG_ERROR("Attempting to write to null UDP connection");
            }
        }

        std::string getConnectionUuid()
        {
            return connection_uuid;
        }

        std::shared_ptr<Connection> connection() const {
            return connection_;
        }

        void close() {
            if (connection_) {
                connection_->close();
            }
        }

    private:
        std::shared_ptr<Connection> connection_;
        std::string connection_uuid;
    };

    static std::shared_ptr<Connection> connect(asio::io_context& io_context,
                                                      const std::string& host,
                                                      const std::string& port,
                                                      std::function<void(std::error_code, std::shared_ptr<Connection>)> callback) {
        auto connection = Connection::create(io_context);

        asio::ip::udp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        connection->socket().async_connect(*endpoints.begin(),
                                           [connection, callback](const std::error_code& ec) {
                                               callback(ec, connection);
                                           });

        return connection;
    }

    static std::shared_ptr<Connection> createConnection(asio::io_context& io_context) {
        return Connection::create(io_context);
    }

    static std::shared_ptr<Session> createSession(asio::io_context& io_context) {
        auto connection = createConnection(io_context);
        connection->socket().open(asio::ip::udp::v4());
        return std::make_shared<Session>(connection);
    }
};


