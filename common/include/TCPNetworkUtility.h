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

class TCPNetworkUtility {
public:
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        using DisconnectCallback = std::function<void(const std::string&)>;

        explicit Connection(asio::io_context& io_context, std::string identifier)
                : socket_(io_context), strand_(asio::make_strand(io_context)), identifier_(std::move(identifier)) {}


        static std::shared_ptr<Connection> create(asio::io_context& io_context, std::string identifier) {
            return std::make_shared<Connection>(io_context, std::move(identifier));
        }

        asio::ip::tcp::socket& socket() { return socket_; }

        void write(const FastVector::ByteVector& message) {
            LOG_DEBUG("Connection::write called. Message size: %zu", message.size());

            // Prepare the packet: 4-byte header (payload size) + payload
            FastVector::ByteVector packet;
            auto size = static_cast<uint32_t>(message.size());
            packet.resize(4 + size);
            packet[0] = size & 0xFF;
            packet[1] = (size >> 8) & 0xFF;
            packet[2] = (size >> 16) & 0xFF;
            packet[3] = (size >> 24) & 0xFF;
            std::memcpy(packet.begin() + 4, message.begin(), message.size());

            asio::post(strand_, [this, packet]() mutable {
                bool write_in_progress = !write_queue_.empty();
                write_queue_.push_back(packet);
                if (!write_in_progress) {
                    do_write();
                }
            });
        }

        void read(const std::function<void(const FastVector::ByteVector&)>& callback) {
            LOG_DEBUG("Connection::read called");

            asio::post(strand_, [this, callback]() mutable {
                do_read_header(callback);
            });
        }

        asio::ip::tcp::endpoint remoteEndpoint() const {
            return socket_.remote_endpoint();
        }
        void setOnDisconnectedCallback(DisconnectCallback callback) {
            onDisconnected_ = std::move(callback);
        }

