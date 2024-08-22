//
// Created by maxim on 21.08.2024.
//

#pragma once

#include "AsioThreadPool.h"
#include "BinaryData.h"
#include "Config.h"
#include "Logger.h"
#include "NetworkUtility.h"

#include <asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <atomic>

class ClientBase {
public:
    ClientBase(const std::string& config_file)
            : m_config(), m_thread_pool(nullptr), m_connection(nullptr), m_connected(false) {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeClient();
    }

    virtual ~ClientBase() {
        disconnect();
    }

    void connect() {
        if (!m_thread_pool) {
            LOG_ERROR("Thread pool not initialized");
            return;
        }

        LOG_INFO("Connecting to server %s:%d", m_host.c_str(), m_port);

        NetworkUtility::connect(m_thread_pool->get_io_context(), m_host, std::to_string(m_port),
                                [this](std::error_code ec, std::shared_ptr<NetworkUtility::Connection> connection) {
                                    if (!ec) {
                                        m_connection = connection;
                                        m_connected.store(true);
                                        LOG_INFO("Connected to server");
                                        onConnected();
                                        startRead();
                                    } else {
                                        LOG_ERROR("Connection error: %s", ec.message().c_str());
                                        onConnectionError(ec);
                                    }
                                });


    }

    void disconnect() {
        if (m_connected.exchange(false)) {
            LOG_INFO("Disconnecting from server");
            if (m_connection) {
                m_connection->close();
            }
            if (m_thread_pool) {
                m_thread_pool->stop();
            }
            onDisconnected();
        }
    }

    void sendMessage(const std::vector<uint8_t>& message) {
        if (m_connected && m_connection) {
            m_connection->write(message);
        } else {
            LOG_ERROR("Cannot send message: not connected");
        }
    }

protected:
    virtual void handleMessage(const std::vector<uint8_t>& message) = 0;

    virtual void onConnected() {
        LOG_INFO("Successfully connected to server");
    }

    virtual void onDisconnected() {
        LOG_INFO("Disconnected from server");
    }

    virtual void onConnectionError(const std::error_code& ec) {
        LOG_ERROR("Connection error: %s", ec.message().c_str());
    }

    Config& getConfig() { return m_config; }

protected:
    void initializeClient() {
        m_host = m_config.get<std::string>("server_host", "127.0.0.1");
        m_port = m_config.get<int>("server_port", 8080);
        int thread_count = m_config.get<int>("thread_count", 1);

        std::string log_level = m_config.get<std::string>("log_level", "INFO");
        std::string log_file = m_config.get<std::string>("log_file", "client.log");

        Logger::getInstance().setLogLevel(Logger::getInstance().parseLogLevel(log_level));
        Logger::getInstance().setLogFile(log_file);

        m_thread_pool = std::make_unique<AsioThreadPool>(thread_count);
    }

    void startRead() {
        if (m_connected && m_connection) {
            m_connection->read([this](const std::vector<uint8_t>& message) {
                handleMessage(message);
                startRead();  // Continue reading
            });
        }
    }

    Config m_config;
    std::unique_ptr<AsioThreadPool> m_thread_pool;
    std::shared_ptr<NetworkUtility::Connection> m_connection;
    std::string m_host;
    int m_port;
    std::atomic<bool> m_connected;
};
