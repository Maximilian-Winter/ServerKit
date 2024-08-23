// Server: UDPAudioServer.cpp
#include "UDPServerBase.h"
#include <cstdio>
#include <array>
#include <iostream>
#include <thread>
#include <atomic>

class UDPAudioServer : public UDPServerBase {
public:
    explicit UDPAudioServer(const std::string& config_file) : UDPServerBase(config_file), m_running(false) {}

    void startStreaming() {
        m_running = true;
        m_streamThread = std::thread(&UDPAudioServer::streamAudio, this);
    }

    void stopStreaming() {
        m_running = false;
        if (m_streamThread.joinable()) {
            m_streamThread.join();
        }
    }

protected:
    void handleMessage(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& sender_endpoint) override {
        // In this example, we don't expect to receive messages from clients
        std::cout << "Received unexpected message from " << sender_endpoint << std::endl;
    }

private:
    void streamAudio() {
        std::array<char, 1024> buffer{};
        FILE* pipe = _popen("ffmpeg -re -i input.mp3 -acodec pcm_s16le -ar 44100 -ac 2 -f data -", "r");
        if (!pipe) {
            std::cerr << "Error opening ffmpeg pipe" << std::endl;
            return;
        }

        asio::ip::udp::endpoint client_endpoint(asio::ip::make_address("127.0.0.1"), 12345); // Replace with actual client address and port

        while (m_running && !feof(pipe)) {
            if (fread(buffer.data(), 1, buffer.size(), pipe) > 0) {
                sendMessage(std::vector<uint8_t>(buffer.begin(), buffer.end()), client_endpoint);
            }
        }

        _pclose(pipe);
    }

    std::atomic<bool> m_running;
    std::thread m_streamThread;
};

int main() {
    try {
        UDPAudioServer server("server_config.json");
        server.start();
        server.startStreaming();

        std::cout << "Press Enter to stop the server..." << std::endl;
        std::cin.get();

        server.stopStreaming();
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}