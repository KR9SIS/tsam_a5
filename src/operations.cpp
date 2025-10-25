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
#include <memory>
#include <mutex>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utility>
#include <vector>
#include <cstring>

int64_t open_client_socket(Server &server, std::string ip, int16_t port, std::string name, std::unique_lock<std::mutex>& lock) {
    server_log_message(name, "PROCESSING", "Opening client socket " + ip + ":" + std::to_string(port));
    int64_t sock;
    if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        server_log_message("null", "ERROR", "Creating socket for client at " + ip + ":" + std::to_string(port) + " failed", "open_client_socket");
        return -1;
    }
    std::shared_ptr<Client> client = std::make_shared<Client>();

    if (client->name == "tmp" && name != "tmp") {
        server_log_message(name, "INFO", "Updated client name: tmp to " + name);
    }
    client->name = name;
    //client.ip = ip;
    client->client_num = server.get_server_clients(lock).size();


    client->fd = sock;
    socklen_t client_add_len = sizeof(client->addr);

    client->addr.sin_family = AF_INET;
    client->addr.sin_port = htons(port);

    if ((inet_pton(AF_INET, ip.c_str(), &client->addr.sin_addr) != 1)) {
        server_log_message(name, "ERROR", "Failed converting ip address to binary form for client at " + ip + ":" + std::to_string(port), "open_client_socket");
        return -1;
    }

    // Connect to server
    if (connect(sock, (const sockaddr *)&client->addr, sizeof(client->addr)) < 0) {
        if (errno == EINPROGRESS) {
            server_log_message(name, "PROCESSING", "Wait for epoll. Connect returned EINPROGRESS for client at " + ip + ":" + std::to_string(port));
        } else {
            server_log_message(name, "ERROR","Failed connecting to client at " + ip + ":" + std::to_string(port), "open_client_socket");
            server_log_message(name, "ERROR", "Failed connecting to client at " + ip + ":" + std::to_string(port), "open_client_socket");
            return -1;
        }
    }

    server_log_message(client->name, "INFO", "Adding client");

    server.add_client(std::make_pair(client->fd, client), lock);
    server.client_names.insert(name);
    server.add_to_epoll(client->name, client->fd);

    server.dispatch(client_worker, std::ref(server), client->fd); // Dispatch READ Thread
    send_helo(*client);

    return client->fd;
}

void send_helos(Server &server) {
    std::unique_lock<std::mutex> server_lock(server.get_mutex());
    server_log_message("EVERYONE", "INFO", "Sending Helos To Everyone");
    while (true) {
        for (auto &[_, client] : server.get_server_clients(server_lock)) {
            send_helo(*client);
        }
        sleep(120);
    }
}

void send_statusreqs(Server &server) {
    std::unique_lock<std::mutex> server_lock(server.get_mutex());
    while (true) {
        for (auto &[_, client] : server.get_server_clients(server_lock)) {
            client->push_message("STATUSREQ");
        }
        sleep(200);
    }
}

void send_helo(Client &client) {
    //log_message("PROCESSING", "Sending HELO to " + client.name);
    client.push_message("HELO,A5_9");
}

