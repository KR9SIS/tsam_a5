#include "../include/helpers.h"
#include "../include/server.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

constexpr int8_t SOH = 0x01;
constexpr int8_t STX = 0x02;
constexpr int8_t ETX = 0x03;

std::map<int64_t, std::vector<char>> client_messages;

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
    for (uint16_t i = 0; i < stream.size(); i++) {
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

void message_handler(int16_t id, char *message) {
    // Check which command and call correct function

    // Send back to handle_client what it got from the function
}

std::vector<std::string> extract_frames(std::string message) {
    // Extract message frames from buffer

    std::vector<std::string> commands;
    size_t start = 0;

    while (start < message.size()) {
        size_t soh_pos = message.find(SOH, start);
        size_t etx_pos = message.find(ETX, soh_pos);

        if (soh_pos != std::string::npos && etx_pos != std::string::npos) {
            std::string frame =
                message.substr(soh_pos + 1, etx_pos - soh_pos - 1);
            commands.push_back(frame);
            start = etx_pos + 1;
        } else {
            break;
        }
    }
    return commands;
}

// TODO: Change this func, so it handles read stuff
void handle_client(int64_t client_fd) {
    // Read from client and call message_handler
    // Add framing to message and send
    std::cout << "Client: " << client_fd << " called\n";

    char buffer[5000] = {0};

    while (true) {
        int bytesRead = read(client_fd, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            break;
        }

        // TODO: Unframe
        std::string message = buffer;

        std::vector<std::string> messages = extract_frames(message);

        // TODO: Call message_handler

        // TODO: Frame message

        // TODO: Send framed message
    }

    close(client_fd); // Might have to remove
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
        int16_t num_events = serv.wait_for_events();
        for (int16_t i = 0; i < num_events; ++i) {
            if (serv.check_for_new_client(i)) {
                serv.register_client();
                serv.dispatch(send_helo, serv.get_event_socket(i));
            } else {

                // If EPOLLIN is set, it's ready to read
                if ((serv.get_event_status(i) & EPOLLIN) != 0) {
                    serv.dispatch(handle_client, serv.get_event_socket(i));
                }

                // If EPOLLOUT is set, it's ready to write
                if ((serv.get_event_status(i) & EPOLLOUT) != 0) {
                    continue; // Handle writing the sockets (send to ppl) -
                              // Change later
                }
            }
        }
    }
    return 0;
}
