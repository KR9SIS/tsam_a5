#ifndef SERVEROPERATIONS
#define SERVEROPERATIONS

#include "operations.hpp"
#include "client_logs.hpp"
#include "helpers.h"
#include "server.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <map>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <vector>
#include <cstring>

int64_t open_client_socket(Server &server, std::string ip, int16_t port, std::string name) {
    log_message("PROCESSING", "Opening client socket " + ip + ":" + std::to_string(port));
    int64_t sock;
    if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        log_message("ERROR", "Creating socket for client at " + ip + ":" + std::to_string(port) + " failed");
        return -1;
    }
    Client client;
    client.name = name;
    //client.ip = ip;
    client.client_num = server.get_server_clients().size();
    client.fd = sock;
    socklen_t client_add_len = sizeof(client.addr);

    client.addr.sin_family = AF_INET;
    client.addr.sin_port = htons(port);
    if ((inet_pton(AF_INET, ip.c_str(), &client.addr.sin_addr) != 1)) {
        log_message("ERROR", "Failed converting ip address to binary form for client at " + ip + ":" + std::to_string(port));
        return -1;
    }

    // Connect to server
    if (connect(sock, (const sockaddr *)&client.addr, sizeof(client.addr)) < 0) {
        if (errno == EINPROGRESS) {
            log_message("PROCESSING", "Wait for epoll. Connect returned EINPROGRESS for client at " + ip + ":" + std::to_string(port));
        } else {
            write_error("Failed connecting to client at " + ip + ":" + std::to_string(port));
            log_message("ERROR", "Failed connecting to client at " + ip + ":" + std::to_string(port));
            return -1;
        }
    }

    server.get_server_clients().insert(std::pair<int64_t, Client>(client.fd, client));

    server.client_names.insert(name);

    Client &stored_client = server.get_server_client(client.fd);

    server.add_to_epoll(client.fd);

    server.dispatch(handle_client_read, std::ref(stored_client), std::ref(server)); // Dispatch READ Thread
    server.dispatch(handle_client_write, std::ref(stored_client), std::ref(server)); // Dispatch WRITE Thread
    send_helo(stored_client);

    return client.fd;
}

void send_helos(Server &server) {
    while (true) {
        for (auto &[_, client] : server.get_server_clients()) {
            send_helo(client);
        }
        sleep(120);
    }
}
void send_helo(Client &client) {
    //log_message("PROCESSING", "Sending HELO to " + client.name);
    client.push_message("HELO,A5_9");
}

int16_t recv_helo(Client &client, Server &server, const std::string &name) {
    //log_message("RECV_HELO", name);
    client.name = name;
    std::map<int64_t, Client> &clients = server.get_server_clients();
    std::string message("SERVERS,");

    // Append itself first:
    message.append("A5_9,");
    message.append("130.208.246.98").append(","); // WARNING: CHANGE THE HARD CODING!!!!!!!!!!!!!!!!!!
    message.append(std::to_string(ntohs(server.sk_addr.sin_port))).append(";");

    for (auto it = clients.begin(); it != clients.end(); ++it) {
        std::string address;
        if (!inet_ntop(AF_INET, &it->second.addr.sin_addr, address.data(), 15)) {
            write_error(std::string("IP Address Of Client ")
                            .append(it->second.name)
                            .append(" Could Not Be Translated To String"));
            continue;
        }

        message.append(it->second.name).append(",");
        message.append(inet_ntoa(it->second.addr.sin_addr)).append(",");
        message.append(std::to_string(ntohs(it->second.addr.sin_port))).append(";");
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
        write_error("Client With Insufficient Privileges To GETMSG Made An Attempt");
        return -1;
    }

    return send_stream(server.get_message_for_cnc(), client);
}

int16_t send_msg(Server &server, std::string to_group, std::string from_group, const std::string &message) {
    if (to_group == "A5_9") {
        server.store_message_for_cnc(message);
    }

    std::map<int64_t, Client> &clients = server.get_server_clients();
    for (auto &[_, client] : clients) {
        if (client.name == to_group) {
            client.push_message(message);
            return 0;
        }
        for (auto &[_, conn] : client.connections) {
            if (conn.name == to_group) {
                client.push_message(message);
                return 0;
            }
        }
    }

    std::string msg_copy = message;
    ssize_t hops_idx = msg_copy.find('\x04');
    if (hops_idx == std::string::npos) {
        msg_copy.push_back('\x04');
        msg_copy.append("A5_9");
        clients.begin()->second.push_message(msg_copy);
    } else {
        std::vector<std::string> recipients = extract_parameters(msg_copy.substr(hops_idx), ",");
        for (auto &[_, client] : clients) {
            if (std::find(recipients.begin(), recipients.end(), client.name) == recipients.end()) {
                msg_copy.append(",A5_9");
                client.push_message(msg_copy);
                break;
            }
        }
    }

    return 0;
}

