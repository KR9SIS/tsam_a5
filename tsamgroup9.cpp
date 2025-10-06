#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define SOH 0x01
#define STX 0x02
#define ETX 0x03

constexpr u_int8_t BACKLOG = 5; // Max length of queue of waiting connections
constexpr u_int8_t MAX_CLIENTS = 8; // Maximum number of clients
constexpr u_int8_t MAX_EVENTS = 10; // Maximum number of events

void write_error(std::string msg, FILE *fd = stderr) {
  std::error_code ec(errno, std::system_category());
  fprintf(fd, "MESSAGE: %s\n ERROR CODE: %d\n REASON: %s", msg.c_str(),
          ec.value(), ec.message().c_str());
}

int16_t send_stream(std::string stream_str, int64_t client_fd) {
  constexpr int16_t MAX_STREAM_LEN = 5000;

  if (stream_str.length() > MAX_STREAM_LEN - 3) {
    stream_str.resize(MAX_STREAM_LEN - 3);
  }

  // Create data
  std::vector<char> stream;
  stream.push_back(SOH);
  stream.push_back(stream_str.length());
  stream.push_back(STX);
  for (char c : stream_str) {
    stream.push_back(c);
  }
  stream.push_back(ETX);

  // Convert data to network byte order
  for (int i = 0; i < stream.size(); i++) {
    stream[i] = htons(stream[i]);
  }

  // Send stream
  int16_t total_sent = 0, sent = 0;
  while (total_sent < stream.size()) {
    sent = send(client_fd, stream.data() + total_sent,
                stream.size() - total_sent, 0);
    if (sent < 0) {
      write_error("Failed to send stream to client " +
                  std::to_string(client_fd));
      return -1;
    }
    total_sent += sent;
  }

  return 0;
}

int16_t send_helo() { return 0; }

int16_t recv_helo() { return 0; }

int16_t send_keepalive(int16_t msg_num) { return 0; }

int16_t get_msg(int16_t group_id) { return 0; }

int16_t send_msg(int16_t to_group, int16_t from_group) { return 0; }

int16_t req_status() { return 0; }

int16_t resp_status() { return 0; }

void handle_client(int64_t client_fd) {
  // Read from client and add switch case to determine which server command
  // function to execute
  std::cout << "Client: " << client_fd << " called\n";
}

int64_t open_socket(int16_t port) {
  struct sockaddr_in sk_addr; // address settings for bind()
  int64_t sock;               // socket opened for this port
  int set = 1;                // for setsockopt

  // Create socket for connection. Set to be non-blocking, so recv will
  // return immediately if there isn't anything waiting to be read.
  if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) < 0) {
    write_error("Failed to open socket");
    return -1;
  }

  // Turn on SO_REUSEADDR to allow socket to be quickly reused after
  // program exit.
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
    write_error("Failed to set SO_REUSEADDR");
  }

  memset(&sk_addr, 0, sizeof(sk_addr));

  sk_addr.sin_family = AF_INET;
  sk_addr.sin_addr.s_addr = INADDR_ANY;
  sk_addr.sin_port = htons(port);

  // Bind to socket to listen for connections from clients
  if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0) {
    write_error("Failed to bind to socket");
    return -1;
  } else {
    return sock;
  }
}

uint16_t setup_server(int16_t port, int64_t &server_fd, int64_t &epoll_fd,
                      epoll_event &event) {
  if ((server_fd = open_socket(port)) < 0) {
    write_error("Failed to open socket");
    return -1;
  }

  if (listen(server_fd, MAX_CLIENTS) == -1) {
    write_error("Failed to listen to server socket");
    close(server_fd);
    return -1;
  }

  if ((epoll_fd = epoll_create1(0)) < 1) {
    write_error("Failed to create epoll instance");
    close(server_fd);
    return -1;
  }

  // Add server socket to epoll
  event.events = EPOLLIN;
  event.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
    write_error("Failed to add server socket to epoll instance");
    close(server_fd);
    close(epoll_fd);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr
        << "Port argument must be specified\nExample: ./tsamgroup9 4009\n";
    return -1;
  }
  int16_t port = atoi(argv[1]);
  int64_t server_fd, epoll_fd;
  struct epoll_event event, events[MAX_EVENTS];
  event = {0};

  if (setup_server(port, server_fd, epoll_fd, event) < 0) {
    write_error("Failed to set up server");
    return -1;
  }
  std::cout << "Server started. Listening on port " << port << "\n";

  bool finished = false;
  int16_t num_events;
  while (!finished) {
    // WARN: -1 is no timeout, this will block until
    // a file descriptor delivers an event
    if ((num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) < 1) {
      write_error("Failed to wait for events");
      return -1;
    }
    for (int16_t i = 0; i < num_events; ++i) {
      if (events[i].data.fd == server_fd) {
        // Accept new client connection
        struct sockaddr_in client_addr;
        socklen_t client_add_len = sizeof(client_addr);
        int64_t client_fd;

        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                                &client_add_len)) < 0) {
          write_error("Failed to accept client connection");
          continue;
        }

        // Add client socket to epoll
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
          write_error("Failed to add client socket to epoll instance");
          close(client_fd);
          continue;
        }

        // Create a new thread to handle the client connection
        std::thread client_thread(handle_client, client_fd);
        client_thread.detach();
      } else {
        // Handle client data
        int64_t client_fd = events[i].data.fd;
        std::thread client_thread(handle_client, client_fd);
        client_thread.detach();
      }
    }
  }
  return 0;
}
