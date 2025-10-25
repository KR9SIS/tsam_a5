#include "server.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <unistd.h>


constexpr u_int8_t BACKLOG = 5; // Max length of queue of waiting connections

int main(int argc, char **argv) {
    // Gets in the arguments needed when server is executed and starts the server
    if (argc != 2) {
        std::cerr << "Port argument must be specified\nExample: ./tsamgroup9 4009\n";
        return -1;
    }
    int16_t port = atoi(argv[1]);

    new_file("SERVER");

    Server server(port);
    server_log_message("A5_9", "SUCCESS", "Server started. Listening on port: " + std::to_string(port));

    server.start(); // Start the server
    return 0;
}
