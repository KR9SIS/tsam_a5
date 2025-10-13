#include <client_logs.hpp>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <cstring>
#include <mutex>

std::mutex log_mutex;
std::ofstream client_logfile; // Ofstream object and opens the file

// I like this more than Friðrik's suggestion (I think it is simpler)

// Type: is it response received or command sent
void log_message(const std::string &type, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    time_t now = time(0);
    char* date_time = ctime(&now);
    date_time[strlen(date_time) - 1] = '\0'; // So it does not add a newline



    std::cout << "[" << date_time << "] [" << type << "]" << message << std::endl;

    // Check if the file is open to write to it
    client_logfile << "[" << date_time << "] [" << type << "]" << message << "\n";
    client_logfile.flush();

    new_file(); // Will only make a new file if needed
}

// TODO: Change this function so It makes a new file each day
void new_file() {
    struct stat logfile_info;

    if (stat(CLIENT_LOGFILE, &logfile_info) == 0 && logfile_info.st_size >= MAX_LOG_FILE_SIZE) {
        client_logfile.close();
        rename("client_log.txt", "client_log_old.txt"); // TODO: Change the renaming
        client_logfile.open(CLIENT_LOGFILE, std::ios::out | std::ios::app);
    }
}