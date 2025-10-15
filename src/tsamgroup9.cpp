#include "helpers.h"
#include "server.hpp"
#include <../logs/client_logs.hpp>
#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

constexpr int8_t SOH = 0x01;
constexpr int8_t STX = 0x02;
constexpr int8_t ETX = 0x03;

// std::map<int64_t, std::vector<char>> client_messages;

constexpr u_int8_t BACKLOG = 5; // Max length of queue of waiting connections

int16_t send_stream(const std::string &stream_str, Client &client) {
    constexpr uint16_t frame_overhead =
        1 /*SOH*/ + 2 /*len*/ + 1 /*STX*/ + 1 /*ETX*/;
    constexpr uint16_t MAX_STREAM_LEN = 5000;

    if (stream_str.size() > MAX_STREAM_LEN - frame_overhead) {
        write_error("Tried To Send A Message Exceeding Max Length");
        log_message("ERROR", "Tried to send a message exceeding max length");
        return -1;
    }

    uint16_t total_len =
        static_cast<uint16_t>(stream_str.size() + frame_overhead);
    uint16_t net_len = htons(total_len);

    std::vector<char> stream;
    stream.reserve(total_len);
    stream.push_back(SOH);
    stream.push_back(static_cast<char>((net_len >> 8) & 0xFF)); // high byte
    stream.push_back(static_cast<char>(net_len & 0xFF));        // LOW byte
    stream.push_back(STX);
    stream.insert(stream.end(), stream_str.begin(), stream_str.end());
    stream.push_back(ETX);

    // Use the vector's size (host order) as the number of bytes to send
    ssize_t sent = send(client.fd, stream.data(), stream.size(), 0);
    if (sent < 0 || static_cast<size_t>(sent) != stream.size()) {
        write_error("Failed to send stream(" + stream_str + ") to client " +
                    std::to_string(client.fd));
        log_message("ERROR", "Failed to send message: " + stream_str);
        return -1;
    }
    log_message("SENT", stream_str);
    return 0;
}

int16_t send_helo(Client &client) {
    //
    return send_stream("HELO,A5_9", client);
}

int16_t recv_helo(Client &client, Server &server, const std::string &name) {
    log_message("RECV_HELO", name);
    client.name = name;
    std::map<int64_t, Client> &clients = server.get_server_clients();
    std::string message("SERVER,");
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        std::string address;
        if (!inet_ntop(AF_INET, &it->second.addr.sin_addr, address.data(),
                       15)) {
            write_error(std::string("IP Address Of Client ")
                            .append(it->second.name)
                            .append(" Could Not Be Translated To String"));
            continue;
        }

        message.append(name).append(",");
        message.append(address).append(",");
        message.append(std::to_string(ntohs(it->second.addr.sin_port)))
            .append(";");
    }
    return send_stream(message, client);
}

int16_t send_keepalive(std::map<int64_t, Client> &clients) {
    int16_t num_failures = 0;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (send_stream(
                std::string("KEEPALIVE,")
                    .append(std::to_string(it->second.pending_messages)),
                it->second) == -1) {
            write_error(std::string("Failed To Send KEEPALIVE To")
                            .append(it->second.name));
            num_failures += 1;
        }
    }

    return num_failures;
}

int16_t recieve_keepalive(Client &client, uint16_t num_messages) {
    client.pending_messages = num_messages;

    if (num_messages > 0) {
        client.status = ClientState::ACTIVE;
    }

    return 0;
}

int16_t get_msg(Client &client, Server &server) {
    if (client.name != "A5_9") {
        write_error(
            "Client With Insufficient Privileges To GETMSG Made An Attempt");
        return -1;
    }

    return send_stream(server.get_message_for_cnc(),
                       client); // WARNING: PROBABLY NOT BE THREAD SAFE!!!!
}

// int16_t get_msg(int16_t group_id) { return 0; }

int16_t send_msg(Server &server, int16_t to_group, int16_t from_group,
                 const std::string &message) {
    if (to_group == 9) {
        server.store_message_for_cnc(
            message); // WARNING: PROBABLY NOT BE THREAD SAFE!!!!
    }
    return 0;
}

int16_t req_status() { return 0; }

int16_t resp_status(std::map<int64_t, Client> &clients) { return 0; }

