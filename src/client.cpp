// Client from assignment 1
#include <client_logs.hpp>
#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>

constexpr int8_t SOH = 0x01; // Start of header
constexpr int8_t STX = 0x02; // End of header
constexpr int8_t ETX = 0x03; //End of message

const std::string GROUP_ID = "A5_9";

#define MAX_BUFFER_SIZE 5000

void get_msg(int64_t sock_fd) {
    std::string command = "GETMSG," + GROUP_ID; // Might have to change so it does not include group id

    // Send the command to the server (server command: GETMSG,<GROUP_ID>)
    send_command_to_server(sock_fd, command);

    log_message("SENT", command);

    // Get the response back from the server
    std::string response = receive_respons_from_server(sock_fd);

    // Check if the response is empty, and output the response if not
    if (!response.empty()) {
        std::cout << "Server response:" << std::endl;
        std::cout << response << std::endl;

        log_message("RESPONSE", response);
    }
}

void send_msg(int64_t sock_fd) {
    std::string group_id;
    std::string message;

    // Get the group ID of the group to deliver the message to
    std::cout << "Enter the group ID to whom to deliver the message to: ";
    std::getline(std::cin, group_id);

    // Get the message to be delivered
    std::cout << "Enter the message you want to deliver: ";
    std::getline(std::cin, message);

    // Send the command to the server (server command: SENDMSG,<TP_GROUP_ID>,<FROM_GROUP_ID>,<message contents>)
    std::string command = "SENDMSG," + group_id + "," + GROUP_ID + "," + message;
    send_command_to_server(sock_fd, command);

    log_message("SENT", command);

    // Get the response back from the server
    std::string response = receive_respons_from_server(sock_fd);

    // Check if the response is empty, and output the response if not
    if (!response.empty()) {
        std::cout << "Server response:" << std::endl;
        std::cout << response << std::endl;

        log_message("RESPONSE", response);
    }
}

void list_servers(int64_t sock_fd) {
    // To get the list of servers we need to send a HELO command to the server
    std::string command = "HELO," + GROUP_ID;
    send_command_to_server(sock_fd, command);

    log_message("SENT", command);

    // Get the response back from the server
    std::string response = receive_respons_from_server(sock_fd);

    // Check if the response is empty, and output the response if not
    if (!response.empty()) {
        std::cout << "Server response:" << std::endl;
        std::cout << response << std::endl;

        log_message("RESPONSE", response);
    }
}

std::string frame_message(const std::string &message) {
    uint8_t frame_size = sizeof(SOH) + sizeof(uint16_t) + sizeof(STX) + sizeof(ETX);

    uint16_t total_packet_size = message.length() + frame_size;
    total_packet_size = htons(total_packet_size);

    // Create data
    std::string framed_message;
    framed_message.push_back(SOH);
    framed_message.push_back(total_packet_size >> 0x08);
    framed_message.push_back(total_packet_size & 0x08);
    framed_message.push_back(STX);
    for (const char& c : message) {
        framed_message.push_back(c);
    }
    framed_message.push_back(ETX);

    return framed_message;
}

std::string unframe_message(const std::string &message) {
    
    ssize_t start_of_message_pos = message.find(STX, 0);

    if (message[0] == SOH && message[message.size() - 6] == ETX) { // Make sure headers are there before removing
        return message.substr(start_of_message_pos + 1, message.size() - 6);
    }
    return message; // If headers are not there just return the message back
}

void send_command_to_server(int64_t sock_fd, const std::string &command) {
    // Frame command
    std::string framed_command = frame_message(command);

    // Send command to the server through the open socket
    ssize_t bytes_sent = send(sock_fd, framed_command.c_str(), framed_command.length(), 0);

    // Check if sending the command to server failed
    if (bytes_sent < 0) {
        perror("Error sending the command to the server");
        exit(1);
    }

    log_message("SENT", command);
}

std::string receive_respons_from_server(int64_t sock_fd) {
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE); // Clear buffer

    // Set response timeout
    struct timeval tv;
    tv.tv_sec = 5; // 5 second timeout
    tv.tv_usec = 0; // Make sure there are no extra microseconds

    // Check if respond timeout fails, when setting it
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
        perror("Error setting response timeout");
        exit(1);
    }

    int bytes_received = recv(sock_fd, buffer, MAX_BUFFER_SIZE, 0);

    if (bytes_received == 0) {
        std::cout << "Connection to server has closed" << std::endl;
        exit(0); // Since the connection has been closed
    } else if (bytes_received < 0) {
        std::cout << "Response from server timed out" << std::endl;
        return ""; // So it goes back to the loop
    }

    // Unframe the message received from the server
    return unframe_message(std::string(buffer, bytes_received)); // Cast the char buffer to a string
}

int64_t set_up_connection(const std::string &ip_string, int port) { // Might have to change ip parameter
    // Create the socket
    int64_t sock_fd;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(0);
    }

    // Setup socket address structure for connection
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // Initialize server addr. struct.

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Try to set socket address
    if (inet_pton(AF_INET, ip_string.c_str(), &server_addr.sin_addr) != 1) {
        std::cout << "Invalid ip address" << std::endl;
        exit(0);
    }

    // Connect to server
    if (connect(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(0);
    }

    std::cout << "Connected to server successfully" << std::endl;
    return sock_fd;
}


int main(int argc, char *argv[]) {
    // Check for right number of arguments - Gives error if more or less than 3 arguments
    // Arguments: ./client 130.208.246.98 4009 - execute program, server ip and port nr.
    if (argc != 3) {
        perror("Wrong amount of arguments. Should be: ./client <server_ip> <port_nr>");
        return 1;
    }
    
    // Read port from command line arguments and convert to an int
    int port = atoi(argv[2]);

    // Read server ip from command line arguments
    std::string ip_string = argv[1];

    // Connect to the server
    int64_t sock_fd = set_up_connection(ip_string, port);

    // After connect enter a loop to keep connection open and handle commands until user wants to exit
    while (true) {
        // We available commands
        std::cout << "Available commands:\n" 
        << "(1) GETMSG (Get a message from the server for your group)\n" 
        << "(2) SENDMSG (Let server deliver a message to a group)\n" 
        << "(3) LISTSERVERS (List servers your server is connected to)\n" 
        << "Press enter if you want to exit the program\n"
        << "Enter the number of your choice of a command "; 

        // Read command from keyboard input
        std::string command;
        getline(std::cin, command);

        // Handle the command of the user's input
        if (command == "") {
            break;
        } else if (command == "1") {
            get_msg(sock_fd);
        } else if (command == "2") {
            send_msg(sock_fd);
        } else if (command == "3") {
            list_servers(sock_fd);
        } else {
            std::cout << "Invalid command..." << std::endl;
        }
    }

    // Try to close the socket connection after client quit
    if (close(sock_fd) < 0) {
        perror("Close failed");
        exit(0);
    }
    
    return 0;
}
