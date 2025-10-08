#include "../include/helpers.h"
#include "../include/server.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <inttypes.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <cstring>

constexpr int8_t SOH = 0x01;
constexpr int8_t STX = 0x02;
constexpr int8_t ETX = 0x03;

constexpr u_int8_t BACKLOG = 5; // Max length of queue of waiting connections

int16_t send_stream(std::string stream_str, int64_t client_fd) {
    constexpr int16_t MAX_STREAM_LEN = 5000;

    uint8_t reserved =
        sizeof(SOH) + sizeof(stream_str.length()) + sizeof(STX) + sizeof(ETX);
    if (stream_str.length() > MAX_STREAM_LEN - reserved) {
        stream_str.resize(MAX_STREAM_LEN - reserved);
    }

    // Create data
    std::vector<char> stream;
    stream.push_back(SOH);
    stream.push_back(stream_str.length());
    stream.push_back(STX);
    for (char c : stream_str) {
        stream.push_back(c);
    }
    stream.push_back(ETX);

    // Convert data to network byte order
    for (int i = 0; i < stream.size(); i++) {
        stream[i] = htons(stream[i]);
    }

    // Send stream
    int16_t total_sent = 0, sent = 0;
    while (total_sent < stream.size()) {
        sent = send(client_fd, stream.data() + total_sent,
                    stream.size() - total_sent, 0);
        if (sent < 0) {
            write_error("Failed to send stream to client " +
                        std::to_string(client_fd));
            return -1;
        }
        total_sent += sent;
    }

    return 0;
}

int16_t send_helo(int64_t client_fd) {
    return send_stream("HELO,A5_9", client_fd);
}

int16_t recv_helo() { return 0; }

int16_t send_keepalive(int16_t msg_num) { return 0; }

int16_t get_msg(int16_t group_id) { return 0; }

int16_t send_msg(int16_t to_group, int16_t from_group) { return 0; }

int16_t req_status() { return 0; }

int16_t resp_status() { return 0; }

// TODO: Change this func, so it handles read stuff
void handle_client(int64_t client_fd) {
    // Read from client and add switch case to determine which server command
    // function to execute
    std::cout << "Client: " << client_fd << " called\n";

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr
            << "Port argument must be specified\nExample: ./tsamgroup9 4009\n";
        return -1;
    }
    int16_t port = atoi(argv[1]);

    Server serv(port);
    std::cout << "Server started. Listening on port " << port << "\n";

    bool finished = false;
    int16_t num_events;
    while (!finished) {
        int num_events = serv.wait_for_events();
        for (int16_t i = 0; i < num_events; ++i) {
            if (serv.check_for_new_client(i)) {
                serv.register_client();
                serv.dispatch(send_helo, serv.get_event_socket(i));
            } else {
        
                // If EPOLLIN is set, it's ready to read
                if (serv.GetEventStatus(i) & EPOLLIN != 0) {
                    serv.Dispatch(handle_client, serv.GetEventSocket(i));
                } 
                
                // If EPOLLOUT is set, it's ready to write
                if (serv.GetEventStatus(i) & EPOLLOUT != 0) {
                    continue; // Handle writing the sockets (send to ppl) - Change later
                }
            }
        }
    }
    return 0;
}
