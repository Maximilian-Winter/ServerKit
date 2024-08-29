#pragma once

#include "AsioThreadPool.h"
#include "BinaryData.h"
#include "Config.h"
#include "Logger.h"
#include "UDPNetworkUtility.h"

#include <asio.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

class UDPServerBase {
public:
    explicit UDPServerBase(const std::string& config_file)
        : m_config(), m_thread_pool(nullptr), m_endpoint(nullptr) {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeServer();
    }

    virtual ~UDPServerBase() {
        stop();
    }

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
        if (m_endpoint) {
            m_endpoint->socket().close();
        }
        if (m_thread_pool) {
            m_thread_pool->stop();
        }
    }

protected:
    virtual void handleMessage(const asio::ip::udp::endpoint& sender, const FastVector::ByteVector& message) = 0;

    void sendTo(const asio::ip::udp::endpoint& recipient, const FastVector::ByteVector& message) {
        UDPNetworkUtility::sendTo(m_endpoint, recipient, message);
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
        logger.addDestination(std::make_shared<AsyncLogger::FileDestination>(log_file, log_file_size_in_mb * (1024 * 1024)));

        m_thread_pool = std::make_unique<AsioThreadPool>(thread_count);

        m_endpoint = UDPNetworkUtility::createEndpoint(m_thread_pool->get_io_context(), m_host, m_port);
    }

    void startReceive() {
        UDPNetworkUtility::receiveFrom(m_endpoint, 
            [this](const asio::ip::udp::endpoint& sender, const FastVector::ByteVector& message) {
                handleMessage(sender, message);
            });
    }

    Config m_config;
    std::unique_ptr<AsioThreadPool> m_thread_pool;
    std::shared_ptr<UDPNetworkUtility::Endpoint> m_endpoint;
    std::string m_host;
    int m_port{};
};