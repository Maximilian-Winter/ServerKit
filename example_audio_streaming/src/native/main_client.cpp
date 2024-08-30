#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <asio.hpp>
#include <portaudio.h>
#include <queue>
#include <mutex>

using asio::ip::udp;

class AudioPlayer {
public:
    AudioPlayer() : stream_(nullptr) {}

    bool initialize() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        err = Pa_OpenDefaultStream(&stream_, 0, 2, paInt16, 44100, paFramesPerBufferUnspecified,
                                   audioCallback, this);
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return false;
        }

        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(stream_);
            Pa_Terminate();
            return false;
        }

        return true;
    }

    void addAudioData(const char* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        audioBuffer_.insert(audioBuffer_.end(), data, data + size);
    }

    ~AudioPlayer() {
        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
        }
        Pa_Terminate();
    }

private:
    static int audioCallback(const void* inputBuffer, void* outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData) {
        AudioPlayer* player = static_cast<AudioPlayer*>(userData);
        int16_t* out = static_cast<int16_t*>(outputBuffer);

        std::lock_guard<std::mutex> lock(player->mutex_);
        size_t bytesToCopy = std::min(static_cast<unsigned long>(player->audioBuffer_.size()), framesPerBuffer * 4);
        if (bytesToCopy > 0) {
            memcpy(out, player->audioBuffer_.data(), bytesToCopy);
            player->audioBuffer_.erase(player->audioBuffer_.begin(), player->audioBuffer_.begin() + bytesToCopy);
        } else {
            memset(out, 0, framesPerBuffer * 4);
        }

        return paContinue;
    }

    PaStream* stream_;
    std::vector<char> audioBuffer_;
    std::mutex mutex_;
};

class UDPAudioClient {
public:
    UDPAudioClient(asio::io_context& io_context, const std::string& host, const std::string& port)
            : socket_(io_context, udp::endpoint(udp::v4(), 0)),
              resolver_(io_context) {
        auto endpoints = resolver_.resolve(udp::v4(), host, port);
        server_endpoint_ = *endpoints.begin();
    }

    bool start() {
        if (!audioPlayer_.initialize()) {
            return false;
        }

        // Send initial packet to server
        std::string init_msg = "Hello Server";
        socket_.send_to(asio::buffer(init_msg), server_endpoint_);

        start_receive();
        return true;
    }

private:
    void start_receive() {
        socket_.async_receive_from(
                asio::buffer(recv_buffer_), server_endpoint_,
                [this](std::error_code ec, std::size_t bytes_recvd) {
                    if (!ec && bytes_recvd > 0) {
                        audioPlayer_.addAudioData(recv_buffer_.data(), bytes_recvd);
                        start_receive(); // Continue receiving
                    } else {
                        std::cerr << "Error: " << ec.message() << std::endl;
                    }
                });
    }

    udp::socket socket_;
    udp::resolver resolver_;
    udp::endpoint server_endpoint_;
    std::array<char, 65536> recv_buffer_;
    AudioPlayer audioPlayer_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }

    try {
        asio::io_context io_context;
        UDPAudioClient client(io_context, argv[1], argv[2]);

        if (client.start()) {
            io_context.run();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}