// WARNING: IMPLEMENT
int16_t req_status() { return 0; }
int16_t resp_status(std::map<int64_t, Client> &clients) { return 0; }

void message_handler(Client &client, Server &server, const std::string &command) {
    log_message("RESPONSE", client.name + " sent " + command);

    if (command.rfind("SERVERS", 0) == 0) {
        std::string rest = command.substr(strlen("SERVERS,"));
        
        std::vector<std::string> client_conn = extract_parameters(rest, ";");

        client.update_connections(client_conn);

        if (server.get_server_clients().size() < server.max_clients) {
            for (auto &[_, client_conn] : client.connections) {
                if (server.client_names.find(client_conn.name) != server.client_names.end()) {
                    open_client_socket(server, client_conn.ip, client_conn.port, client_conn.name);
                }
                if (server.client_names.size() == server.max_clients) {
                    break;
                }
            }
        }
    }
    if (command.rfind("GETMSG", 0) == 0) {
        get_msg(client, server);
    }
    if (command.rfind("SENDMSG", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");
        if (parameters.size() <= 3) { // Parameters need to be 4
            write_error("Wrong Number Of Parameters Passed To SENDMSG Aborting OPERATION...");
            return;
        }

        send_msg(server, parameters[1], parameters[2], parameters[3]);
    }
    if (command.rfind("HELO", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");

        if (parameters.size() < 1) {
            write_error("Wrong Number Of Parameters Passed To HELO Aborting OPERATION...");
            return;
        }
        recv_helo(client, server, parameters[1]); // So it does not store HELO as name - MIGHT HAVE TO CHANGE
    }
}

void handle_client_read(Client &client, Server &serv) {
    // Add framing to message and send
    char buffer[5000];

    log_message("PROCESSING", "Handling read for " + client.name);

    while (serv.get_server_clients().contains(client.fd)) {
        // NOTE: Wait For Read Event To Be Set
        while ((client.get_event_status() & EPOLLIN) == 0) {
            std::this_thread::yield();
        }

        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(client.fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
            write_error("getsockopt threw error for " + client.name);
            log_message("ERROR", "getsockopt threw error for " + client.name);
            return;
        }

        int bytes_read = recv(client.fd, buffer, sizeof(buffer), 0);
        //log_message("PROCESSING", "Received " + std::to_string(bytes_read) + " Bytes");
        if (bytes_read <= 0) {
            log_message("INFO", "Nothing To Read Exiting handle_client");
            break;
        }

        // Unframe
        std::string message(buffer, static_cast<size_t>(bytes_read));
        //log_message("PROCESSING", message);

        std::vector<std::string> messages = extract_frames(message);

        std::vector<std::string> commands = extract_payload(messages);

        log_message("INFO", "Received " + std::to_string(commands.size()) + " Commands");
        // Call message handler on every message
        for (auto it = commands.begin(); it != commands.end(); ++it) {
            message_handler(client, serv, *it);
        }

        client.reset_read();
    }
}

void handle_client_write(Client &client, Server &serv) {
    // Add framing to message and send
    char buffer[5000];
    // && !client.outbound_messages.empty()

    log_message("PROCESSING", "Handling write for " + client.name);

    while (serv.get_server_clients().contains(client.fd)) {
        // NOTE: Wait For Write Event To Be Set
        while ((client.get_event_status() & EPOLLOUT) == 0) {
            std::this_thread::yield();
        }

        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(client.fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
            write_error("getsockopt threw error for " + client.name);
            log_message("ERROR", "getsockopt threw error for " + client.name);
            return;
        }

        std::string msg = client.front_message();

        if (msg != "") {
            if ((send_stream(msg, client)) != -1) {
                client.pop_message();
            };
        }
    }
}
#endif
