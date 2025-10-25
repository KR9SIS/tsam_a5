#pragma once
#include "serv_client.hpp"
#include <cstdint>
#include <mutex>

class Server; // WARNING: FORWARD DECAILRATION, DO NOT REMOVE THIS!!!!!

extern void send_helo(Client& client_fd);
extern int16_t recv_helo(int64_t client_fd, Server &server, const std::string &name, std::unique_lock<std::mutex>& lock);
//extern int16_t send_keepalive(std::map<int64_t, Client> &clients);
extern int16_t send_keepalive_helper(Client& client);
extern int16_t recieve_keepalive(Client& client, uint16_t num_messages);
extern int16_t get_msg(int64_t client_fd, Server &server);
extern int16_t send_msg(Server &server, std::string to_group, std::string from_group, const std::string &message, std::unique_lock<std::mutex>& lock);
extern int16_t req_status(Client& client_fd);
extern int16_t resp_status(Server& server, int64_t client_fd, std::unique_lock<std::mutex>& lock);
extern void message_handler(int64_t client_fd, Server &server, const std::string &command, std::unique_lock<std::mutex>& lock);
extern void handle_client_read(int64_t client_fd, Server &serv, std::unique_lock<std::mutex>& lock);
extern void handle_client_write(int64_t client_fd, Server &serv, std::unique_lock<std::mutex>& lock);
extern void send_helos(Server &server);
extern void send_statusreqs(Server &server);
extern int64_t open_client_socket(Server &server, std::string ip, int16_t port, std::string name, std::unique_lock<std::mutex>& lock);
extern void client_worker(Server&, int64_t client_fd);
extern void keep_alive_loop(Server&, std::unique_lock<std::mutex>&);
