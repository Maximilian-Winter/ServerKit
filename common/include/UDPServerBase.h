//
// Created by maxim on 23.08.2024.
//

#pragma once

#include "AsioThreadPool.h"
#include "BinaryData.h"
#include "Config.h"
#include "Logger.h"
#include "UDPNetworkUtility.h"

#include <asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <unordered_map>


class UDPServerBase {
public:
    explicit UDPServerBase(const std::string& config_file)
            : m_config(), m_thread_pool(nullptr), m_session(nullptr) {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeServer();
    }

    virtual ~UDPServerBase() = default;

    void start() {
        if (!m_thread_pool) {
            LOG_ERROR("Thread pool not initialized");
            return;
        }

        LOG_INFO("Starting UDP server on %s:%d", m_host.c_str(), m_port);
        startReceive();
        m_thread_pool->run();
    }

    void stop() {
        LOG_INFO("Stopping UDP server");
        if (m_session) {
            m_session->close();
        }
        if (m_thread_pool) {
            m_thread_pool->stop();
        }
    }

protected:
    virtual void handleMessage(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& sender_endpoint) = 0;

    void sendMessage(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& recipient_endpoint) {
        if (m_session) {
            m_session->send_to(message, recipient_endpoint);
        } else {
            LOG_ERROR("Cannot send message: UDP session not set up");
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

        asio::ip::udp::endpoint endpoint(asio::ip::make_address(m_host), m_port);
        m_session = UDPNetworkUtility::createSession(m_thread_pool->get_io_context());
        m_session->connection()->socket().open(endpoint.protocol());
        m_session->connection()->socket().bind(endpoint);
    }

    void startReceive() {
        if (m_session) {
            m_session->start([this](const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& sender_endpoint) {
                handleMessage(message, sender_endpoint);
            });
        }
    }

    Config m_config;
    std::unique_ptr<AsioThreadPool> m_thread_pool;
    std::shared_ptr<UDPNetworkUtility::Session> m_session;
    std::string m_host;
    int m_port{};
};
