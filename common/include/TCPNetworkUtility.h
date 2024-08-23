#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>

class TCPNetworkUtility {
public:
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        explicit Connection(asio::io_context& io_context)
                : socket_(io_context), strand_(asio::make_strand(io_context)), read_buffer_() {}

        static std::shared_ptr<Connection> create(asio::io_context& io_context) {
            return std::make_shared<Connection>(io_context);
        }

        asio::ip::tcp::socket& socket() { return socket_; }

        void write(const std::vector<uint8_t>& message) {
            LOG_DEBUG("Connection::write called. Message size: %zu", message.size());

            // Prepare the packet: 4-byte header (payload size) + payload
            std::vector<uint8_t> packet;
            auto size = static_cast<uint32_t>(message.size());
            packet.resize(4 + size);
            packet[0] = size & 0xFF;
            packet[1] = (size >> 8) & 0xFF;
            packet[2] = (size >> 16) & 0xFF;
            packet[3] = (size >> 24) & 0xFF;
            std::copy(message.begin(), message.end(), packet.begin() + 4);

            asio::post(strand_, [this, packet = std::move(packet)]() mutable {
                bool write_in_progress = !write_queue_.empty();
                write_queue_.push_back(std::move(packet));
                if (!write_in_progress) {
                    do_write();
                }
            });
        }

        void read(const std::function<void(const std::vector<uint8_t>&)>& callback) {
            LOG_DEBUG("Connection::read called");
            asio::post(strand_, [this, callback]() mutable {
                do_read_header(callback);
            });
        }

        asio::ip::tcp::endpoint remoteEndpoint() const {
            return socket_.remote_endpoint();
        }

        void close() {
            asio::post(strand_, [this]() {
                std::error_code ec;
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                if (ec) LOG_ERROR("Error shutting down socket: %s", ec.message().c_str());
            });
        }

    private:
        void do_write() {
            auto& packet = write_queue_.front();
            asio::async_write(socket_, asio::buffer(packet),
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

        void do_read_header(const std::function<void(const std::vector<uint8_t>&)>& callback) {
            asio::async_read(socket_, asio::buffer(header_buffer_, 4),
                             asio::bind_executor(strand_, [this, self = shared_from_this(), callback]
                                     (std::error_code ec, std::size_t /*length*/) {
                                 if (!ec) {
                                     uint32_t payload_size = header_buffer_[0] |
                                                             (header_buffer_[1] << 8) |
                                                             (header_buffer_[2] << 16) |
                                                             (header_buffer_[3] << 24);
                                     do_read_body(payload_size, callback);
                                 } else if (ec == asio::error::eof) {
                                     LOG_INFO("Connection closed by peer");
                                 } else {
                                     LOG_ERROR("Error in read_header: %s", ec.message().c_str());
                                     close();
                                 }
                             }));
        }

        void do_read_body(uint32_t payload_size, const std::function<void(const std::vector<uint8_t>&)>& callback) {
            LOG_DEBUG("Connection::do_read_body called. Payload size: %u", payload_size);
            read_buffer_.resize(payload_size);
            asio::async_read(socket_, asio::buffer(read_buffer_),
                             asio::bind_executor(strand_, [this, self = shared_from_this(), callback]
                                     (std::error_code ec, std::size_t length) {
                                 if (!ec) {
                                     LOG_DEBUG("Read message size: %zu", length);

                                     // Execute callback outside of strand to prevent potential deadlocks
                                     asio::post([callback, message = read_buffer_]() {
                                         LOG_DEBUG("Executing read callback");
                                         try {
                                             callback(message);
                                         } catch (const std::exception& e) {
                                             LOG_ERROR("Exception in read callback: %s", e.what());
                                         }
                                     });

                                     // Continue reading
                                     do_read_header(callback);
                                 } else {
                                     LOG_ERROR("Error in read_body: %s", ec.message().c_str());
                                     close();
                                 }
                             }));
        }

        asio::ip::tcp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        std::vector<uint8_t> read_buffer_;
        uint8_t header_buffer_[4];
        std::deque<std::vector<uint8_t>> write_queue_;
    };

    class Session : public std::enable_shared_from_this<Session> {
    public:
        explicit Session(std::shared_ptr<Connection> connection)
                : connection_(std::move(connection)) {}

        void start(const std::function<void(const std::vector<uint8_t>&)>& messageHandler) {
            if (!connection_) {
                LOG_ERROR("Attempting to start session with null connection");
                return;
            }
            connection_->read(messageHandler);
        }

        void write(const std::vector<uint8_t>& message) {
            if (connection_) {
                connection_->write(message);
            } else {
                LOG_ERROR("Attempting to write to null connection");
            }
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
    };

    static std::shared_ptr<Connection> connect(asio::io_context& io_context,
                                               const std::string& host,
                                               const std::string& port,
                                               std::function<void(std::error_code, std::shared_ptr<Connection>)> callback) {
        auto connection = Connection::create(io_context);

        asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        asio::async_connect(connection->socket(), endpoints,
                            [connection, callback](const std::error_code& ec, const asio::ip::tcp::endpoint&) {
                                callback(ec, connection);
                            });

        return connection;
    }

    static std::shared_ptr<Connection> createConnection(asio::io_context& io_context) {
        return Connection::create(io_context);
    }

    static std::shared_ptr<Session> createSession(asio::io_context& io_context, asio::ip::tcp::socket& socket) {
        auto connection = createConnection(io_context);
        connection->socket() = std::move(socket);
        return std::make_shared<Session>(connection);
    }
};

