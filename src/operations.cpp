#ifndef SERVEROPERATIONS
#define SERVEROPERATIONS

#include "operations.hpp"
#include "client_logs.hpp"
#include "helpers.h"
#include "server.hpp"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <thread>

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
    if (command.find("HELO") != std::string::npos) {
        std::vector<std::string> parameters = extract_parameters(command);
        if (parameters.size() < 1) {
            write_error("Wrong Number Of Parameters Passed To HELO Aborting "
                        "OPERATION...");
            return;
        }
        recv_helo(client, server, parameters[0]);
    }
}

void handle_client_read(Client &client, Server &serv) {
    // Add framing to message and send
    char buffer[5000];

    while (serv.get_server_clients().contains(client.fd)) {
        // NOTE: Wait For Read Event To Be Set
        while ((client.get_event_status() & EPOLLIN) == 0) {
            std::this_thread::yield();
        }

        int bytes_read = recv(client.fd, buffer, sizeof(buffer), 0);
        log_message("PROCESSING",
                    "Received " + std::to_string(bytes_read) + " Bytes");
        if (bytes_read <= 0) {
            log_message("INFO", "Nothing To Read Exiting handle_client");
            break;
        }

        // Unframe
        std::string message(buffer, static_cast<size_t>(bytes_read));
        log_message("PROCESSING", message);

        std::vector<std::string> messages = extract_frames(message);
        std::vector<std::string> commands = extract_payload(messages);

        log_message("INFO", "Received " + std::to_string(commands.size()) +
                                " Commands");
        // Call message handler on every message
        for (auto it = commands.begin(); it != commands.end(); ++it) {
            message_handler(client, serv, *it);
        }

        client.reset_read();
    }

    close(client.fd); // Might have to remove
}

void handle_client_write(Client &client, Server &serv) {
    // Add framing to message and send
    char buffer[5000];

    while (serv.get_server_clients().contains(client.fd)) {
        // NOTE: Wait For Write Event To Be Set
        while ((client.get_event_status() & EPOLLOUT) == 0) {
            std::this_thread::yield();
        }
        // TODO: MAKE LOGIC FOR SENDING MESSAGES
    }

    close(client.fd); // Might have to remove
}
#endif
