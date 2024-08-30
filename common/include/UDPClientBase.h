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

class UDPClientBase {
public:
    explicit UDPClientBase(const std::string& config_file)
        : m_config(), m_thread_pool(nullptr), m_endpoint(nullptr), m_server_endpoint() {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeClient();
    }

    virtual ~UDPClientBase() {
        stop();
    }

    virtual void start() {
        if (!m_thread_pool) {
            LOG_ERROR("Thread pool not initialized");
            return;
        }

        LOG_INFO("Starting UDP client, connecting to server %s:%d", m_server_host.c_str(), m_server_port);
        startReceive();
        m_thread_pool->run();
    }

    void stop() {
        LOG_INFO("Stopping UDP client");
        if (m_endpoint) {
            m_endpoint->socket().close();
        }
        if (m_thread_pool) {
            m_thread_pool->stop();
        }
    }

    void sendToServer(const FastVector::ByteVector& message) {
        UDPNetworkUtility::sendTo(m_endpoint, m_server_endpoint, message);
    }

protected:
    virtual void handleMessage(const asio::ip::udp::endpoint& sender, const FastVector::ByteVector& message) = 0;

    Config& getConfig() { return m_config; }

private:
    void initializeClient() {
        m_server_host = m_config.get<std::string>("server_host", "127.0.0.1");
        m_server_port = m_config.get<int>("server_port", 8080);
        int thread_count = m_config.get<int>("thread_count", 1);

        auto log_level = m_config.get<std::string>("log_level", "INFO");
        auto log_file = m_config.get<std::string>("log_file", "client.log");
        auto log_file_size_in_mb = m_config.get<float>("max_log_file_size_in_mb", 1.0f);
        auto& logger = AsyncLogger::getInstance();

        logger.setLogLevel(AsyncLogger::parseLogLevel(log_level));
        logger.addDestination(std::make_shared<AsyncLogger::ConsoleDestination>());
        logger.addDestination(std::make_shared<AsyncLogger::FileDestination>(log_file, log_file_size_in_mb * (1024 * 1024)));

        m_thread_pool = std::make_unique<AsioThreadPool>(thread_count);

        // Create endpoint with a random available port
        m_endpoint = UDPNetworkUtility::createEndpoint(m_thread_pool->get_io_context(), "0.0.0.0", 0);

        // Resolve server endpoint
        asio::ip::udp::resolver resolver(m_thread_pool->get_io_context());
        m_server_endpoint = *resolver.resolve(m_server_host, std::to_string(m_server_port)).begin();
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
    asio::ip::udp::endpoint m_server_endpoint;
    std::string m_server_host;
    int m_server_port{};
};