std::vector<std::string> extract_parameters(const std::string payload) {
    std::vector<std::string> params;
    size_t delim_pos = payload.find(",");
    while (delim_pos != std::string::npos) {
        size_t next_delim = payload.find(",", delim_pos);
        if (next_delim == std::string::npos) {
            params.push_back(payload.substr(delim_pos));
            break;
        }
        params.push_back(payload.substr(delim_pos, next_delim - delim_pos));
    }

    return params;
}

void message_handler(Client &client, Server &server,
                     const std::string &command) {
    log_message("RESPONSE", client.name + " sent " + command);
    if (command.find("GETMSG") != std::string::npos) {
        get_msg(client, server);
    }
    if (command.find("SENDMSG") != std::string::npos) {
        std::vector<std::string> parameters = extract_parameters(command);
        if (parameters.size() < 3) {
            write_error("Wrong Number Of Parameters Passed To SENDMSG Aborting "
                        "OPERATION...");
            return;
        }
        send_msg(server, std::stoi(parameters[0]), std::stoi(parameters[1]),
                 parameters[2]);
    }
}

std::vector<std::string> extract_frames(const std::string &message) {
    // Extract message frames from buffer
    std::vector<std::string> frames;
    size_t start = 0;

    while (start < message.size()) {
        size_t soh = message.find(SOH, start);
        if (soh == std::string::npos)
            break;

        // Need at least SOH + 2 len + STX
        if (soh + 4 >= message.size())
            break;

        uint16_t len = (static_cast<unsigned char>(message[soh + 1]) << 8) |
                       static_cast<unsigned char>(message[soh + 2]);

        // Full frame is len bytes starting at SOH
        if (soh + len > message.size())
            break; // Partial frame - wait for more

        frames.push_back(message.substr(soh, len));
        start = soh + len;
    }
    return frames;
}

std::vector<std::string>
extract_payload(const std::vector<std::string> &messages) {
    std::vector<std::string> payloads;
    for (const std::string &f : messages) {
        // f = SOH | len_hi | len_lo | STX | payload (message) | ETX
        if (f.size() < 5)
            continue;
        if (static_cast<unsigned char>(f[0]) != SOH)
            continue;
        if (static_cast<unsigned char>(f[3]) != STX)
            continue;
        if (static_cast<unsigned char>(f.back()) != ETX)
            continue;

        payloads.push_back(f.substr(4, f.size() - 5)); // between STX and ETX
    }
    return payloads;
}

// Read from client, unframe, and call message_handler
void handle_client(Client &client, Server &serv) {
    // Add framing to message and send
    std::cout << "Client: " << client.fd << " called\n";

    char buffer[5000] = {0};

    while (true) {
        int bytes_read = read(client.fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            break;
        }

        // Unframe
        std::string message(buffer, static_cast<size_t>(bytes_read));

        std::vector<std::string> messages = extract_frames(message);
        std::vector<std::string> commands = extract_payload(messages);

        // Call message handler on every message
        for (int i = 0; i < commands.size(); i++) {
            std::string command = commands[i];
            message_handler(client, serv, command);
        }
    }

    close(client.fd); // Might have to remove
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Port argument must be specified\nExample: "
                     "./tsamgroup9 4009\n";
        return -1;
    }
    int16_t port = atoi(argv[1]);

    Server server(port);
    std::cout << "Server started. Listening on port " << port << "\n";

    bool finished = false;
    int16_t num_events;
    while (!finished) {
        int16_t num_events = server.wait_for_events();

        // NOTE: Had To Add std::ref() here because
        //       otherwise the thread does not get the real reference but a copy
        //       of it

        for (int16_t i = 0; i < num_events; ++i) {
            if (server.check_for_new_client(i)) {
                Client *new_client = server.register_client();
                log_message("REGISTER", new_client->name);
                server.dispatch(send_helo, std::ref(*new_client));
            } else {

                // If EPOLLIN is set, it's ready to read
                if ((server.get_event_status(i) & EPOLLIN) != 0) {
                    Client client =
                        server.get_server_client(server.get_event_socket(i));
                    log_message("EPOLLIN", client.name);
                    server.dispatch(handle_client, std::ref(client),
                                    std::ref(server));
                }

                // If EPOLLOUT is set, it's ready to write
                if ((server.get_event_status(i) & EPOLLOUT) != 0) {
                    continue; // Handle writing the sockets (send to ppl) -
                    // Change later
                }
            }
        }
    }
    return 0;
}
