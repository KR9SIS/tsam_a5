#include "helpers.h"
#include "server.hpp"
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

int16_t send_stream(const std::string& stream_str, Client& client) {
    constexpr int16_t MAX_STREAM_LEN = 5000;

    uint8_t frame_size = sizeof(SOH) + sizeof(uint16_t) + sizeof(STX) + sizeof(ETX);

    if(stream_str.length() > MAX_STREAM_LEN - frame_size){
        write_error("Tried To Send A Message Exceeding Max Length");
        return -1;
    }

    uint16_t total_packet_size = stream_str.length() + frame_size;
    total_packet_size = htons(total_packet_size);

    // Create data
    std::vector<char> stream;
    stream.push_back(SOH);
    stream.push_back(total_packet_size >> 0x08);
    stream.push_back(total_packet_size & 0x08);
    stream.push_back(STX);
    for (const char& c : stream_str) {
        stream.push_back(c);
    }
    stream.push_back(ETX);

    while (!client.commands.empty()) {
        // Resend failed streams
        Client::Msg msg = client.commands.front();
        int16_t sent = send(client.fd, msg.stream_data + msg.total_sent,
                            msg.length - msg.total_sent, 0);
        if (sent + msg.total_sent < msg.length) {
            if (sent < 0) {
                write_error("Failed to send stream to client " +
                            std::to_string(client.fd));
            } else {
                msg.total_sent += sent;
            }

            Client::Msg new_msg;
            new_msg.total_sent = 0;
            new_msg.length = total_packet_size;
            new_msg.stream_data = stream.data();
            client.commands.push(new_msg);

            return -1;
        }
        client.commands.pop();
    }

    int16_t sent;
    if ((sent = send(client.fd, stream.data(), total_packet_size, 0)) < total_packet_size) {
        if (sent < 0) {
            write_error("Failed to send stream to client " +
                        std::to_string(client.fd));
            sent = 0;
        }
        Client::Msg new_msg;
        new_msg.total_sent = sent;
        new_msg.length = total_packet_size;
        new_msg.stream_data = stream.data();
        client.commands.push(new_msg);

        return -1;
    }

    return 0;
}

int16_t send_helo(Client& client) { return send_stream("HELO,A5_9", client); }

int16_t recv_helo(Client& client, Server& server, const std::string& name) { 
    client.name = name;
    std::map<int64_t, Client>& clients = server.get_server_clients();
    std::string message("SERVER,");
    for(auto it = clients.begin(); it != clients.end(); ++it){
        std::string address;
        if(!inet_ntop(AF_INET, &it->second.addr.sin_addr, address.data(), 15)){
            write_error(std::string("IP Address Of Client ").append(it->second.name).append(" Could Not Be Translated To String"));
            continue;
        }

        message.append(name).append(",");
        message.append(address).append(",");
        message.append(std::to_string(ntohs(it->second.addr.sin_port))).append(";");
    }
    return send_stream(message, client);
}

int16_t send_keepalive(std::map<int64_t, Client>& clients) {
    int16_t num_failures = 0;
    for(auto it = clients.begin(); it != clients.end(); ++it){
        if(send_stream(std::string("KEEPALIVE,")
                       .append(std::to_string(it->second.pending_messages)),
                       it->second) == -1){
            write_error(std::string("Failed To Send KEEPALIVE To").append(it->second.name));
            num_failures += 1;
        }
    }

    return num_failures;
}

int16_t recieve_keepalive(Client& client, uint16_t num_messages){
    client.pending_messages = num_messages;

    if(num_messages > 0){
        client.status = ClientState::ACTIVE;
    }

    return 0;
}

int16_t get_msg(Client& client, Server& server){
    if(client.name != "A5_9"){
        write_error("Client With Insufficient Privileges To GETMSG Made An Attempt");
        return -1;
    }

    return send_stream(server.GetMessageForCnC(), client); 
}

int16_t get_msg(int16_t group_id) { return 0; }

int16_t send_msg(int16_t to_group, int16_t from_group, const std::string& message) {
    if(to_group == 9){
        
    }
    return 0; 
}

int16_t req_status() { return 0; }

int16_t resp_status(std::map<int64_t, Client>& clients) { return 0; }

void message_handler(std::string command) {
    // Check which command and call correct function

    // Send back to handle_client what it got from the function
}

std::vector<std::string> extract_frames(const std::string& message) {
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

std::vector<std::string> extract_payload(const std::vector<std::string>& messages) {
    std::vector<std::string> commands;

    for (int i = 0; i < messages.size(); i++) {
        std::string message = messages[i];
        size_t stx_pos = message.find(STX, 0);       // Start of command
        size_t end_pos = message.find(STX, stx_pos); // Start of next command

        std::string command =
            message.substr(stx_pos - 1, end_pos - stx_pos - 2);
        commands.push_back(command);
    }

    return commands;
}

// TODO: Change this func, so it handles read stuff
void handle_client(Client& client, Server& serv) {
    // Read from client and call message_handler
    // Add framing to message and send
    std::cout << "Client: " << client.fd << " called\n";

    char buffer[5000] = {0};

    while (true) {
        int bytesRead = read(client.fd, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            break;
        }

        // TODO: Unframe
        std::string message(buffer);

        std::vector<std::string> messages = extract_frames(message);
        std::vector<std::string> commands = extract_payload(messages);

        // TODO: Call message_handler
        // NEED TO FIX!!
        for (int i = 0; i < commands.size(); i++) {
            std::string command = commands[i];
            message_handler(command);
        }

        // TODO: call sendstream?
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

    Server serv(port);
    std::cout << "Server started. Listening on port " << port << "\n";

    bool finished = false;
    int16_t num_events;
    while (!finished) {
        int16_t num_events = serv.wait_for_events();

        // NOTE: Had To Add std::ref() here because 
        //       otherwise the thread does not get the real reference but a copy of it

        for (int16_t i = 0; i < num_events; ++i) {
            if (serv.check_for_new_client(i)) {
                serv.register_client();
                serv.dispatch(send_helo, std::ref(serv.get_server_client(serv.get_event_socket(i))));
            } else {

                // If EPOLLIN is set, it's ready to read
                if ((serv.get_event_status(i) & EPOLLIN) != 0) {
                    serv.dispatch(handle_client, std::ref(serv.get_server_client(serv.get_event_socket(i))),std::ref(serv));
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
