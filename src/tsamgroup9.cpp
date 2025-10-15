#include "helpers.h"
#include "server.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>


// std::map<int64_t, std::vector<char>> client_messages;

constexpr u_int8_t BACKLOG = 5; // Max length of queue of waiting connections

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Port argument must be specified\nExample: "
                     "./tsamgroup9 4009\n";
        return -1;
    }
    int16_t port = atoi(argv[1]);

    Server server(port);
    std::cout << "Server started. Listening on port " << port << "\n";

    server.start();
    return 0;
}
