//
// Created by maxim on 23.08.2024.
//

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

class UDPClientBase {
public:
    explicit UDPClientBase(const std::string& config_file)
            : m_config(), m_thread_pool(nullptr), m_session(nullptr), m_connected(false) {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeClient();
    }

    virtual ~UDPClientBase() {
        disconnect();
    }

    void connect() {
        if (!m_thread_pool) {
            LOG_ERROR("Thread pool not initialized");
            return;
        }

        LOG_INFO("Setting up UDP client for server %s:%d", m_host.c_str(), m_port);

        m_session = UDPNetworkUtility::createSession(m_thread_pool->get_io_context());
        m_connected.store(true);
        LOG_INFO("UDP session created");
        onConnected();
        startReceive();

        m_thread_pool->run();
    }

    void disconnect() {
        if (m_connected.exchange(false)) {
            LOG_INFO("Closing UDP session");
            if (m_session) {
                m_session->close();
            }
            if (m_thread_pool) {
                m_thread_pool->stop();
            }
            onDisconnected();
        }
    }

    void sendMessage(const std::vector<uint8_t>& message) {
        if (m_connected && m_session) {
            asio::ip::udp::endpoint server_endpoint(asio::ip::make_address(m_host), m_port);
            m_session->send_to(message, server_endpoint);
        } else {
            LOG_ERROR("Cannot send message: UDP session not set up");
        }
    }

protected:
    virtual void handleMessage(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& sender_endpoint) = 0;

    virtual void onConnected() {
        LOG_INFO("UDP session set up successfully");
    }

    virtual void onDisconnected() {
        LOG_INFO("UDP session closed");
    }

    Config& getConfig() { return m_config; }

protected:
    void initializeClient() {
        m_host = m_config.get<std::string>("server_host", "127.0.0.1");
        m_port = m_config.get<int>("server_port", 8080);
        int thread_count = m_config.get<int>("thread_count", 1);

        auto log_level = m_config.get<std::string>("log_level", "INFO");
        auto log_file = m_config.get<std::string>("log_file", "client.log");
        auto log_file_size_in_mb = m_config.get<float>("max_log_file_size_in_mb", 1.0f);
        auto& logger = AsyncLogger::getInstance();

        logger.setLogLevel(AsyncLogger::parseLogLevel(log_level));
        logger.addDestination(std::make_shared<AsyncLogger::ConsoleDestination>());
        logger.addDestination(std::make_shared<AsyncLogger::FileDestination>(log_file, log_file_size_in_mb * (1024 * 1024))); // Convert from megabytes to bytes.

        m_thread_pool = std::make_unique<AsioThreadPool>(thread_count);
    }

    void startReceive() {
        if (m_connected && m_session) {
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
    std::atomic<bool> m_connected;
};