        void close() {
            asio::post(strand_, [this, self = shared_from_this()]() {
                // Call the onDisconnected callback if it's set
                if (onDisconnected_) {
                    onDisconnected_(identifier_);
                }

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
            auto packet = std::make_shared<FastVector::ByteVector>(write_queue_.front());
            asio::async_write(socket_, asio::buffer(*packet),
                              asio::bind_executor(strand_, [this, self = shared_from_this()](std::error_code ec, std::size_t length) {
                                  if (!ec) {
                                      LOG_DEBUG("Write completed. Length: %zu", length);
                                      write_queue_.pop_front();
                                      if (!write_queue_.empty()) {
                                          do_write();
                                      }
                                  } else {
                                      LOG_ERROR("Error in write: %s", ec.message().c_str());
                                      close();
                                  }
                              }));
        }

        void do_read_header(const std::function<void(const FastVector::ByteVector&)>& callback) {
            auto header_buffer = std::make_shared<std::vector<uint8_t>>(4);
            asio::async_read(socket_, asio::buffer(*header_buffer),
                             asio::bind_executor(strand_, [this, self = shared_from_this(), callback, header_buffer]
                                     (std::error_code ec, std::size_t /*length*/) {
                                 if (!ec) {
                                     uint32_t payload_size = (*header_buffer)[0] |
                                                             ((*header_buffer)[1] << 8) |
                                                             ((*header_buffer)[2] << 16) |
                                                             ((*header_buffer)[3] << 24);
                                     LOG_DEBUG("Read header: %02x %02x %02x %02x",
                                               (*header_buffer)[0], (*header_buffer)[1],
                                               (*header_buffer)[2], (*header_buffer)[3]);
                                     LOG_DEBUG("Interpreted payload size: %u", payload_size);
                                     do_read_body(payload_size, callback);
                                 } else if (ec == asio::error::eof) {
                                     LOG_INFO("Connection closed by peer");
                                 } else {
                                     LOG_ERROR("Error in read_header: %s", ec.message().c_str());
                                     close();
                                 }
                             }));
        }

        void do_read_body(uint32_t payload_size, const std::function<void(const FastVector::ByteVector&)>& callback) {
            LOG_DEBUG("Connection::do_read_body called. Payload size: %u", payload_size);
            auto read_buffer = std::make_shared<FastVector::ByteVector>(payload_size);
            asio::async_read(socket_, asio::buffer(*read_buffer),
                             asio::bind_executor(strand_, [this, self = shared_from_this(), read_buffer, callback, payload_size]
                                     (std::error_code ec, std::size_t length) {
                                 if (!ec) {
                                     LOG_DEBUG("Read message size: %zu", length);
                                     if (length == payload_size) {
                                         // Execute callback outside of strand to prevent potential deadlocks
                                         asio::post([this, callback, read_buffer]() {
                                             LOG_DEBUG("Executing read callback");
                                             try {
                                                 callback(*read_buffer);
                                             } catch (const std::exception& e) {
                                                 LOG_ERROR("Exception in read callback: %s", e.what());
                                             }
                                             // Only start reading the next header after the callback has completed
                                             asio::post(strand_, [this, callback]() {
                                                 do_read_header(callback);
                                             });
                                         });
                                     } else {
                                         LOG_ERROR("Incomplete read. Expected %u bytes, got %zu", payload_size, length);
                                         close();
                                     }
                                 } else {
                                     LOG_ERROR("Error in read_body: %s", ec.message().c_str());
                                     close();
                                 }
                             }));
        }

        asio::ip::tcp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        std::deque<FastVector::ByteVector> write_queue_;
        std::string identifier_;
        DisconnectCallback onDisconnected_;
    };

    class Session : public std::enable_shared_from_this<Session> {
    public:
        explicit Session(std::shared_ptr<Connection> connection, std::string identifier)
                : connection_(std::move(connection)), connection_id(identifier) {}

        void start(const std::function<void(const FastVector::ByteVector&)>& messageHandler) {
            if (!connection_) {
                LOG_ERROR("Attempting to start session with null connection");
                return;
            }
            connection_->read(messageHandler);
        }

        void write(const FastVector::ByteVector& message) {
            if (connection_) {
                connection_->write(message);
            } else {
                LOG_ERROR("Attempting to write to null connection");
            }
        }

        std::string getConnectionId()
        {
            return connection_id;
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
        std::string connection_id;
    };
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

        std::string uuid = ss.str();

        return uuid;
    }
    static std::shared_ptr<Connection> connect(asio::io_context& io_context,
                                               const std::string& host,
                                               const std::string& port,
                                               std::function<void(std::error_code, std::shared_ptr<Connection>)> callback,
                                               std::string identifier = "") {
        if(identifier.empty())
        {
            identifier = generateUuid();
        }
        auto connection = Connection::create(io_context, identifier);

        asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        asio::async_connect(connection->socket(), endpoints,
                            [connection, callback](const std::error_code& ec, const asio::ip::tcp::endpoint&) {
                                callback(ec, connection);
                            });

        return connection;
    }

    static std::shared_ptr<Connection> createConnection(asio::io_context& io_context, std::string identifier) {
        return Connection::create(io_context, std::move(identifier));
    }

    static std::shared_ptr<Session> createSession(asio::io_context& io_context, asio::ip::tcp::socket& socket) {
        std::string id = generateUuid();
        auto connection = createConnection(io_context, id);
        connection->socket() = std::move(socket);
        return std::make_shared<Session>(connection, id);
    }

    static std::shared_ptr<Session> createSession(asio::io_context& io_context,
                                                  const std::string& host,
                                                  const std::string& port,
                                                  const std::function<void(std::error_code, std::shared_ptr<Connection>)>& callback) {
        std::string id = generateUuid();
        auto connection = Connection::create(io_context, id);

        asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        asio::async_connect(connection->socket(), endpoints,
                            [connection, callback](const std::error_code& ec, const asio::ip::tcp::endpoint&) {
                                callback(ec, connection);
                            });

        return std::make_shared<Session>(connection, id);
    }


};

