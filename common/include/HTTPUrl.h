//
// Created by maxim on 23.08.2024.
//

#pragma once

#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>

class HTTPUrl {
public:
    HTTPUrl() = default;
    explicit HTTPUrl(const std::string& url) { parse(url); }

    void parse(const std::string& url) {
        size_t schemeEnd = url.find("://");
        if (schemeEnd != std::string::npos) {
            m_scheme = url.substr(0, schemeEnd);
            schemeEnd += 3;
        } else {
            schemeEnd = 0;
        }

        size_t hostEnd = url.find('/', schemeEnd);
        if (hostEnd == std::string::npos) {
            m_host = url.substr(schemeEnd);
            m_path = "/";
        } else {
            m_host = url.substr(schemeEnd, hostEnd - schemeEnd);
            size_t queryStart = url.find('?', hostEnd);
            if (queryStart == std::string::npos) {
                m_path = url.substr(hostEnd);
            } else {
                m_path = url.substr(hostEnd, queryStart - hostEnd);
                parseQueryString(url.substr(queryStart + 1));
            }
        }

        size_t portStart = m_host.find(':');
        if (portStart != std::string::npos) {
            m_port = std::stoi(m_host.substr(portStart + 1));
            m_host = m_host.substr(0, portStart);
        }
    }

    void parseQueryString(const std::string& queryString) {
        std::istringstream iss(queryString);
        std::string pair;
        while (std::getline(iss, pair, '&')) {
            size_t equalPos = pair.find('=');
            if (equalPos != std::string::npos) {
                std::string key = pair.substr(0, equalPos);
                std::string value = pair.substr(equalPos + 1);
                m_queryParams[urlDecode(key)] = urlDecode(value);
            }
        }
    }

    static std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value;
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += str[i];
                }
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }

    [[nodiscard]] const std::string& getScheme() const { return m_scheme; }
    [[nodiscard]] const std::string& getHost() const { return m_host; }
    [[nodiscard]] int getPort() const { return m_port; }
    [[nodiscard]] const std::string& getPath() const { return m_path; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& getQueryParams() const { return m_queryParams; }

private:
    std::string m_scheme;
    std::string m_host;
    int m_port = -1;
    std::string m_path;
    std::unordered_map<std::string, std::string> m_queryParams;
};
