#ifndef SERVER_CLASS
#define SERVER_CLASS
#include <thread>
#include <utility>
#include "helpers.h"
#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_CLIENTS 8
#define MAX_EVENTS 10
#define EPOLL_BLOCK -1

class Server{
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

    bool CheckForNewClient(int event_index) {
        return poll_events[event_index].data.fd == server_fd;
    }

    int WaitForEvents(){
        int num_events;
        if ((num_events = epoll_wait(epoll_fd, poll_events, MAX_EVENTS,
                                     EPOLL_BLOCK)) < 1) {
            write_error("Failed to wait for events");
            cleanup();
            exit(-1);
        }

        return num_events;
    }

    /*Accepts And Registers A New Client Into The Polling Event System*/
    void RegisterClient() {
        // Accept new client connection
        struct sockaddr_in client_addr;
        socklen_t client_add_len = sizeof(client_addr);
        int64_t client_fd;

        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                                &client_add_len)) < 0) {
            write_error("Failed to accept client connection");
            return;
        }

        // Add client socket to epoll
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
            write_error("Failed to add client socket to epoll instance");
            close(client_fd);
            return;
        }
    }

    /*Grabs The Socket FD For The Socket That Triggered Event At Index
     * event_index*/
    int64_t GetEventSocket(int event_index) {
        return poll_events[event_index].data.fd;
    }

    /*Dispatches a new thread running handler with the supplied arguments*/
    template <typename Func, typename... Args>
    void Dispatch(Func &&handler, Args &&...arguments) {
        std::thread client_thread(std::forward<Func>(handler),
                                  std::forward<Args>(arguments)...);
        client_thread.detach();
    }

    ~Server() { cleanup(); }
};

#endif
