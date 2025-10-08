// Client from assignment 1

#include <stdio.h>
#include <iostream>

using namespace std;

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// TODO: Implement these functions

int get_msg(int group_id) { return 0; }

int send_msg(int to_group, int from_group) { return 0; }

int list_servers() { return 0; }


int main(int argc, char *argv[]) {
 
    int sock;

    // Check for right number of arguments - Gives error if less than 3 arguments
    // Arguments: ./client 127.0.0.1 5000 - execute program, server ip and port nr.
    if (argc < 3) {
        perror("Wrong amount of arguments");
        return 1;
    }
    
    // Read port from command line arguments and convert to an int
    int port = atoi(argv[2]);

    // Read server ip from command line arguments
    char *ip_string = argv[1];

    // Client: socket() - From slides might have to change
    // Setup socket address structure for connection
    struct sockaddr_in server_addr;
    struct hostent *server;

    // Might need to make it exactly like the server - Got this from lecture code
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(0);
    }

    // Setup socket address structure for connection - Rest of it
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Try to set socket address - Need to know the IP address of the server
    // Server ip: 127.0.0.1
    if (inet_pton(AF_INET, ip_string, &server_addr.sin_addr) != 1) {
        cout << "ip address is weird" << endl;
        exit(0);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(0);
    }

    // After connect enter a loop to keep connection open
    while (true) {

        // Read command from keyboard input
        string command;
        getline(cin, command);

        // Break loop if exit typed in
        if (command == "exit") {
            break;
        }
    
        // Make the command into a SYS command - by adding SYS in front of the command
        string sys_command = "SYS " + command;
    
        // Send the command to the server
        int n = send(sock, sys_command.c_str(), sys_command.length(), 0);
        if (n < sys_command.length()) {
            cout << "Did not send everything" << endl;
        }
    
        // Server reply - Taken from slides
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer)); // Clear buffer

        while(true) {
            recv(sock, buffer, sizeof(buffer), 0);
            string output = string(buffer);

            size_t pos_of_identifier = output.find("done");

            // If the output is not no position, so we did find the str "done" it quits the loop
            if(pos_of_identifier != string::npos) {
                string substring = output.substr(0, pos_of_identifier);
                cout << substring;
                break;
            }

            cout << buffer;
        }
    }

    if (close(sock) < 0) {
        perror("Close failed");
        exit(0);
    }
    
    return 0;
}