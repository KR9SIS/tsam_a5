#include <client_logs.hpp>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <cstring>
#include <mutex>

std::mutex client_log_mutex;
std::mutex server_log_mutex;
std::ofstream client_logfile(CLIENT_LOGFILE, std::ios::out | std::ios::app);
std::ofstream server_logfile(SERVER_LOGFILE, std::ios::out | std::ios::app);

// Static initializer to make the ofstream auto-flush after each write
static struct InitLog {
    InitLog() {
        client_logfile.setf(std::ios::unitbuf);
        server_logfile.setf(std::ios::unitbuf);
    }
} _initLog;

// Type: is it response received or command sent
void client_log_message(const std::string &type, const std::string &message) {
    std::lock_guard<std::mutex> lock(client_log_mutex);

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
}

void server_log_message(const std::string &client, const std::string &type, const std::string &message, const std::string &func) {
    std::lock_guard<std::mutex> lock(server_log_mutex);

    if (!server_logfile.is_open()) {
        server_logfile.open(SERVER_LOGFILE, std::ios::out | std::ios::app);
        server_logfile.setf(std::ios::unitbuf); // reapply auto-flush after reopen
    }
    
    time_t now = time(0);
    char* date_time = ctime(&now);
    date_time[strlen(date_time) - 1] = '\0'; // So it does not add a newline

    if (type == "ERROR") {
    std::cout << "[" << date_time << "] [" << client << "] [" << type << "] [" << func << "] " << message << std::endl;

    server_logfile << "[" << date_time << "] [" << client << "] [" << type << "] [" << func << "] " << message << "\n";
    } else {
    std::cout << "[" << date_time << "] [" << client << "] [" << type << "] " << message << std::endl;

    server_logfile << "[" << date_time << "] [" << client << "] [" << type << "] " << message << "\n";
    }

    server_logfile.flush();
}

// TODO: Change this function so It makes a new file each day
void new_file(const std::string &is_server_or_client) {
    if (is_server_or_client == "CLIENT") {
        client_logfile.close();
        std::rename(CLIENT_LOGFILE, "../logs/client_log_old.txt");
    
        client_logfile.open(CLIENT_LOGFILE, std::ios::out | std::ios::app);
        client_logfile.setf(std::ios::unitbuf);
    } else {
        server_logfile.close();

        std::rename(SERVER_LOGFILE, "A5_9_server_logs_old.txt");
    
        server_logfile.open(SERVER_LOGFILE, std::ios::out | std::ios::app);
        server_logfile.setf(std::ios::unitbuf);
    }
}
