#ifndef SERVEROPERATIONS
#define SERVEROPERATIONS

#include "operations.hpp"
#include "block_bots.hpp"
#include "client_logs.hpp"
#include "helpers.h"
#include "server.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
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
    std::unique_lock<std::mutex> server_lock(server.get_mutex(), std::defer_lock);
    server_log_message("EVERYONE", "INFO", "Sending Helos To Everyone");
    while (true) {
        std::map<int64_t, std::shared_ptr<Client>> clients = server.get_server_clients(server_lock);
        int interval_ms = 3000 / std::max(1, (int)clients.size());
        for (auto &[_, client] : clients) {
            if(client->name == "A5_9" || client->name == "A5_09"){ 
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                continue;
            }

            send_helo(*client);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
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
    if(client.helo_pending.load(std::memory_order_acquire)) return;
    client.push_message("HELO,A5_9");
    client.helo_pending.store(true, std::memory_order_release);
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

    return send_stream(message, *client, lock);
}

void keep_alive_loop(Server& server, std::unique_lock<std::mutex>& lock){
    // Implementation so every client/server gets one keepalive per minute but distributed evenly through that minute
    // This is to not overload the server since sending it all at the same time is too much
    static size_t keepalive_index = 0;

    while(true){
        std::map<int64_t, std::shared_ptr<Client>> clients = server.get_server_clients(lock);
        if (!clients.empty()) {
            int interval = std::max(1, static_cast<int>(60000.0 / clients.size())); // To distribute evenly
            auto it = std::next(clients.begin(), keepalive_index % clients.size());
            if (it->second->fd >= 0) {
                send_keepalive_helper(*it->second);
            }
            keepalive_index++;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }
}

// Does the same thing as keepalive just minimises errors and sends at seperate occations not all at same time
int16_t send_keepalive_helper(Client &client) {
    if (client.fd < 0) return -1; // skip closed sockets

    client.append_message(std::string("KEEPALIVE,").append(std::to_string(client.pending_messages)));
    return 0;
}


int16_t recieve_keepalive(Client& client, uint16_t num_messages) {
    client.pending_messages += num_messages;
    if (num_messages > 0) {
        server_log_message(client.name, "INFO", "Storing message from client");
        client.push_message("GETMSGS,A5_09");
    }
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
        client->push_message(server.get_message_for_cnc(lock));
        client->pending_messages--;
        return 0;

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
        client->push_message(messages);
        return 0;
    }
}

std::vector<std::string> parse_send_msg(const std::string& message){
    std::vector<std::string> sections;
    size_t group_to_pos = strlen("SENDMSG,");
    size_t group_from_pos = message.find(",",group_to_pos) + 1;
    size_t message_start = message.find(",", group_from_pos) + 1;
    size_t eot_pos = message.rfind("\x04");
    size_t end_of_packet = strlen(message.c_str()) - 1;

    sections.push_back(message.substr(group_to_pos, (group_from_pos - group_to_pos) - 1));
    sections.push_back(message.substr(group_from_pos, (message_start - group_from_pos) - 1));
    if(eot_pos != std::string::npos){
        sections.push_back(message.substr(message_start, eot_pos - message_start));
        sections.push_back(message.substr(eot_pos + 1));
    }
    else{
        sections.push_back(message.substr(message_start));
    }
    return sections;
}

int16_t send_msg(Server &server, std::string to_group, std::string from_group, const std::string &message,std::unique_lock<std::mutex>& lock) {
    // <STH><Length><STX>SENDMSG,<group_to>,<group_from>,<message><EOT><HOPS><ETX>
    std::vector<std::string> message_tokens = parse_send_msg(message);

    if (to_group == "A5_9" || to_group == "A5_09") {
        std::string message_with_group = "[" + message_tokens[2] + "] " + message_tokens[2];
        server.store_message_for_cnc(message_tokens[2], lock);
    }

    std::string message_with_group = "[FROM: " + message_tokens[2] + "] " + message_tokens[2];
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

    std::string msg_copy("SENDMSG,");
    msg_copy.append(to_group).append(",").append(from_group).append(",");
    msg_copy.append(message_tokens[2]);
    msg_copy.append("\x04");

    if (message_tokens.size() < 4) {
        msg_copy.append("A5_9");
        clients.begin()->second->push_message(msg_copy);
    } else {
        std::string recipients = message_tokens[3];
        msg_copy.append(recipients);
        for (auto &[_, client] : clients) {
            if (recipients.find(client->name) == std::string::npos) {
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

    return send_stream(resp, *client, lock);
}

void message_handler(int64_t client_fd, Server &server, const std::string &command, std::unique_lock<std::mutex>& lock) {
    std::shared_ptr<Client> client = server.get_server_client(client_fd, lock);
    if(!client){
        server_log_message("NULL", "ERROR", "message_handler Fetched A Null(Deleted) Client");
        return;
    }
    server_log_message(client->name, "RESPONSE", client->name + " sent " + command);

    std::string parsed_command = command.substr(0, command.find(","));
    server_log_message("SYSTEM", "DEBUG", "Parsed Command Processed " + parsed_command);

    if (parsed_command.rfind("SERVERS", 0) == 0) {
        std::string rest = command.substr(strlen("SERVERS,"));

        std::vector<std::string> client_conn = extract_parameters(rest, ";");
        std::vector<Client::conn> candidate_connections;

        std::unordered_set<std::string> seen_connections;// Testing for duplicates in SERVERS response
        for(const auto& conn : client_conn){
            if (conn.empty()) {
                continue;
            }

            std::vector<std::string> parts = extract_parameters(conn,",");

            if (parts.empty())
                continue;

            if(seen_connections.count(parts[0])){
                //Found duplicate in SERVERS command, bot is not behaving well block
                block_group(client->name);
                server.delete_client(client->fd, lock);
                return;
            }
            seen_connections.insert(parts[0]);
        }

        client->update_connections(client_conn);

        if (server.get_server_clients(lock).size() < server.max_clients) {
            for (auto &[_, client_conn] : client->connections) {
                if(is_blocked(client_conn.name))
                    continue;

                if(client_conn.name.find("A5") == std::string::npos)
                    continue;

                if (server.client_names.find(client_conn.name) == server.client_names.end())
                    candidate_connections.push_back(client_conn);

                if (server.client_names.size() + candidate_connections.size() >= server.max_clients)
                    break;
            }

            for(auto& new_client : candidate_connections){
                server_log_message("SYSTEM", "INFO", "Connecting To Client [IP]: " + new_client.ip + " [ID]: " + new_client.name + " [PORT]: " + std::to_string(new_client.port));
                open_client_socket(server, new_client.ip, new_client.port, new_client.name, lock);
            }

        }
    }

    if (parsed_command.rfind("GETMSGS,", 0) == 0) {
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
    if (parsed_command.rfind("SENDMSG", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");
        if (parameters.size() <= 3) { // Parameters need to be 4
            server_log_message(client->name, "ERROR","Wrong Number Of Parameters Passed To SENDMSG Aborting OPERATION...", "message_handler");
            return;
        }

        send_msg(server, parameters[1], parameters[2], command, lock);
    }
    if (parsed_command.rfind("HELO", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");

        if (parameters.size() < 2 || parameters[1].empty()) {
            server_log_message(client->name, "ERROR","Wrong Number Of Parameters Passed To HELO Aborting OPERATION...", "message_handler");
            return;
        }
        recv_helo(client_fd, server, parameters[1], lock); // So it does not store HELO as name - MIGHT HAVE TO CHANGE
    }
    if (parsed_command.rfind("KEEPALIVE", 0) == 0) {
        std::vector<std::string> parameters = extract_parameters(command, ",");

        if (parameters.size() == 2 && !parameters[1].empty() && std::all_of(parameters[1].begin(), parameters[1].end(), ::isdigit)) {

            uint16_t num_messages = static_cast<uint16_t>(std::stoi(parameters[1]));
            recieve_keepalive(*client, num_messages);

            server_log_message(client->name, "INFO", "Received KEEPALIVE from " + client->name + " with " + std::to_string(num_messages) + " messages");
        } else {
            server_log_message(client->name, "ERROR", "Malformed KEEPALIVE from " + client->name + ": " + command, "message_handler");
        }
    }

    if(parsed_command.rfind("STATUSREQ", 0) == 0){
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
            if ((send_stream(msg, *client, lock)) != -1) {
                client->pop_message();
                client->remove_first_message();
                std::string command = msg.substr(0, msg.find(","));
                if(command == "HELO"){
                    client->helo_pending.store(false, std::memory_order_release);
                }
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
