#include <stdio.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <csignal>
#include <client_logs.hpp>
#include <arpa/inet.h>

constexpr int8_t SOH = 0x01; // Start of header
constexpr int8_t STX = 0x02; // End of header
constexpr int8_t ETX = 0x03; // End of message

const std::string GROUP_ID = "A5_9";

#define MAX_BUFFER_SIZE 5000

static void handle_signal(int) {
    if (client_logfile.is_open()) {
        client_logfile.flush();
        client_logfile.close();
    }
    std::_Exit(0); // exit safely
}

std::string frame_message(const std::string &message) {
    // SOH(1) + len(2) + STX(1) + payload + ETX(1)
    constexpr uint16_t OVERHEAD = 1 + 2 + 1 + 1;
    uint16_t total_len_host = static_cast<uint16_t>(message.size() + OVERHEAD);
    uint16_t total_len_net  = htons(total_len_host);

    std::string framed;
    framed.reserve(total_len_host);

    framed.push_back(SOH);
    framed.push_back(static_cast<char>((total_len_net >> 8) & 0xFF)); // hi
    framed.push_back(static_cast<char>( total_len_net & 0xFF)); // lo 
    framed.push_back(STX);
    framed.append(message);
    framed.push_back(ETX);

    return framed;
}

void send_command_to_server(int64_t sock_fd, const std::string &command) {
    // Frame command
    std::string framed_command = frame_message(command);

    // Send command to the server through the open socket
    ssize_t bytes_sent = send(sock_fd, framed_command.c_str(), framed_command.length(), 0);

    // Check if sending the command to server failed
    if (bytes_sent < 0) {
        client_log_message("ERROR", "Error sending the command to the server");
        exit(1);
    }

    std::cout << "\n";
    client_log_message("SENT", command);
}

std::string unframe_message(const std::string &message) {
    
    auto stx = message.find(static_cast<char>(STX));
    auto etx = message.rfind(static_cast<char>(ETX));

    // If the message does not need unframing
    if (stx == std::string::npos || etx == std::string::npos || stx + 1 > etx) {
        return message;
    }

    return message.substr(stx + 1, etx - stx - 1);
}

std::string receive_respons_from_server(int64_t sock_fd) {
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE); // Clear buffer

    // Set response timeout
    struct timeval tv;
    tv.tv_sec = 10;  // 10 second timeout
    tv.tv_usec = 0; // Make sure there are no extra microseconds

    // Check if respond timeout fails, when setting it
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
        client_log_message("ERROR", "Error setting response timeout");
        exit(1);
    }

    ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer), 0);

    if (bytes_received == 0) {
        client_log_message("CLOSE", "Connection to server has closed\n");
        exit(0); // Since the connection has been closed
    } else if (bytes_received < 0) {
        client_log_message("TIMEOUT", "Response from server timed out");
        return ""; // So it goes back to the loop
    }

    // Make sure to have a complete frame and if so unframe it
    static std::string buffer_accum;
    buffer_accum.append(buffer, bytes_received);

    size_t soh = buffer_accum.find(SOH);
    size_t etx = buffer_accum.find(ETX, soh);
    if (soh != std::string::npos && etx != std::string::npos && etx > soh) {
        std::string frame = buffer_accum.substr(soh, etx - soh + 1);
        buffer_accum.erase(0, etx + 1);
        std::string message = unframe_message(frame); // Unframe the message received from the server
    }
    return ""; // No complete frame yet
}

void get_response(int64_t sock_fd) {
    // Receive from server
    std::string response = receive_respons_from_server(sock_fd);

    // Check if the response is empty, and output the response if not
    if (!response.empty()) {
        client_log_message("RESPONSE", response);
    }
}

void get_msg(int64_t sock_fd) {
    std::string command ="GETMSG,A5_9"; // Might have to change so it does not include group id

    // Send the command to the server (server command: GETMSG,<GROUP_ID>)
    send_command_to_server(sock_fd, command);

    // Get the response back from the server
    get_response(sock_fd);
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

    // Send the command to the server (server command:
    // SENDMSG,<TP_GROUP_ID>,<FROM_GROUP_ID>,<message contents>)
    std::string command = "SENDMSG," + group_id + "," + GROUP_ID + "," + message;
    
    send_command_to_server(sock_fd, command);
}

void list_servers(int64_t sock_fd) {
    // To get the list of servers we need to send a HELO command to the server
    std::string command = "HELO," + GROUP_ID;
    send_command_to_server(sock_fd, command);

    // Get the response back from the server
    get_response(sock_fd);
}

void req_status(int64_t sock_fd) {
    // Send the command to the server
    std::string command = "STATUSREQ";
    send_command_to_server(sock_fd, command);

    // Get the response back from the server
    get_response(sock_fd);
}

int64_t set_up_connection(const std::string &ip_string, int port) { // Might have to change ip parameter
    // Create the socket
    int64_t sock_fd;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        client_log_message("ERROR", "Socket creation failed");
        exit(0);
    }

    // Setup socket address structure for connection
    struct sockaddr_in server_addr;
    memset(&server_addr, 0,
           sizeof(server_addr)); // Initialize server addr. struct.

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Try to set socket address
    if (inet_pton(AF_INET, ip_string.c_str(), &server_addr.sin_addr) != 1) {
        client_log_message("INVALID", "Invalid ip address");
        exit(0);
    }

    // Connect to server
    if (connect(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        client_log_message("ERROR", "Connection failed");
        exit(0);
    }

    client_log_message("CONNECTED", "Connected to server successfully");
    return sock_fd;
}

int main(int argc, char *argv[]) {
    // Check for right number of arguments - Gives error if more or less than 3
    // arguments Arguments: ./client 130.208.246.98 4009 - execute program,
    // server ip and port nr.
    if (argc != 3) {
        client_log_message("INVALID", "Wrong amount of arguments. Should be: ./client <server_ip> <port_nr>");
        return 1;
    }

    // Read port from command line arguments and convert to an int
    int port = atoi(argv[2]);

    // Read server ip from command line arguments
    std::string ip_string = argv[1];

    // Connect to the server
    int64_t sock_fd = set_up_connection(ip_string, port);

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    list_servers(sock_fd);

    new_file("CLIENT");


    // After connect enter a loop to keep connection open and handle commands until user wants to exit
    while (true && sock_fd != -1) {
        // We available commands
        std::cout << "\nAvailable commands:\n" 
        << "(1) GETMSG (Get a message from the server for your group)\n" 
        << "(2) SENDMSG (Let server deliver a message to a group)\n" 
        << "(3) LISTSERVERS (List servers your server is connected to)\n"
        << "(4) STATUSREQ (Request the status of the server)\n"
        << "Press enter if you want to exit the program\n"
        << "Enter the number of your choice of a command: "; 

        // Read command from keyboard input
        std::string command;
        getline(std::cin, command);

        std::cout << "\n";

        // Handle the command of the user's input
        if (command == "") {
            client_log_message("CLOSE", "Connection to server has closed\n");
            break;
        } else if (command == "1") {
            get_msg(sock_fd);
        } else if (command == "2") {
            send_msg(sock_fd);
        } else if (command == "3") {
            list_servers(sock_fd);
        } else if (command == "4") {
            req_status(sock_fd);
        } else {
            client_log_message("INVALID", "Invalid command...");
        }
    }

    // Try to close the socket connection after client quit
    client_log_message("INFO", "Closing socket...");
    if (close(sock_fd) < 0) {
        client_log_message("ERROR", "Close failed");
        exit(0);
    }
    client_log_message("CLOSE", "Connection to server has been closed");

    return 0;
}
