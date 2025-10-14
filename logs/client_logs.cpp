#include <client_logs.hpp>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <cstring>
#include <mutex>

std::mutex log_mutex;
std::ofstream client_logfile(CLIENT_LOGFILE, std::ios::out | std::ios::app);
// I like this more than Friðrik's suggestion (I think it is simpler)

// Static initializer to make the ofstream auto-flush after each write
static struct InitLog {
    InitLog() {
        client_logfile.setf(std::ios::unitbuf);
    }
} _initLog;

// Type: is it response received or command sent
void log_message(const std::string &type, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex);

    if (!client_logfile.is_open()) {
        client_logfile.open(CLIENT_LOGFILE, std::ios::out | std::ios::app);
        client_logfile.setf(std::ios::unitbuf); // reapply auto-flush after reopen
    }
    
    time_t now = time(0);
    char* date_time = ctime(&now);
    date_time[strlen(date_time) - 1] = '\0'; // So it does not add a newline

    std::cout << "[" << date_time << "] [" << type << "] " << message << std::endl;

    client_logfile << "[" << date_time << "] [" << type << "] " << message << "\n";

    client_logfile.flush();

    new_file(); // Will only make a new file if needed
}

// TODO: Change this function so It makes a new file each day
void new_file() {
    struct stat logfile_info{};
    if (stat(CLIENT_LOGFILE, &logfile_info) == 0 &&
        static_cast<std::size_t>(logfile_info.st_size) >= MAX_LOG_FILE_SIZE) {

        client_logfile.close();
        // Keep rotated file inside logs/ as well
        std::rename(CLIENT_LOGFILE, "logs/client_log_old.txt");

        client_logfile.open(CLIENT_LOGFILE, std::ios::out | std::ios::app);
        client_logfile.setf(std::ios::unitbuf);
    }
}
