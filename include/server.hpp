#ifndef SERVER_CLASS
#define SERVER_CLASS

#include "./helpers.h"
#include "./serv_client.hpp"
#include "client_logs.hpp"
#include "operations.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <utility>

constexpr uint32_t MAX_MESSAGES = 10000; // NOTE: uint32_t just in case we want to change to something bigger
constexpr int8_t MAX_CLIENTS = 8;
constexpr int8_t MAX_EVENTS = 10;
constexpr int16_t EPOLL_BLOCK = 5000; // Block epoll for 5 seconds

class Server {
    std::mutex server_mutex;
    std::map<int64_t, Client> clients;
    std::queue<std::string> message_store;

    int64_t server_fd, epoll_fd;
    epoll_event event, poll_events[MAX_EVENTS];

    int64_t server_start_listening(int16_t port) {
        int64_t sock;              // socket opened for this port
        int set = 1;               // for setsockopt

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
    uint8_t max_clients = MAX_CLIENTS;
    std::unordered_set<std::string> client_names;
    sockaddr_in sk_addr = {0}; // address settings for bind()
    std::string name = "A5_9";

    Server(int16_t port) : server_fd(-1), epoll_fd(-1) {
        if ((server_fd = server_start_listening(port)) < 0) {
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
        event.events = EPOLLIN | EPOLLOUT;
        event.data.fd = server_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
            write_error("Failed to add server socket to epoll instance");
            cleanup();
            exit(-1);
        }
    }

    void start() {
        dispatch(send_helos, std::ref(*this));
        dispatch(open_client_socket, std::ref(*this), "127.0.0.1", 5001, "I_1");
        dispatch(open_client_socket, std::ref(*this), "127.0.0.1", 5002, "I_2");
        while (true) {
            int8_t num_events;
            if ((num_events = epoll_wait(epoll_fd, poll_events, MAX_EVENTS, EPOLL_BLOCK)) < 0) {
                write_error("Failed to wait for events");
                cleanup();
                exit(-1);
            }

            for (int16_t i = 0; i < num_events; ++i) {
                if (check_for_new_client(i)) {
                    register_client();
                } else {
                    // If EPOLLIN is set, it's ready to read
                    if ((get_event_status(i) & EPOLLIN) != 0) {
                        get_server_client(get_event_socket(i)).set_read();
                    }

                    // If EPOLLOUT is set, it's ready to write
                    if ((get_event_status(i) & EPOLLOUT) != 0) {
                        get_server_client(get_event_socket(i)).set_write();
                    }
                }
            }
            static time_t lastKeepAlive = 0;
            time_t now = time(nullptr);

            if (difftime(now, lastKeepAlive) >= 60) {  // Send KEEPALIVE to servers (clients of this server) every minute
                send_keepalive(clients);
                lastKeepAlive = now;
            }
        }
    }
    /*Grabs The Socket FD For The Socket That Triggered Event At Index
     * event_index*/
    int64_t get_event_socket(int event_index) {
        return poll_events[event_index].data.fd;
    }

    uint32_t get_event_status(int event_index) {
        return poll_events[event_index].events;
    }
    bool check_for_new_client(int8_t event_index) {
        return poll_events[event_index].data.fd == server_fd;
    }

    uint8_t add_to_epoll(int64_t client_fd) {
        // Add client socket to epoll
        event.events = EPOLLIN | EPOLLOUT;
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
            server_log_message("ERROR", "Failed to add client socket to epoll instance");
            close(client_fd);
            return -1;
        }
        return 0;
    }
    /*Accepts And Registers A New Client Into The Polling Event System*/
    void register_client() {
        if (clients.size() == MAX_CLIENTS) {
            for (auto it = clients.begin(); it != clients.end(); ++it) {
                // Deletes oldest client
                if (it->second.client_num == 1) {
                    delete_client(it->second);
                    break;
                }
            }
        }
        // Accept new client connection
        Client client;
        socklen_t client_add_len = sizeof(client.addr);

        if ((client.fd = accept(server_fd, (struct sockaddr *)&client.addr, &client_add_len)) < 0) {
            write_error("Failed to accept client connection");
            return;
        }

        client.client_num = clients.size();
        // Add client socket to epoll
        if (add_to_epoll(client.fd) != 0) {
            close(client.fd);
            return;
        }

        clients.insert(std::pair<int64_t, Client>(client.fd, client));
        client_names.insert(client.name);

        dispatch(send_helo, std::ref(clients[client.fd])); // Initial Hello (This Will Exit As Soon As It's Done Sending)
        // NOTE: Seperate Threads For Read And Write So We Can Spin Wait For
        // Each Event
        dispatch(handle_client_read, std::ref(clients[client.fd]), std::ref(*this)); // Dispatch READ Thread
        dispatch(handle_client_write, std::ref(clients[client.fd]), std::ref(*this)); // Dispatch WRITE Thread
    }

    void delete_client(Client &client) {
        clients.erase(client.fd);
        close(client.fd);
        client_names.erase(client.name);

        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it->second.client_num >= client.client_num) {
                it->second.client_num--;
            }
        }
    }

    /*Dispatches a new thread running handler with the supplied arguments*/
    template <typename Func, typename... Args>
    void dispatch(Func &&handler, Args &&...arguments) {
        std::thread client_thread(std::forward<Func>(handler),
                                  std::forward<Args>(arguments)...);
        client_thread.detach();
    }

    void store_message_for_cnc(const std::string &message) {
        std::lock_guard<std::mutex> lock(server_mutex);
        message_store.push(message);
    }

    std::string get_message_for_cnc() {
        std::lock_guard<std::mutex> lock(server_mutex);
        if (message_store.empty()) {
            return "No Messages!!";
        }

        std::string message = message_store.front();
        message_store.pop();
        return message;
    }

    Client &get_server_client(int64_t client_fd) { return clients[client_fd]; }

    std::map<int64_t, Client> &get_server_clients() { return clients; }

    ~Server() { cleanup(); }
};

#endif
