#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

constexpr u_int8_t BACKLOG = 5; // Max length of queue of waiting connections
constexpr u_int8_t MAX_CLIENTS = 10; // Maximum number of clients
constexpr u_int8_t MAX_EVENTS = 10;  // Maximum number of events

int16_t send_helo() { return 0; }

int16_t recv_helo() { return 0; }

int16_t send_keepalive(int16_t msg_num) { return 0; }

int16_t get_msg(int16_t group_id) { return 0; }

int16_t send_msg(int16_t to_group, int16_t from_group) { return 0; }

int16_t req_status() { return 0; }

int16_t resp_status() { return 0; }

int64_t open_socket(int16_t port) {
  struct sockaddr_in sk_addr; // address settings for bind()
  int64_t sock;               // socket opened for this port
  int set = 1;                // for setsockopt

  // Create socket for connection. Set to be non-blocking, so recv will
  // return immediately if there isn't anything waiting to be read.
  if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to open socket\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
    return -1;
  }

  // Turn on SO_REUSEADDR to allow socket to be quickly reused after
  // program exit.
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to set SO_REUSEADDR\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
  }

  memset(&sk_addr, 0, sizeof(sk_addr));

  sk_addr.sin_family = AF_INET;
  sk_addr.sin_addr.s_addr = INADDR_ANY;
  sk_addr.sin_port = htons(port);

  // Bind to socket to listen for connections from clients
  if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to bind to socket\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
    return -1;
  } else {
    return sock;
  }
}

uint16_t setup_server(int16_t port, int64_t &server_fd, int64_t &epoll_fd,
                      epoll_event &event) {
  if ((server_fd = open_socket(port)) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to open socket\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
    return -1;
  }

  if (listen(server_fd, MAX_CLIENTS) == -1) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to listen to server socket\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
    close(server_fd);
    return -1;
  }

  if ((epoll_fd = epoll_create1(0)) < 1) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to create epoll instance\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
    close(server_fd);
    return -1;
  }

  // Add server socket to epoll
  event.events = EPOLLIN;
  event.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to add server socket to epoll instance\nret: "
              << ec.value() << "\nmsg: " << ec.message() << "\n";

    close(server_fd);
    close(epoll_fd);
    return 1;
  }
  return 0;
}

void handle_client(int64_t client_fd) {
  // Read from client and add switch case to determine which server command
  // function to execute
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

  if (setup_server(port, server_fd, epoll_fd, event) < 0) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to set up server\nret: " << ec.value()
              << "\nmsg: " << ec.message() << "\n";
    return -1;
  }
  std::cout << "Server started. Listening on port " << port << "\n";

  bool finished = false;
  int16_t num_events;
  while (!finished) {
    // WARN: -1 is no timeout, this will block until
    // a file descriptor delivers an event
    if ((num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) < 0) {
      std::error_code ec(errno, std::system_category());
      std::cerr << "Failed to wait for events\nret: " << ec.value()
                << "\nmsg: " << ec.message() << "\n";
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
          std::error_code ec(errno, std::system_category());
          std::cerr << "Failed to accept client connection\nret: " << ec.value()
                    << "\nmsg: " << ec.message() << "\n";
          continue;
        }

        // Add client socket to epoll
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
          std::error_code ec(errno, std::system_category());
          std::cerr << "Failed to add client socket to epoll instance\nret: "
                    << ec.value() << "\nmsg: " << ec.message() << "\n";
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
