#include <cstdint>
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

#define MAX_BUFFER_SIZE 6000

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

    // Add all frame components and the message to the new string
    framed.push_back(SOH);
    framed.push_back(static_cast<char>( total_len_net & 0xFF));        // low byte first
    framed.push_back(static_cast<char>((total_len_net >> 8) & 0xFF)); // high byte second
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
    // Unframe the message using the length instead of tags, to make sure not to unframe it incorrectly

    if (message.size() < 5 || message[0] != SOH) return message;

    uint16_t net_byte_ord_len; // Length tag comes as 2-bytes in network byte order
    memcpy(&net_byte_ord_len, &message[1], sizeof(uint16_t)); // Copy the 2 bytes into net_byte_ord_len
    uint16_t len_tag = ntohs(net_byte_ord_len); // Convert net. byte order to host byte order (make sure it's right endian)

    if (len_tag < 5 || len_tag > message.size()) {
        uint16_t swapped = (net_byte_ord_len >> 8) | (net_byte_ord_len << 5);
        uint16_t len_tag_swapped = ntohs(swapped);

        if (len_tag_swapped >= 5 && len_tag_swapped <= message.size()) {
            len_tag = len_tag_swapped;
        } else {
            return message; // Message is invalid
        }
    }

    // Extract the message
    size_t message_len = len_tag - 5; // Length of the payload (without the tags)
    size_t message_start = 4; // <SOH><len><STX> = 4 bytes - Payload starts after that

    if (message.size() < message_start + message_len) return message; // Message is incomplete

    return message.substr(message_start, message_len);
}

std::vector<std::string> receive_respons_from_server(int64_t sock_fd) {
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE); // Clear buffer

    // Set response timeout
    struct timeval tv;
    tv.tv_sec = 30;  // 30 second timeout
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
        return {}; // So it goes back to the loop
    }

    // Make sure to have a complete frame and if so unframe it
    static std::string buffer_accum;
    buffer_accum.append(buffer, bytes_received);

    std::vector<std::string> messages;

    while (true) {
        size_t soh = buffer_accum.find(SOH);
        if (soh == std::string::npos) break;

        if (buffer_accum.size() < soh + 5) break; // Not enough data yeat for full header

        uint16_t net_len;
        memcpy(&net_len, &buffer_accum[soh + 1], sizeof(uint16_t));
        uint16_t total_len = ntohs(net_len);

        if (buffer_accum.size() < soh + total_len) break; // Not a full frame

        std::string frame = buffer_accum.substr(soh, total_len);
        buffer_accum.erase(0, soh + total_len);

        std::string message = unframe_message(frame);
        messages.push_back(message);
    }

    return messages;
}

void get_responses(int64_t sock_fd) {
    std::vector<std::string> responses = receive_respons_from_server(sock_fd);
    for (const auto &response : responses) {
        if (!response.empty()) {
            client_log_message("RESPONSE", response);
        }
    }
}

void get_msg(int64_t sock_fd) {
    // Gets messages the server is storing for the client
    std::string command ="GETMSGS," + GROUP_ID;

    // Send the command to the server (server command: GETMSGS,<GROUP_ID>)
    send_command_to_server(sock_fd, command);
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
    // SENDMSG,<TO_GROUP_ID>,<FROM_GROUP_ID>,<message contents>)
    std::string command = "SENDMSG," + group_id + "," + GROUP_ID + "," + message;

    send_command_to_server(sock_fd, command);
}

void list_servers(int64_t sock_fd) {
    // To get the list of servers we need to send a HELO command to the server
    std::string command = "HELO," + GROUP_ID;
    send_command_to_server(sock_fd, command);
}

void req_status(int64_t sock_fd) {
    // Get the status of the server using the STATUSREQ command
    std::string command = "STATUSREQ";
    send_command_to_server(sock_fd, command);
}

int64_t set_up_connection(const std::string &ip_string, int port) {
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

void recv_responses(int64_t sock_fd){
    while(true){
        get_responses(sock_fd);
        sleep(60);
    }
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
        // Print to the terminal available commands
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

        get_responses(sock_fd);
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
