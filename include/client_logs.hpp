#pragma once

inline constexpr const char* CLIENT_LOGFILE = "../logs/client_logs.txt";
inline constexpr const char* SERVER_LOGFILE = "A5_9_server_logs.txt";

#include <string>
#include <mutex>
#include <fstream>

extern std::mutex client_log_mutex;
extern std::mutex server_log_mutex;

extern std::ofstream client_logfile;
extern std::ofstream server_logfile;

void client_log_message(const std::string &type, const std::string &message);
void server_log_message(const std::string &type, const std::string &message);

void new_file(const std::string &is_server_or_client);
