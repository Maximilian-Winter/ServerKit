//
// Created by maxim on 29.08.2024.
//

//
// Created by maxim on 29.08.2024.
//

#pragma once

#include "UDPClientBase.h"
#include <portaudio.h>
#include <vector>
#include <mutex>

class AudioUDPClient : public UDPClientBase {
public:
    explicit AudioUDPClient(const std::string& config_file)
        : UDPClientBase(config_file), stream_(nullptr) {}

    ~AudioUDPClient() override {
        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
        }
        Pa_Terminate();
    }

    void start() override {
        UDPClientBase::start();

        if (!initializeAudio()) {
            return;
        }

        // Send initial packet to server
        FastVector::ByteVector init_msg(reinterpret_cast<const char*>("Hello Server"),
                                        reinterpret_cast<const char*>("Hello Server") + 12);
        sendToServer(init_msg);
    }

protected:
    void handleMessage(const asio::ip::udp::endpoint& /*sender*/, const FastVector::ByteVector& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        audioBuffer_.insert(audioBuffer_.end(), message.begin(), message.end());
    }

private:
    bool initializeAudio() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            LOG_ERROR("PortAudio error: %s", Pa_GetErrorText(err));
            return false;
        }

        err = Pa_OpenDefaultStream(&stream_, 0, 2, paInt16, 44100, paFramesPerBufferUnspecified,
                                   audioCallback, this);
        if (err != paNoError) {
            LOG_ERROR("PortAudio error: %s", Pa_GetErrorText(err));
            Pa_Terminate();
            return false;
        }

        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            LOG_ERROR("PortAudio error: %s", Pa_GetErrorText(err));
            Pa_CloseStream(stream_);
            Pa_Terminate();
            return false;
        }

        return true;
    }

    static int audioCallback(const void* inputBuffer, void* outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData) {
        AudioUDPClient* client = static_cast<AudioUDPClient*>(userData);
        int16_t* out = static_cast<int16_t*>(outputBuffer);

        std::lock_guard<std::mutex> lock(client->mutex_);
        size_t bytesToCopy = std::min(static_cast<unsigned long>(client->audioBuffer_.size()), framesPerBuffer * 4);
        if (bytesToCopy > 0) {
            memcpy(out, client->audioBuffer_.data(), bytesToCopy);
            client->audioBuffer_.erase(client->audioBuffer_.begin(), client->audioBuffer_.begin() + bytesToCopy);
        } else {
            memset(out, 0, framesPerBuffer * 4);
        }

        return paContinue;
    }

    PaStream* stream_;
    FastVector::ByteVector audioBuffer_;
    std::mutex mutex_;
};

