#include <logs.hpp>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <cstring>

std::mutex server_log_mutex;
std::mutex client_log_mutex;
std::ofstream server_logfile;
std::ofstream client_logfile; // Ofstream object and opens the file

// I like this more than Friðrik's suggestion (I think it is simpler)

// Type: is it response received or command sent
void client_log_message(std::string type, std::string message) {
    std::lock_guard<std::mutex> lock(client_log_mutex);
    
    time_t now = time(0);
    char* date_time = ctime(&now);
    date_time[strlen(date_time) - 1] = '\0'; // So it does not add a newline



    std::cout << "[" << date_time << "] [" << type << "]" << message << std::endl;

    // Check if the file is open to write to it
    if (client_logfile.is_open()) {
        client_logfile << "[" << date_time << "] [" << type << "]" << message << "\n";
    } else {
        std::cout << "Unable to open file" << std::endl;
    }
}

void new_file() {}