int16_t recv_helo(int64_t client_fd, Server& server, const std::string &name, std::unique_lock<std::mutex>& lock) {
    std::shared_ptr<Client> client = server.get_server_client(client_fd, lock);
    if(!client){
        server_log_message("NULL","ERROR","recv_helo Fetched A Null(Deleted) Client");
        return -1;
    }
    server_log_message(name, "RECV_HELO", name);

    if (client->name == "tmp" && name != "tmp") {
        server_log_message(name, "INFO", "Updated client name: tmp to " + name);
    }
    client->name = name;
    std::map<int64_t, std::shared_ptr<Client>> clients = server.get_server_clients(lock);
    std::string message("SERVERS,");

    // Determine IP of itself
    char ip_str[INET_ADDRSTRLEN] = {0};

    if (server.sk_addr.sin_addr.s_addr != INADDR_ANY) {
        inet_ntop(AF_INET, &(server.sk_addr.sin_addr), ip_str, sizeof(ip_str));
    } else {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s >= 0) {
            sockaddr_in dst{};
            dst.sin_family = AF_INET;
            dst.sin_port = htons(80);
            inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
            if (connect(s, (sockaddr*)&dst, sizeof(dst)) == 0) {
                sockaddr_in local;
                socklen_t len = sizeof(local);
                if (getsockname(s, (sockaddr*)&local, &len) == 0) {
                    inet_ntop(AF_INET, &(local.sin_addr), ip_str, sizeof(ip_str));
                }
            }
            close(s);
        }
    }
    if (ip_str[0] == '\0') std::snprintf(ip_str, sizeof(ip_str), "127.0.0.1");

    // Append itself
    message.append(server.name).append(",");
    message.append(ip_str).append(",");
    message.append(std::to_string(ntohs(server.sk_addr.sin_port))).append(";");

    // Connected servers:
    for (auto &[_, c] : clients) {
        if (c == nullptr) {
            continue;
        }
        // Skip the client we just received HELO from itself (the C&C connection)
        if (c->addr.sin_addr.s_addr == 0 || c->addr.sin_port == 0)
            continue; // skip incomplete entries

        char peer_ip[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &(c->addr.sin_addr), peer_ip, sizeof(peer_ip))) {
            server_log_message(c->name, "ERROR",std::string("inet_ntop failed for client ").append(c->name), "recv_helo");
            continue;
        }

        message.append(c->name).append(",");
        message.append(peer_ip).append(",");
        message.append(std::to_string(ntohs(c->addr.sin_port))).append(";");
    }

    if(!lock.owns_lock()){
        lock.lock(); // Prevent Modification During Send
    }
    return send_stream(message, *client);
}


// Does the same thing as keepalive just minimises errors and sends at seperate occations not all at same time
int16_t send_keepalive_helper(Client &client) {
    if (client.fd < 0) return -1; // skip closed sockets

    client.append_message(std::string("KEEPALIVE,").append(std::to_string(client.pending_messages)));
    return 0;
}


int16_t recieve_keepalive(Client& client, uint16_t num_messages) {
    client.pending_messages = num_messages;

    return 0;
}

int16_t get_msgs(int64_t client_fd, Server &server, const std::string &group_name, std::unique_lock<std::mutex>& lock) {
    std::map<int64_t, std::shared_ptr<Client>> clients = server.get_server_clients(lock);
    std::shared_ptr<Client> client = server.get_server_client(client_fd, lock);

    if(!client){
        server_log_message("NULL","ERROR", "get_msgs Fetched A Null(Deleted) Client");
        return -1;
    }
    if (client->name == "A5_9") {
        return send_stream(server.get_message_for_cnc(lock), *client);
    } else {
        std::string messages;
        for (auto &[_, cli] : clients) {
            if (cli->name == group_name) {
                if (cli->messages_for_client.empty()) {
                    messages.append(group_name).append(" Has no messages...");
                } else {
                    for (std::string message : cli->messages_for_client) {
                        message.append(message).append(";");
                    }
                }
            }
        }
        return send_stream(messages, *client);
    }
}

std::string parse_send_msg(const std::string& message){
    size_t first = message.find(',');
    if (first == std::string::npos) return "";

    size_t second = message.find(',', first + 1);
    if (second == std::string::npos) return "";

    size_t third = message.find(',', second + 1);
    if (third == std::string::npos) return "";

    // Everything after the 3rd comma is the message
    return message.substr(third + 1);
}

int16_t send_msg(Server &server, std::string to_group, std::string from_group, const std::string &message, std::unique_lock<std::mutex>& lock) {
    std::string parsed_message = parse_send_msg(message);

    if (to_group == "A5_9") {
        std::string message_with_group = "[" + from_group + "] " + parsed_message;
        server.store_message_for_cnc(parsed_message, lock);
    }

    std::string message_with_group = "[FROM: " + from_group + "] " + parsed_message;
    std::map<int64_t, std::shared_ptr<Client>> clients = server.get_server_clients(lock);
    for (auto &[_, client] : clients) {
        if (client->name == to_group) {
            client->push_message(message);
            client->append_message(message_with_group);
            return 0;
        }
        for (auto &[_, conn] : client->connections) {
            if (conn.name == to_group) {
                client->push_message(message);
                client->append_message(message_with_group);
                return 0;
            }
        }
    }

    std::string msg_copy(message);
    ssize_t hops_idx = msg_copy.find('\x04');
    if (hops_idx == std::string::npos) {
        msg_copy.push_back('\x04');
        msg_copy.append("A5_9");
        clients.begin()->second->push_message(msg_copy);
    } else {
        std::vector<std::string> recipients = extract_parameters(msg_copy.substr(hops_idx), ",");
        for (auto &[_, client] : clients) {
            if (std::find(recipients.begin(), recipients.end(), client->name) == recipients.end()) {
                msg_copy.append(",A5_9");
                client->push_message(msg_copy);
                break;
            }
        }
    }

    return 0;
}


