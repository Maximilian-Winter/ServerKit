#pragma once

#include "HTTPNetworkUtility.h"
#include "HTTPMessage.h"
#include "AsioThreadPool.h"
#include "Config.h"
#include <functional>
#include <unordered_map>
#include <memory>

class HTTPServer {
public:
    using RequestHandler = std::function<HTTPMessage(const HTTPMessage&)>;

    explicit HTTPServer(const std::string& config_file) : m_config() {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeServer();
    }

    void setRequestHandler(HTTPHeader::Method method, const std::string& path, RequestHandler handler) {
        m_handlers[{method, path}] = std::move(handler);
    }

    void start() {
        if (!m_thread_pool) {
            LOG_ERROR("Thread pool not initialized");
            return;
        }

        LOG_INFO("Starting server on %s:%d", m_host.c_str(), m_port);
        startAccept();
        m_thread_pool->run();
    }

    void stop() {
        LOG_INFO("Stopping server");
        m_acceptor->close();
        if (m_thread_pool) {
            m_thread_pool->stop();
        }
    }

private:
    void initializeServer() {
        m_host = m_config.get<std::string>("server_host", "127.0.0.1");
        m_port = m_config.get<int>("server_port", 8080);
        int thread_count = m_config.get<int>("thread_count", 0);

        m_thread_pool = std::make_unique<AsioThreadPool>(thread_count);

        m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(m_thread_pool->get_io_context());
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(m_host), m_port);
        m_acceptor->open(endpoint.protocol());
        m_acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true));
        m_acceptor->bind(endpoint);
        m_acceptor->listen();
    }

    void startAccept() {
        m_acceptor->async_accept(
                [this](std::error_code ec, asio::ip::tcp::socket socket) {
                    if (!ec) {
                        auto connection = HTTPNetworkUtility::Connection::create(m_thread_pool->get_io_context());
                        connection->socket() = std::move(socket);
                        handleConnection(connection);
                    } else {
                        LOG_ERROR("Accept error: %s", ec.message().c_str());
                    }

                    startAccept();
                });
    }

    void handleConnection(std::shared_ptr<HTTPNetworkUtility::Connection> connection) {
        connection->read([this, connection](HTTPMessage message) {

            HTTPHeader::Method method = message.getMethod();
            std::string path = message.getUrl().getPath();

            auto it = m_handlers.find({method, path});
            if (it != m_handlers.end()) {
                HTTPMessage response = it->second(message);
                sendResponse(connection, response);
            } else {
                sendNotFoundResponse(connection);
            }

            // Continue reading from this connection if it's a keep-alive connection
            if (shouldKeepAlive(message)) {
                handleConnection(connection);
            } else {
                connection->close();
            }
        });
    }

    void sendResponse(std::shared_ptr<HTTPNetworkUtility::Connection> connection, HTTPMessage& response) {
        connection->write(response);
    }

    void sendNotFoundResponse(std::shared_ptr<HTTPNetworkUtility::Connection> connection) {
        HTTPMessage response;
        response.setVersion("HTTP/1.1");
        response.setStatusCode(404);
        response.setStatusMessage("Not Found");
        response.addHeader("Content-Length", "0");
        response.addHeader("Connection", "close");
        sendResponse(connection, response);
    }

    bool shouldKeepAlive(HTTPMessage& request) {
        const HTTPHeader& header = request.getHeaders();
        std::string connection = header.getHeader("Connection");
        return (connection == "keep-alive" || (request.getVersion() == "HTTP/1.1" && connection != "close"));
    }

    Config m_config;
    std::unique_ptr<AsioThreadPool> m_thread_pool;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::string m_host;
    int m_port{};

    struct HandlerKey {
        HTTPHeader::Method method;
        std::string path;

        bool operator==(const HandlerKey& other) const {
            return method == other.method && path == other.path;
        }
    };

    struct HandlerKeyHash {
        std::size_t operator()(const HandlerKey& key) const {
            return std::hash<int>{}(static_cast<int>(key.method)) ^ std::hash<std::string>{}(key.path);
        }
    };

    std::unordered_map<HandlerKey, RequestHandler, HandlerKeyHash> m_handlers;
};