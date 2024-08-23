//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "AsioThreadPool.h"
#include "BinaryData.h"
#include "Config.h"
#include "Logger.h"
#include "TCPNetworkUtility.h"

#include <asio.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

class TCPServerBase {
public:
    explicit TCPServerBase(const std::string& config_file)
            : m_config(), m_thread_pool(), m_acceptor(nullptr) {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeServer();
    }

    virtual ~TCPServerBase() {
        stop();
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

protected:
    virtual void handleMessage(const std::shared_ptr<TCPNetworkUtility::Session>& session, const std::vector<uint8_t>& message) = 0;

    virtual void onClientConnected(const std::shared_ptr<TCPNetworkUtility::Session>& session) {
        LOG_INFO("New client connected: %s", session->connection()->remoteEndpoint().address().to_string().c_str());
    }

    virtual void onClientDisconnected(const std::shared_ptr<TCPNetworkUtility::Session>& session) {
        LOG_INFO("Client disconnected: %s", session->connection()->remoteEndpoint().address().to_string().c_str());
    }

    void broadcastMessage(const std::vector<uint8_t>& message) {
        for (const auto& pair : m_sessions) {
            pair.second->write(message);
        }
    }

    Config& getConfig() { return m_config; }

private:
    void initializeServer() {
        m_host = m_config.get<std::string>("server_host", "127.0.0.1");
        m_port = m_config.get<int>("server_port", 8080);
        int thread_count = m_config.get<int>("thread_count", 0);

        auto log_level = m_config.get<std::string>("log_level", "INFO");
        auto log_file = m_config.get<std::string>("log_file", "server.log");
        auto log_file_size_in_mb = m_config.get<float>("max_log_file_size_in_mb", 1.0f);
        auto& logger = AsyncLogger::getInstance();

        logger.setLogLevel(AsyncLogger::parseLogLevel(log_level));
        logger.addDestination(std::make_shared<AsyncLogger::ConsoleDestination>());
        logger.addDestination(std::make_shared<AsyncLogger::FileDestination>(log_file, log_file_size_in_mb * (1024 * 1024))); // Convert from megabytes to bytes.

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
                        auto session = TCPNetworkUtility::createSession(m_thread_pool->get_io_context(), socket);
                        m_sessions[session->getConnectionUuid()] = session;

                        onClientConnected(session);

                        session->start([this, session](const std::vector<uint8_t>& message) {
                            handleMessage(session, message);
                        });
                    } else {
                        LOG_ERROR("Accept error: %s", ec.message().c_str());
                    }

                    startAccept();
                });
    }

    Config m_config;
    std::unique_ptr<AsioThreadPool> m_thread_pool;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::string m_host;
    int m_port{};
    std::unordered_map<std::string, std::shared_ptr<TCPNetworkUtility::Session>> m_sessions;
};
