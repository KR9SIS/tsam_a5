#pragma once

inline constexpr const char* CLIENT_LOGFILE = "logs/client_logs.txt";

#define MAX_LOG_FILE_SIZE (1024 * 1024 * 5)

#include <string>
#include <mutex>
#include <fstream>

extern std::mutex log_mutex;
extern std::ofstream client_logfile;

void log_message(const std::string &type, const std::string &message);

void new_file();
