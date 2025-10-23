#ifndef SERVER_CLASS
#define SERVER_CLASS

#include "./helpers.h"
#include "./serv_client.hpp"
#include "client_logs.hpp"
#include "operations.hpp"
#include "block_bots.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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
    std::map<int64_t, std::shared_ptr<Client>> clients;
    std::queue<std::string> message_store;

    int64_t server_fd, epoll_fd;
    epoll_event event, poll_events[MAX_EVENTS];

    int64_t server_start_listening(int16_t port) {
        int64_t sock;              // socket opened for this port
        int set = 1;               // for setsockopt

        // Create socket for connection. Set to be non-blocking, so recv will
        // return immediately if there isn't anything waiting to be read.
        if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) < 0) {
            server_log_message("ERROR","Failed to open socket");;
            return -1;
        }

        // Allow socket to be quickly reused after program exit
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
            server_log_message("ERROR","Failed to set SO_REUSEADDR");
        }

        sk_addr.sin_family = AF_INET;
        sk_addr.sin_addr.s_addr = INADDR_ANY;
        sk_addr.sin_port = htons(port);

        // Bind to socket to listen for connections from clients
        if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0) {
            server_log_message("ERROR","Failed to bind to socket");
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
            server_log_message("ERROR","Failed to open socket");
            exit(-1);
        }

        if (listen(server_fd, MAX_CLIENTS) == -1) {
            server_log_message("ERROR","Failed to listen to server socket");
            cleanup();
            exit(-1);
        }

        if ((epoll_fd = epoll_create1(0)) < 1) {
            server_log_message("ERROR","Failed to create epoll instance");
            cleanup();
            exit(-1);
        }

        // Add server socket to epoll
        event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
        event.data.fd = server_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
            server_log_message("ERROR","Failed to add server socket to epoll instance");
            cleanup();
            exit(-1);
        }
    }

    void start() {
        dispatch(send_helos, std::ref(*this));
        dispatch(open_client_socket, std::ref(*this), "130.208.246.98", 5001, "Instr_1");
        dispatch(open_client_socket, std::ref(*this), "130.208.246.98", 5002, "Instr_2");
        while (true) {
            int16_t num_events;
            if ((num_events = epoll_wait(epoll_fd, poll_events, MAX_EVENTS, EPOLL_BLOCK)) < 0) {
                server_log_message("ERROR","Failed to wait for events");
                cleanup();
                exit(-1);
            }

            for (int16_t i = 0; i < num_events; ++i) {
                if (check_for_new_client(i)) {
                    register_client();
                } else {
                    int64_t fd = get_event_socket(i);
                    uint32_t events = get_event_status(i);
                    std::shared_ptr<Client> client = get_server_client(fd);

                    if(get_event_status(i) & EPOLLERR){
                        int error = 0;
                        socklen_t len = sizeof(error);
                        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
                        server_log_message("ERROR","Error On Socket " + client->name + " Error Code: " + std::to_string(error));

                        client->close_con();
                        delete_client(fd);
                        continue;
                    }

                    if(get_event_status(i) & EPOLLHUP){
                        server_log_message("INFO","Client " + get_server_client(fd)->name + "Hung Its Connection.");
                        get_server_client(fd)->close_con();
                        delete_client(fd);
                        continue;
                    }

                    uint32_t new_events = 0;
                    // If EPOLLIN is set, it's ready to read
                    if (get_event_status(i) & EPOLLIN) {
                        new_events |= EPOLLIN;
                    }

                    // If EPOLLOUT is set, it's ready to write
                    if ((get_event_status(i) & EPOLLOUT) && !client->outbound_messages.empty()) {
                        new_events |= EPOLLOUT;
                    }

                    client->notify(new_events);
                }
            }

            // Implementation so every client/server gets one keepalive per minute but distributed evenly through that minute
            // This is to not overload the server since sending it all at the same time is too much
            static time_t lastKeepAlive = 0;
            static size_t keepalive_index = 0;

            time_t now = time(nullptr);
            auto &client_map = clients;

            if (!client_map.empty()) {
                double interval = 60.0 / client_map.size(); // To distribute evenly

                if (difftime(now, lastKeepAlive) >= interval) {
                    auto it = std::next(client_map.begin(), keepalive_index % client_map.size());
                    if (it->second->fd >= 0) {
                        send_keepalive_helper(*it->second);
                    }
                    keepalive_index++;
                    lastKeepAlive = now;
                }
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

    int64_t get_server_socket() const {
        return server_fd;
    }

    uint8_t add_to_epoll(int64_t client_fd) {
        // Add client socket to epoll
        event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR;
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
                if (it->second->client_num == 1) {
                    delete_client(*it->second);
                    break;
                }
            }
        }
        // Accept new client connection
        std::shared_ptr<Client> client = std::make_shared<Client>();
        socklen_t client_add_len = sizeof(client->addr);

        if (is_blocked(client->name)) {
            server_log_message("INFO", "The client " + client->name + " is blocked!");
            return;
        }

        if ((client->fd = accept(server_fd, (struct sockaddr *)&client->addr, &client_add_len)) < 0) {
            server_log_message("ERROR","Failed to accept client connection");
            return;
        }

        client->client_num = clients.size();
        // Add client socket to epoll
        if (add_to_epoll(client->fd) != 0) {
            close(client->fd);
            return;
        }

        std::lock_guard<std::mutex> lk(server_mutex);
        clients.insert(std::pair<int64_t, std::shared_ptr<Client>>(client->fd, client));
        client_names.insert(client->name);

        server_log_message("ACCEPT","Registering New Client: " + client->name);
        dispatch(send_helo, std::ref(*clients[client->fd])); // Initial Hello (This Will Exit As Soon As It's Done Sending)
        // NOTE: Seperate Threads For Read And Write So We Can Spin Wait For
        // Each Event
        dispatch(client_worker,std::ref(*this), client->fd);
    }

    void delete_client(Client &client) {
        std::lock_guard<std::mutex> lk(server_mutex);
        client.close_con();
        std::lock_guard<std::mutex> client_lk(client.get_mutex());
        clients.erase(client.fd);
        client_names.erase(client.name);

        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it->second->client_num >= client.client_num) {
                it->second->client_num--;
            }
        }
    }

    /*Dispatches a new thread running handler with the supplied arguments*/
    template <typename Func, typename... Args>
    void dispatch(Func&& handler, Args&&... arguments) {
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

    std::shared_ptr<Client>& get_server_client(int64_t client_fd) { 
        std::lock_guard<std::mutex> lk(server_mutex);
        return clients[client_fd]; 
    }

    std::map<int64_t, std::shared_ptr<Client>> get_server_clients() {
        std::lock_guard<std::mutex> lk(server_mutex);
        return clients; 
    }

    void delete_client(int64_t client_fd){
        std::lock_guard<std::mutex> lk(server_mutex);
        clients.erase(client_fd);
    }

    void add_client(std::pair<int64_t, std::shared_ptr<Client>> new_client){
        std::lock_guard<std::mutex> lk(server_mutex);
        clients.insert(new_client);
    }

    std::mutex& get_mutex(){
        return server_mutex;
    }

    ~Server() { cleanup(); }
};

#endif
