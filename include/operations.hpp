#pragma once
#include "serv_client.hpp"
#include <map>

class Server;// WARNING: FORWARD DECAILRATION, DO NOT REMOVE THIS!!!!!

extern int16_t send_helo(Client &client);
extern int16_t recv_helo(Client &client, Server &server, const std::string &name);
extern int16_t send_keepalive(std::map<int64_t, Client> &clients);
extern int16_t recieve_keepalive(Client &client, uint16_t num_messages);
extern int16_t get_msg(Client &client, Server &server);
extern int16_t send_msg(Server &server, int16_t to_group, int16_t from_group, const std::string &message);
extern int16_t req_status();
extern int16_t resp_status(std::map<int64_t, Client> &clients);
extern void message_handler(Client &client, Server &server, const std::string &command);
extern void handle_client_read(Client &client, Server &serv);
extern void handle_client_write(Client &client, Server &serv);
