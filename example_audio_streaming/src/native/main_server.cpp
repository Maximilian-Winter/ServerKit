#include <iostream>
#include <string>
#include <vector>
#include <asio.hpp>
#include <memory>
#include <windows.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

using asio::ip::udp;

class FFmpegUDPServer : public std::enable_shared_from_this<FFmpegUDPServer> {
public:
    FFmpegUDPServer(asio::io_context& io_context, short port)
            : socket_(io_context, udp::endpoint(udp::v4(), port)),
              io_context_(io_context),
              running_(false),
              client_connected_(false) {
    }

    bool start(const std::string& input_file) {
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hReadPipe, hWritePipe;

        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            std::cerr << "Failed to create pipe" << std::endl;
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
            std::cerr << "Failed to start ffmpeg process" << std::endl;
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return false;
        }

        CloseHandle(hWritePipe);

        running_ = true;
        read_pipe_thread_ = std::thread([this, hReadPipe]() {
            read_pipe(hReadPipe);
        });

        start_receive();
        return true;
    }

    ~FFmpegUDPServer() {
        running_ = false;
        if (read_pipe_thread_.joinable()) {
            read_pipe_thread_.join();
        }
    }

private:
    void read_pipe(HANDLE hPipe) {
        const int BUFFER_SIZE = 65536;
        std::vector<char> buffer(BUFFER_SIZE);
        DWORD bytesRead;

        while (running_) {
            if (ReadFile(hPipe, buffer.data(), BUFFER_SIZE, &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this] { return client_connected_ || !running_; });
                    if (!running_) break;

                    asio::post(io_context_, [this, data = buffer, bytes = bytesRead]() {
                        socket_.async_send_to(
                                asio::buffer(data.data(), bytes), remote_endpoint_,
                                [this](std::error_code ec, std::size_t bytes_sent) {
                                    if (ec) {
                                        std::cerr << "Send error: " << ec.message() << std::endl;
                                        client_connected_ = false;
                                        start_receive();
                                    }
                                });
                    });
                }
            } else {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    std::cout << "FFmpeg process has ended the stream." << std::endl;
                    break;
                } else {
                    std::cerr << "ReadFile failed with error " << error << std::endl;
                    break;
                }
            }
        }

        CloseHandle(hPipe);
    }

    void start_receive() {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), remote_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec) {
                    if (!client_connected_) {
                        std::cout << "Received data from client. Starting stream..." << std::endl;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            client_connected_ = true;
                        }
                        cv_.notify_one();
                    }
                    // Process received data if needed

                    // Set up the next receive operation
                    start_receive();
                } else {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                    start_receive();
                }
            });
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 1024> recv_buffer_;
    asio::io_context& io_context_;
    std::atomic<bool> running_;
    std::thread read_pipe_thread_;
    std::atomic<bool> client_connected_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <input_file>" << std::endl;
        return 1;
    }

    try {
        asio::io_context io_context;
        auto server = std::make_shared<FFmpegUDPServer>(io_context, std::stoi(argv[1]));

        if (server->start(argv[2])) {
            io_context.run();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}