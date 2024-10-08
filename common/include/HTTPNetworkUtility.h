//
// Created by maxim on 23.08.2024.
//

#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <algorithm>

#include "Utilities.h"
#include "Logger.h"

class HTTPNetworkUtility {
public:
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        explicit Connection(asio::io_context& io_context)
                : socket_(io_context), strand_(asio::make_strand(io_context)),
                  read_buffer_(), headers_() {}

        static std::shared_ptr<Connection> create(asio::io_context& io_context) {
            return std::make_shared<Connection>(io_context);
        }

        asio::ip::tcp::socket& socket() { return socket_; }

        void write(HTTPMessage& message) {
            FastVector::ByteVector messageData = message.serialize();
            LOG_DEBUG("Connection::write called. Message size: %zu", messageData.size());

            asio::post(strand_, [this, messageData = messageData]() mutable {
                bool write_in_progress = !write_queue_.empty();
                write_queue_.push_back(messageData);
                if (!write_in_progress) {
                    do_write();
                }
            });
        }

        void read(const std::function<void(HTTPMessage httpMessage)>& callback) {
            LOG_DEBUG("Connection::read called");
            asio::post(strand_, [this, callback]() mutable {
                do_read_headers(callback);
            });
        }

        asio::ip::tcp::endpoint remoteEndpoint() const {
            return socket_.remote_endpoint();
        }

