#include "AudioClient.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        AudioUDPClient client(argv[1]);
        client.start();
        // Keep the client running
        std::cout << "Client running. Press Enter to stop..." << std::endl;
        std::cin.get();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}