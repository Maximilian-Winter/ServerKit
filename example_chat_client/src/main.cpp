//
// Created by maxim on 21.08.2024.
//

#include "ChatClient.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <username>" << std::endl;
        return 1;
    }

    try {
        ChatClient client(argv[1], argv[2]);
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}