// Client: UDPAudioClient.cpp
#include "UDPClientBase.h"
#include <cstdio>
#include <iostream>
#include <thread>
#include <atomic>

class UDPAudioClient : public UDPClientBase {
public:
    explicit UDPAudioClient(const std::string& config_file)
            : UDPClientBase(config_file), m_pipe(nullptr), m_running(false) {}

    ~UDPAudioClient() override {
        stopPlayback();
    }

    void startPlayback() {
        m_running = true;
        m_playbackThread = std::thread(&UDPAudioClient::playbackLoop, this);
    }

    void stopPlayback() {
        m_running = false;
        if (m_playbackThread.joinable()) {
            m_playbackThread.join();
        }
        if (m_pipe) {
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

protected:
    void handleMessage(const std::vector<uint8_t>& message, const asio::ip::udp::endpoint& sender_endpoint) override {
        if (m_pipe && m_running) {
            fwrite(message.data(), 1, message.size(), m_pipe);
            fflush(m_pipe);
        }
    }

    void onConnected() override {
        UDPClientBase::onConnected();
        std::cout << "Connected to server. Starting audio playback." << std::endl;
        startPlayback();
    }

    void onDisconnected() override {
        stopPlayback();
        UDPClientBase::onDisconnected();
        std::cout << "Disconnected from server. Stopped audio playback." << std::endl;
    }

private:
    void playbackLoop() {
        m_pipe = _popen("ffplay -f s16le -ar 44100 -ac 2 -i pipe:0", "wb");
        if (!m_pipe) {
            std::cerr << "Error opening ffplay pipe" << std::endl;
            return;
        }

        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        _pclose(m_pipe);
        m_pipe = nullptr;
    }

    FILE* m_pipe;
    std::atomic<bool> m_running;
    std::thread m_playbackThread;
};

int main() {
    try {
        UDPAudioClient client("client_config.json");
        client.connect();

        std::cout << "Press Enter to stop the client..." << std::endl;
        std::cin.get();

        client.disconnect();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}