#ifndef LOGGING_CLASS
#define LOGGING_CLASS

#define CLIENT_LOGFILE "client_log.txt"
#define MAX_LOG_FILE_SIZE (1024 * 1024 * 5)

#include <string>
#include <mutex>
#include <fstream>

extern std::mutex logMutex;
extern std::ofstream client_logfile;

void log_message(const std::string &type, const std::string &message);

void new_file();

#endif