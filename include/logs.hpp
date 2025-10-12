#ifndef LOGGING_CLASS
#define LOGGING_CLASS
#define CLIENT_LOGFILE "client_log.txt"

#include <string>
#include <mutex>
#include <fstream>

extern std::mutex logMutex;
extern std::ofstream server_logfile;
extern std::ofstream client_logfile;

void client_log_message(std::string type, std::string message);

void new_file();

#endif