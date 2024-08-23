#pragma once

#include "HTTPNetworkUtility.h"
#include "HTTPMessage.h"
#include "AsioThreadPool.h"
#include "Config.h"
#include <future>
#include <memory>

class HTTPClient {
public:
    explicit HTTPClient(const std::string& config_file) : m_config(), m_connection(nullptr) {
        if (!m_config.load(config_file)) {
            LOG_FATAL("Failed to load configuration file: %s", config_file.c_str());
            throw std::runtime_error("Failed to load configuration file");
        }

        initializeClient();
    }

    std::future<HTTPMessage> sendRequest(HTTPMessage& request) {
        auto promise = std::make_shared<std::promise<HTTPMessage>>();
        auto future = promise->get_future();

        if (!m_connection) {
            promise->set_exception(std::make_exception_ptr(std::runtime_error("Not connected")));
            return future;
        }

        m_connection->write(request);

        m_connection->read([promise](const HTTPMessage& response) {
            promise->set_value(response);
        });

        return future;
    }

    std::future<HTTPMessage> get(const std::string& url) {
        return sendHttpRequest(HTTPHeader::Method::GET_METHOD, url);
    }

    std::future<HTTPMessage> post(const std::string& url, const std::string& body) {
        return sendHttpRequest(HTTPHeader::Method::POST_METHOD, url, body);
    }

    std::future<HTTPMessage> put(const std::string& url, const std::string& body) {
        return sendHttpRequest(HTTPHeader::Method::PUT_METHOD, url, body);
    }

    std::future<HTTPMessage> del(const std::string& url) {
        return sendHttpRequest(HTTPHeader::Method::DELETE_METHOD, url);
    }

    void connect(const std::string& host, const std::string& port) {
        auto connect_promise = std::make_shared<std::promise<void>>();
        auto connect_future = connect_promise->get_future();

        HTTPNetworkUtility::connect(m_thread_pool->get_io_context(), host, port,
                                    [this, connect_promise](std::error_code ec, std::shared_ptr<HTTPNetworkUtility::Connection> connection) {
                                        if (!ec) {
                                            m_connection = connection;
                                            connect_promise->set_value();
                                        } else {
                                            connect_promise->set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
                                        }
                                    });

        m_thread_pool->run();
        connect_future.get(); // This will throw an exception if the connection failed
    }

    void disconnect() {
        if (m_connection) {
            m_connection->close();
            m_connection.reset();
        }
    }

private:
    void initializeClient() {
        int thread_count = m_config.get<int>("thread_count", 1);
        m_thread_pool = std::make_unique<AsioThreadPool>(thread_count);
    }

    std::future<HTTPMessage> sendHttpRequest(HTTPHeader::Method method, const std::string& url, const std::string& body = "") {
        HTTPMessage request;
        request.setMethod(method);

        HTTPUrl httpUrl(url);
        request.setUrl(httpUrl);

        request.setVersion("HTTP/1.1");
        request.addHeader("Host", httpUrl.getHost());

        // Change this line
        request.addHeader("Connection", "keep-alive");

        if (!body.empty()) {
            HTTPBody httpBody;
            httpBody.setContent(body);
            request.setBody(httpBody);
            httpBody.serialize();
            request.addHeader("Content-Length", std::to_string(httpBody.ByteSize()));
            request.addHeader("Content-Type", "text/plain");
        }

        auto future = sendRequest(request);

        return future;
    }

    Config m_config;
    std::unique_ptr<AsioThreadPool> m_thread_pool;
    std::shared_ptr<HTTPNetworkUtility::Connection> m_connection;
};