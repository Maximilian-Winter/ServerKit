//
// Created by maxim on 29.08.2024.
//
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <asio.hpp>
#include <portaudio.h>
#include <queue>
#include <mutex>
#include <thread>

using asio::ip::udp;

class AudioManager {
public:
    AudioManager() : input_stream_(nullptr), output_stream_(nullptr) {}

    bool initialize() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        err = Pa_OpenDefaultStream(&input_stream_, 1, 0, paInt16, 44100, paFramesPerBufferUnspecified,
                                   inputCallback, this);
        if (err != paNoError) {
            std::cerr << "PortAudio input error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return false;
        }

        err = Pa_OpenDefaultStream(&output_stream_, 0, 1, paInt16, 44100, paFramesPerBufferUnspecified,
                                   outputCallback, this);
        if (err != paNoError) {
            std::cerr << "PortAudio output error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(input_stream_);
            Pa_Terminate();
            return false;
        }

        err = Pa_StartStream(input_stream_);
        if (err != paNoError) {
            std::cerr << "PortAudio input start error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(input_stream_);
            Pa_CloseStream(output_stream_);
            Pa_Terminate();
            return false;
        }

        err = Pa_StartStream(output_stream_);
        if (err != paNoError) {
            std::cerr << "PortAudio output start error: " << Pa_GetErrorText(err) << std::endl;
            Pa_StopStream(input_stream_);
            Pa_CloseStream(input_stream_);
            Pa_CloseStream(output_stream_);
            Pa_Terminate();
            return false;
        }

        return true;
    }

    void addOutputData(const char* data, size_t size) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_buffer_.insert(output_buffer_.end(), data, data + size);
    }

    std::vector<char> getInputData() {
        std::lock_guard<std::mutex> lock(input_mutex_);
        std::vector<char> data = std::move(input_buffer_);
        input_buffer_.clear();
        return data;
    }

    ~AudioManager() {
        if (input_stream_) {
            Pa_StopStream(input_stream_);
            Pa_CloseStream(input_stream_);
        }
        if (output_stream_) {
            Pa_StopStream(output_stream_);
            Pa_CloseStream(output_stream_);
        }
        Pa_Terminate();
    }

private:
    static int inputCallback(const void* inputBuffer, void* outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData) {
        AudioManager* manager = static_cast<AudioManager*>(userData);
        const int16_t* in = static_cast<const int16_t*>(inputBuffer);

        std::lock_guard<std::mutex> lock(manager->input_mutex_);
        manager->input_buffer_.insert(manager->input_buffer_.end(),
                                      reinterpret_cast<const char*>(in),
                                      reinterpret_cast<const char*>(in + framesPerBuffer));

        return paContinue;
    }

    static int outputCallback(const void* inputBuffer, void* outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void* userData) {
        AudioManager* manager = static_cast<AudioManager*>(userData);
        int16_t* out = static_cast<int16_t*>(outputBuffer);

        std::lock_guard<std::mutex> lock(manager->output_mutex_);
        size_t bytesToCopy = std::min(static_cast<unsigned long>(manager->output_buffer_.size()), framesPerBuffer * 2);
        if (bytesToCopy > 0) {
            memcpy(out, manager->output_buffer_.data(), bytesToCopy);
            manager->output_buffer_.erase(manager->output_buffer_.begin(), manager->output_buffer_.begin() + bytesToCopy);
        } else {
            memset(out, 0, framesPerBuffer * 2);
        }

        return paContinue;
    }

    PaStream* input_stream_;
    PaStream* output_stream_;
    std::vector<char> input_buffer_;
    std::vector<char> output_buffer_;
    std::mutex input_mutex_;
    std::mutex output_mutex_;
};

class VoiceChatClient {
public:
    VoiceChatClient(asio::io_context& io_context, const std::string& host, const std::string& port)
        : socket_(io_context, udp::endpoint(udp::v4(), 0)),
          resolver_(io_context),
          send_timer_(io_context) {
        auto endpoints = resolver_.resolve(udp::v4(), host, port);
        server_endpoint_ = *endpoints.begin();
    }

    bool start() {
        if (!audio_manager_.initialize()) {
            return false;
        }

        start_receive();
        start_send();
        return true;
    }

private:
    void start_receive() {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), server_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    audio_manager_.addOutputData(recv_buffer_.data(), bytes_recvd);
                    start_receive();
                } else {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                    start_receive();
                }
            });
    }

    void start_send() {
        send_timer_.expires_after(std::chrono::milliseconds(20)); // 50Hz send rate
        send_timer_.async_wait([this](std::error_code ec) {
            if (!ec) {
                std::vector<char> audio_data = audio_manager_.getInputData();
                if (!audio_data.empty()) {
                    socket_.async_send_to(
                        asio::buffer(audio_data), server_endpoint_,
                        [this](std::error_code ec, std::size_t bytes_sent) {
                            if (ec) {
                                std::cerr << "Send error: " << ec.message() << std::endl;
                            }
                        });
                }
                start_send();
            }
        });
    }

    udp::socket socket_;
    udp::resolver resolver_;
    udp::endpoint server_endpoint_;
    std::array<char, 1024> recv_buffer_;
    AudioManager audio_manager_;
    asio::steady_timer send_timer_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }

    try {
        asio::io_context io_context;
        VoiceChatClient client(io_context, argv[1], argv[2]);

        if (client.start()) {
            std::cout << "Connected to voice chat server. Start speaking..." << std::endl;
            io_context.run();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}