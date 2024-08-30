//
// Created by maxim on 29.08.2024.
//

#pragma once

#include "UDPServerBase.h"
#include <windows.h>
#include <thread>
#include <atomic>
#include <condition_variable>

class AudioUDPServer : public UDPServerBase {
public:
    explicit AudioUDPServer(const std::string& config_file)
        : UDPServerBase(config_file), running_(false), client_connected_(false) {}

    ~AudioUDPServer() override {
        running_ = false;
        if (read_pipe_thread_.joinable()) {
            read_pipe_thread_.join();
        }
    }

    void start() override {
        UDPServerBase::start();

        const auto input_file = getConfig().get<std::string>("input_file", "input.mp3");
        if (input_file.empty()) {
            LOG_ERROR("Input file not specified in config");
            return;
        }
        startFFmpeg(input_file);

    }

protected:
    void handleMessage(const asio::ip::udp::endpoint& sender, const FastVector::ByteVector& message) override {
        if (!client_connected_) {
            LOG_INFO("Received data from client. Starting stream...");
            {
                std::lock_guard<std::mutex> lock(mutex_);
                client_connected_ = true;
                client_endpoint_ = sender;
            }
            cv_.notify_one();
        }
        // Process received data if needed
    }

private:
    bool startFFmpeg(const std::string& input_file) {
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hReadPipe, hWritePipe;

        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            LOG_ERROR("Failed to create pipe");
            return false;
        }

        std::string ffmpeg_command = "ffmpeg -re -i \"" + input_file +
                             "\" -f s16le -acodec pcm_s16le -ar 44100 -ac 2 -";

        STARTUPINFOA si = {sizeof(si)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION pi;

        if (!CreateProcessA(NULL, const_cast<char*>(ffmpeg_command.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            LOG_ERROR("Failed to start ffmpeg process");
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return false;
        }

        CloseHandle(hWritePipe);

        running_ = true;
        read_pipe_thread_ = std::thread([this, hReadPipe]() {
            readPipe(hReadPipe);
        });

        return true;
    }

    void readPipe(HANDLE hPipe) {
        const int BUFFER_SIZE = 65536;
        std::vector<char> buffer(BUFFER_SIZE);
        DWORD bytesRead;

        while (running_) {
            if (ReadFile(hPipe, buffer.data(), BUFFER_SIZE, &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this] { return client_connected_ || !running_; });
                    if (!running_) break;

                    FastVector::ByteVector audioData(buffer.data(), buffer.data() + bytesRead);
                    sendTo(client_endpoint_, audioData);
                }
            } else {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    LOG_INFO("FFmpeg process has ended the stream.");
                    break;
                } else {
                    LOG_ERROR("ReadFile failed with error %d", error);
                    break;
                }
            }
        }

        CloseHandle(hPipe);
    }

    std::atomic<bool> running_;
    std::thread read_pipe_thread_;
    std::atomic<bool> client_connected_;
    std::mutex mutex_;
    std::condition_variable cv_;
    asio::ip::udp::endpoint client_endpoint_;
};