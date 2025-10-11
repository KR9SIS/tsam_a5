#ifndef SERVER_CLASS
#define SERVER_CLASS
#include "./serv_client.hpp"
#include "helpers.h"
#include <cstdint>
#include <cstdlib>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <unistd.h>
#include <utility>

constexpr int8_t MAX_CLIENTS = 8;
constexpr int8_t MAX_EVENTS = 10;
constexpr int8_t EPOLL_BLOCK = -1;

class Server {
    std::map<int64_t, Client> clients;

    int64_t server_fd, epoll_fd;
    epoll_event event, poll_events[MAX_EVENTS];

    int64_t open_socket(int16_t port) {
        sockaddr_in sk_addr = {0}; // address settings for bind()
        int64_t sock;              // socket opened for this port
        int set = 1;               // for setsockopt

        // Create socket for connection. Set to be non-blocking, so recv will
        // return immediately if there isn't anything waiting to be read.
        if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) <
            0) {
            write_error("Failed to open socket");
            return -1;
        }

        // Turn on SO_REUSEADDR to allow socket to be quickly reused after
        // program exit.
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
            write_error("Failed to set SO_REUSEADDR");
        }

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

    void cleanup() {
        if (server_fd != -1) {
            close(server_fd);
            server_fd = -1;
        }
        if (epoll_fd != -1) {
            close(epoll_fd);
            epoll_fd = -1;
        }
    }

  public:
    Server(int16_t port) : server_fd(-1), epoll_fd(-1) {
        if ((server_fd = open_socket(port)) < 0) {
            write_error("Failed to open socket");
            exit(-1);
        }

        if (listen(server_fd, MAX_CLIENTS) == -1) {
            write_error("Failed to listen to server socket");
            cleanup();
            exit(-1);
        }

        if ((epoll_fd = epoll_create1(0)) < 1) {
            write_error("Failed to create epoll instance");
            cleanup();
            exit(-1);
        }

        // Add server socket to epoll
        event.events = EPOLLIN;
        event.data.fd = server_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
            write_error("Failed to add server socket to epoll instance");
            cleanup();
            exit(-1);
        }
    }

    bool check_for_new_client(int8_t event_index) {
        return poll_events[event_index].data.fd == server_fd;
    }

    int8_t wait_for_events() {
        int8_t num_events;
        if ((num_events = epoll_wait(epoll_fd, poll_events, MAX_EVENTS,
                                     EPOLL_BLOCK)) < 1) {
            write_error("Failed to wait for events");
            cleanup();
            exit(-1);
        }

        return num_events;
    }

    /*Accepts And Registers A New Client Into The Polling Event System*/
    void register_client() {
        if (clients.size() > MAX_CLIENTS) {
            for (std::map<int64_t, Client>::iterator it = clients.begin();
                 it != clients.end(); ++it) {
                if (it->second.client_num == 1) {
                    delete_client(it->second);
                    break;
                }
            }
        }
        // Accept new client connection
        Client client = Client();
        socklen_t client_add_len = sizeof(client.addr);

        if ((client.fd = accept(server_fd, (struct sockaddr *)&client.addr,
                                &client_add_len)) < 0) {
            write_error("Failed to accept client connection");
            return;
        }

        client.client_num = clients.size();
        clients[client.fd] = client;
        // Add client socket to epoll
        event.events = EPOLLIN;
        event.data.fd = client.fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.fd, &event) == -1) {
            write_error("Failed to add client socket to epoll instance");
            close(client.fd);
            return;
        }
    }

    void delete_client(Client client) {
        clients.erase(client.fd);
        close(client.fd);

        for (std::map<int64_t, Client>::iterator it = clients.begin();
             it != clients.end(); ++it) {
            if (it->second.client_num >= client.client_num) {
                it->second.client_num--;
            }
        }
    }

    /*Grabs The Socket FD For The Socket That Triggered Event At Index
     * event_index*/
    int64_t get_event_socket(int event_index) {
        return poll_events[event_index].data.fd;
    }

    /*Dispatches a new thread running handler with the supplied arguments*/
    template <typename Func, typename... Args>
    void dispatch(Func &&handler, Args &&...arguments) {
        std::thread client_thread(std::forward<Func>(handler),
                                  std::forward<Args>(arguments)...);
        client_thread.detach();
    }

    Client& get_server_client(int64_t client_fd) { return clients[client_fd]; }

    std::map<int64_t, Client>& get_server_clients(int64_t client_fd) {
        return clients;
    }

    ~Server() { cleanup(); }

    uint32_t get_event_status(int event_index) {
        return poll_events[event_index].events;
    }
};

#endif