int16_t resp_status(Server& server, int64_t client_fd, std::unique_lock<std::mutex>& lock) {
    std::map<long, std::shared_ptr<Client>> clients = server.get_server_clients(lock);
    std::string resp("STATUSRESP,");
    for(auto client = clients.begin(); client != clients.end(); ++client){
        resp.append(client->second->name).append(",").append(std::to_string(client->second->pending_messages)).append(",");
    }

    std::shared_ptr<Client> client = server.get_server_client(client_fd, lock);
    if(!client){
        server_log_message("NULL", "ERROR", "resp_status Fetched A Null(Deleted) Client");
        return -1;
    }
    resp.append(client->name).append(",").append(std::to_string(client->pending_messages));

    if(!lock.owns_lock()){
        lock.lock();
    }
    return send_stream(resp, *client);
}

void message_handler(int64_t client_fd, Server &server, const std::string &command, std::unique_lock<std::mutex>& lock) {
    std::shared_ptr<Client> client = server.get_server_client(client_fd, lock);
    if(!client){
        server_log_message("NULL", "ERROR", "message_handler Fetched A Null(Deleted) Client");
        return;
    }
    server_log_message(client->name, "RESPONSE", client->name + " sent " + command);

    if (command.rfind("SERVERS", 0) == 0) {
        std::string rest = command.substr(strlen("SERVERS,"));

        std::vector<std::string> client_conn = extract_parameters(rest, ";");

        client->update_connections(client_conn);

        if (server.get_server_clients(lock).size() < server.max_clients) {
            for (auto &[_, client_conn] : client->connections) {
                if (server.client_names.find(client_conn.name) == server.client_names.end()) {
                    server_log_message(client->name, "INFO", "Connecting To Client [IP]: " + client_conn.ip + " [ID]: " + client_conn.name + " [PORT]: " + std::to_string(client_conn.port));
                    open_client_socket(server, client_conn.ip, client_conn.port, client_conn.name, lock);
                }

                if (server.client_names.size() == server.max_clients) {
                    break;
                }
            }
        }
        if (client_conn.size() > 9) {
            block_group(client->name);
            server.delete_client(client->fd, lock);
            return;
        }


        client->update_connections(client_conn);
    }

    if (command.rfind("GETMSGS,", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");
        if (parameters.size() != 2) { // Parameters need to be 2
            server_log_message(client->name, "ERROR","No Group ID Passed to GETMSGS Aporting Operation...", "message_handler");
            return;
        }
        get_msgs(client_fd, server, parameters[1], lock);
        if(lock.owns_lock()){
            lock.unlock(); // NOTE: get_msgs does not unlock, just returns so we give back the lock here
        }
    }
    if (command.rfind("SENDMSG", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");
        if (parameters.size() <= 3) { // Parameters need to be 4
            server_log_message(client->name, "ERROR","Wrong Number Of Parameters Passed To SENDMSG Aborting OPERATION...", "message_handler");
            return;
        }

        send_msg(server, parameters[1], parameters[2], command, lock);
    }
    if (command.rfind("HELO", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");

        if (parameters.size() < 2 || parameters[1].empty()) {
            server_log_message(client->name, "ERROR","Wrong Number Of Parameters Passed To HELO Aborting OPERATION...", "message_handler");
            return;
        }
        recv_helo(client_fd, server, parameters[1], lock); // So it does not store HELO as name - MIGHT HAVE TO CHANGE
    }
    if (command.rfind("KEEPALIVE", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");

        if (parameters.size() == 2 && !parameters[1].empty() && std::all_of(parameters[1].begin(), parameters[1].end(), ::isdigit)) {

            uint16_t num_messages = static_cast<uint16_t>(std::stoi(parameters[1]));
            recieve_keepalive(*client, num_messages);

            server_log_message(client->name, "INFO", "Received KEEPALIVE from " + client->name + " with " + std::to_string(num_messages) + " messages");
        } else {
            server_log_message(client->name, "ERROR", "Malformed KEEPALIVE from " + client->name + ": " + command, "message_handler");
        }
    }

    if(command.rfind("STATUSREQ", 0) == 0){
        resp_status(server, client->fd, lock);
    }
}

void handle_client_read(int64_t client_fd, Server &serv, std::unique_lock<std::mutex>& lock) {
    if (client_fd < 0 || !serv.get_server_clients(lock).contains(client_fd)) {
        return; // invalid file descriptor or socket was closed elsewhere
    }
    // Add framing to message and send
    char buffer[6000];
    std::shared_ptr<Client> client = serv.get_server_client(client_fd, lock);

    if(!client){
        server_log_message("ERROR", "NULL", "Fetched Null(Deleted) Client During Read");
        return;
    }

    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(client->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        server_log_message(client->name, "ERROR","getsockopt threw error for " + client->name, "handle_client_read");
        server_log_message(client->name, "ERROR", "getsockopt threw error for " + client->name, "handle_client_read");
        return;
    }

    int bytes_read = recv(client->fd, buffer, sizeof(buffer), 0);
    //log_message("PROCESSING", "Received " + std::to_string(bytes_read) + " Bytes");
    if (bytes_read < 0) {
        if (errno != EAGAIN) {
            server_log_message(client->name, "ERROR", "recv() failed for " + client->name + ": " + strerror(errno), "handle_client_read");
        }
        return;
    } else if (bytes_read == 0) {
        return;
    }

    // Append new data into the client's input buffer
    client->inbuf.append(buffer, bytes_read);

    std::vector<std::string> messages = extract_frames(client->inbuf);

    // calculate how much of inbuf we consumed
    size_t consumed_bytes = 0;
    for (const auto &msg : messages) {
        consumed_bytes += msg.size();
    }

    std::vector<std::string> commands = extract_payload(messages);

    // Only erase what was fully parsed
    client->inbuf.erase(0, consumed_bytes);

    server_log_message(client->name, "INFO", "Received " + std::to_string(commands.size()) + " Commands");
    // Call message handler on every message
    for (auto it = commands.begin(); it != commands.end(); ++it) {
        message_handler(client->fd, serv, *it, lock);
    }

    size_t last_etx = client->inbuf.rfind(static_cast<char>(ETX));
    if (last_etx != std::string::npos) {
        client->inbuf.erase(0, last_etx + 1);
    }
}

void handle_client_write(int64_t client_fd, Server &serv, std::unique_lock<std::mutex>& lock) {
    if (client_fd < 0 || !serv.get_server_clients(lock).contains(client_fd)) {
        return; // invalid file descriptor or socket was closed elsewhere
    }
    // Add framing to message and send
    char buffer[5000];
    // && !client.outbound_messages.empty()
    std::shared_ptr<Client> client = serv.get_server_client(client_fd, lock);

    if(!client){
        server_log_message("ERROR", "NULL", "Handle Write Got A Null(deleted) Client");
        return;
    }

    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(client->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        server_log_message(client->name, "ERROR","getsockopt threw error for " + client->name, "handle_client_write");
        server_log_message(client->name, "ERROR", "getsockopt threw error for " + client->name, "handle_client_write");
        return;
    }

    while(true){
        std::string msg = client->front_message();
        if(msg == ""){
            break;
        }
        if (msg != "") {
            if ((send_stream(msg, *client)) != -1) {
                client->pop_message();
                client->remove_first_message();
            };
        }
    }

}

void client_worker(Server& server,int64_t client_fd){
    std::unique_lock<std::mutex> server_lock(server.get_mutex(), std::defer_lock);
    std::shared_ptr<Client> client = server.get_server_client(client_fd, server_lock);
    if(!client){ return; }

    std::string name = client->name;
    server_log_message(name, "INFO", "Client Registered");


    while(true){
        int32_t events = client->wait_for_events();

        if(client->is_closed())
            return;


        if(events == -1){
            server_log_message(name, "INFO", "Worker Exiting");
            return; // Connection was closed while waiting so this means we exit thread
        }

        if(events & EPOLLIN){
            handle_client_read(client_fd, server, server_lock);
        }

        if(events & EPOLLOUT){
            handle_client_write(client_fd, server, server_lock);
        }

        if(server_lock.owns_lock()){
            server_lock.unlock();
        }
    }
}
#endif
