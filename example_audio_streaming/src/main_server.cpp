#include "AudioServer.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        AudioUDPServer server(argv[1]);
        server.start();
        // Keep the server running
        std::cout << "Server running. Press Enter to stop..." << std::endl;
        std::cin.get();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}