        bool shouldClose(const HTTPMessage& message) const {
            const HTTPHeader& header = message.getHeader();
            std::string connection = header.getHeader("Connection");
            return (connection == "close" || (message.getVersion() == "HTTP/1.0" && connection != "keep-alive"));
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
            auto& message = write_queue_.front();
            asio::async_write(socket_, asio::buffer(message),
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

        void do_read_headers(const std::function<void(HTTPMessage httpMessage)>& callback) {
            auto self = shared_from_this();
            asio::async_read_until(
                    socket_,
                    asio::dynamic_buffer(read_buffer_),
                    "\r\n",
                    [this, self, callback](std::error_code ec, std::size_t bytes_transferred) {
                        if (!ec) {
                            std::string line(read_buffer_.begin(), read_buffer_.begin() + bytes_transferred);
                            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + bytes_transferred);

                            if (line == "\r\n") {
                                //headers_.insert(headers_.end(), line.begin(), line.end());
                                process_headers(callback);
                            } else {
                                // Continue reading headers
                                headers_.insert(headers_.end(), line.begin(), line.end());
                                do_read_headers(callback);
                            }
                        } else {
                            LOG_ERROR("Error reading headers: %s", ec.message().c_str());
                            close();
                        }
                    });
        }

        void process_headers(const std::function<void(HTTPMessage httpMessage)>& callback) {
            std::string header_string(headers_.begin(), headers_.end());
            size_t content_length = parse_content_length(header_string);
            bool is_chunked = is_chunked_encoding(header_string);
            bool close_connection = should_close_connection(header_string);

            if (is_chunked) {
                do_read_chunked([this, callback](const FastVector::ByteVector& body) {
                    HTTPHeader httpHeader;
                    httpHeader.deserialize(headers_);
                    HTTPBody httpBody;
                    httpBody.deserialize(body);

                    HTTPMessage httpMessage;
                    httpMessage.setHeader(httpHeader);
                    httpMessage.setBody(httpBody);
                    callback(httpMessage);
                });
            } else if (content_length > 0) {
                do_read_body(content_length, [this, callback](const FastVector::ByteVector& body) {
                    HTTPHeader httpHeader;
                    httpHeader.deserialize(headers_);
                    HTTPBody httpBody;
                    httpBody.deserialize(body);

                    HTTPMessage httpMessage;
                    httpMessage.setHeader(httpHeader);
                    httpMessage.setBody(httpBody);
                    callback(httpMessage);
                });
            } else if (close_connection) {
                do_read_until_close([this, callback](const FastVector::ByteVector& body) {
                    HTTPHeader httpHeader;
                    httpHeader.deserialize(headers_);
                    HTTPBody httpBody;
                    httpBody.deserialize(body);

                    HTTPMessage httpMessage;
                    httpMessage.setHeader(httpHeader);
                    httpMessage.setBody(httpBody);
                    callback(httpMessage);
                });
            } else {
                // No body, just return the headers
                HTTPHeader httpHeader;
                httpHeader.deserialize(headers_);
                HTTPBody httpBody;

                HTTPMessage httpMessage;
                httpMessage.setHeader(httpHeader);
                httpMessage.setBody(httpBody);
                callback(httpMessage);
            }
        }

        void do_read_body(size_t content_length, const std::function<void(const FastVector::ByteVector&)>& callback) {
            if (read_buffer_.size() >= content_length) {
                // We already have enough data
                FastVector::ByteVector body(read_buffer_.begin(), read_buffer_.begin() + content_length);
                read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + content_length);
                callback(body);
            } else {
                // We need to read more data
                asio::async_read(socket_, asio::dynamic_buffer(read_buffer_), asio::transfer_at_least(content_length - read_buffer_.size()),
                                 asio::bind_executor(strand_, [this, self = shared_from_this(), content_length, callback]
                                         (std::error_code ec, std::size_t /*length*/) {
                                     if (!ec) {
                                         FastVector::ByteVector body(read_buffer_.begin(), read_buffer_.begin() + content_length);
                                         read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + content_length);
                                         callback(body);
                                     } else {
                                         LOG_ERROR("Error in read_body: %s", ec.message().c_str());
                                         close();
                                     }
                                 }));
            }
        }

        void do_read_chunked(const std::function<void(const FastVector::ByteVector&)>& callback) {
            FastVector::ByteVector full_body;
            read_next_chunk(full_body, callback);
        }

        void read_next_chunk(FastVector::ByteVector& full_body, const std::function<void(const FastVector::ByteVector&)>& callback) {
            asio::async_read_until(socket_, asio::dynamic_buffer(read_buffer_), "\r\n",
                                   asio::bind_executor(strand_, [this, self = shared_from_this(), &full_body, callback]
                                           (std::error_code ec, std::size_t length) {
                                       if (!ec) {
                                           std::string chunk_size_line(read_buffer_.begin(), read_buffer_.begin() + length);
                                           read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + length);

                                           // Parse chunk size
                                           size_t chunk_size;
                                           std::istringstream iss(chunk_size_line);
                                           iss >> std::hex >> chunk_size;

                                           if (chunk_size == 0) {
                                               // Last chunk, read the final CRLF and return
                                               asio::async_read(socket_, asio::dynamic_buffer(read_buffer_), asio::transfer_exactly(2),
                                                                asio::bind_executor(strand_, [this, self = shared_from_this(), callback, full_body]
                                                                        (std::error_code ec, std::size_t /*length*/) {
                                                                    if (!ec) {
                                                                        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + 2);
                                                                        callback(full_body);
                                                                    } else {
                                                                        LOG_ERROR("Error reading final CRLF: %s", ec.message().c_str());
                                                                        close();
                                                                    }
                                                                }));
                                           } else {
                                               // Read the chunk data
                                               asio::async_read(socket_, asio::dynamic_buffer(read_buffer_), asio::transfer_exactly(chunk_size + 2),
                                                                asio::bind_executor(strand_, [this, self = shared_from_this(), chunk_size, &full_body, callback]
                                                                        (std::error_code ec, std::size_t /*length*/) {
                                                                    if (!ec) {
                                                                        full_body.insert(full_body.end(), read_buffer_.begin(), read_buffer_.begin() + chunk_size);
                                                                        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + chunk_size + 2);
                                                                        read_next_chunk(full_body, callback);
                                                                    } else {
                                                                        LOG_ERROR("Error reading chunk data: %s", ec.message().c_str());
                                                                        close();
                                                                    }
                                                                }));
                                           }
                                       } else {
                                           LOG_ERROR("Error reading chunk size: %s", ec.message().c_str());
                                           close();
                                       }
                                   }));
        }

        void do_read_until_close(const std::function<void(const FastVector::ByteVector&)>& callback) {
            asio::async_read(socket_, asio::dynamic_buffer(read_buffer_),
                             asio::transfer_at_least(1),
                             asio::bind_executor(strand_, [this, self = shared_from_this(), callback]
                                     (std::error_code ec, std::size_t /*length*/) {
                                 if (!ec) {
                                     do_read_until_close(callback);
                                 } else if (ec == asio::error::eof) {
                                     FastVector::ByteVector body(read_buffer_.begin(), read_buffer_.end());
                                     callback(body);
                                 } else {
                                     LOG_ERROR("Error in read_until_close: %s", ec.message().c_str());
                                     close();
                                 }
                             }));
        }

        size_t parse_content_length(const std::string& headers) {
            std::istringstream iss(headers);
            std::string line;
            while (std::getline(iss, line) && line != "\r") {
                if (line.substr(0, 16) == "Content-Length: ") {
                    return std::stoul(line.substr(16));
                }
            }
            return 0;  // No Content-Length header found
        }

        bool is_chunked_encoding(const std::string& headers) {
            std::istringstream iss(headers);
            std::string line;
            while (std::getline(iss, line) && line != "\r") {
                if (line.substr(0, 19) == "Transfer-Encoding: ") {
                    return line.find("chunked") != std::string::npos;
                }
            }
            return false;
        }

        bool should_close_connection(const std::string& headers) {
            std::istringstream iss(headers);
            std::string line;
            while (std::getline(iss, line) && line != "\r") {
                if (line.substr(0, 12) == "Connection: ") {
                    return line.find("close") != std::string::npos;
                }
            }
            return false;
        }

        asio::ip::tcp::socket socket_;
        asio::strand<asio::io_context::executor_type> strand_;
        std::string read_buffer_;
        std::deque<FastVector::ByteVector> write_queue_;
        FastVector::ByteVector headers_;
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
};
