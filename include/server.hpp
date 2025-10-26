#ifndef SERVER_CLASS
#define SERVER_CLASS

#include "./helpers.h"
#include "./serv_client.hpp"
#include "client_logs.hpp"
#include "operations.hpp"
#include "block_bots.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
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
#include <vector>

constexpr uint32_t MAX_MESSAGES = 10000; // NOTE: uint32_t just in case we want to change to something bigger
constexpr int8_t MAX_CLIENTS = 8;
constexpr int8_t MAX_EVENTS = 10;
constexpr int16_t EPOLL_BLOCK = 5000; // Block epoll for 5 seconds

class Server {
    std::mutex server_mutex;
    std::unique_lock<std::mutex> main_thread_lock;
    std::map<int64_t, std::shared_ptr<Client>> clients;
    std::queue<std::string> message_store;
    std::map<std::string, std::queue<std::string>> group_messages;

    int64_t server_fd, epoll_fd;
    epoll_event event, poll_events[MAX_EVENTS];

    int64_t server_start_listening(int16_t port) {
        int64_t sock; // socket opened for this port
        int set = 1; // for setsockopt

        // Create socket for connection. Set to be non-blocking, so recv will return immediately if there isn't anything waiting to be read.
        if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) < 0) {
            server_log_message("A5_9", "ERROR","Failed to open socket", "server_start_listening");;
            return -1;
        }

        // Allow socket to be quickly reused after program exit
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
            server_log_message("A5_9", "ERROR","Failed to set SO_REUSEADDR", "server_start_listening");
        }

        sk_addr.sin_family = AF_INET;
        sk_addr.sin_addr.s_addr = INADDR_ANY;
        sk_addr.sin_port = htons(port);

        // Bind to socket to listen for connections from clients
        if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0) {
            server_log_message("A5_9", "ERROR","Failed to bind to socket", "server_start_listening");
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

    Server(int16_t port) : server_fd(-1), epoll_fd(-1), main_thread_lock(server_mutex, std::defer_lock) {
        if ((server_fd = server_start_listening(port)) < 0) {
            server_log_message(name, "ERROR","Failed to open socket", "Server constructor");
            exit(-1);
        }

        if (listen(server_fd, MAX_CLIENTS) == -1) {
            server_log_message(name, "ERROR","Failed to listen to server socket", "Server constructor");
            cleanup();
            exit(-1);
        }

        if ((epoll_fd = epoll_create1(0)) < 1) {
            server_log_message(name, "ERROR","Failed to create epoll instance", "Server constructor");
            cleanup();
            exit(-1);
        }

        // Add server socket to epoll
        event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
        event.data.fd = server_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
            server_log_message(name, "ERROR","Failed to add server socket to epoll instance", "Server constructor");
            cleanup();
            exit(-1);
        }
    }

    /*
        Handle messages for non-connected servers -> Unknown groups
    */

    size_t group_num_messages(const std::string& group, std::unique_lock<std::mutex>& lock){
        if(!lock.owns_lock()){
            lock.lock();
        }
        auto group_queue = group_messages.find(group);
        if(group_queue == group_messages.end()) return 0;
        size_t check = group_queue->second.size();
        lock.unlock();
        return check;
    }

    void store_message_for_group(const std::string& group, const std::string& message, std::unique_lock<std::mutex>& lock){
        if(!lock.owns_lock()){
            lock.lock();
        }

        auto it = group_messages.find(group);
        if (it != group_messages.end()) {
            it->second.push(message);
        }

        lock.unlock();
    }

    std::string fetch_message_for_group(const std::string& group, std::unique_lock<std::mutex>& lock){
        if(!lock.owns_lock()){
            lock.lock();
        }
        auto group_queue = group_messages.find(group);
        if(group_queue == group_messages.end()) return "";
        if(group_queue->second.empty()) return "";
        std::string message_copy = group_queue->second.front();
        lock.unlock();
        return message_copy;
    }

    void pop_message_for_group(const std::string& group, std::unique_lock<std::mutex>& lock){
        if(!lock.owns_lock()){
            lock.lock();
        }
        auto group_queue = group_messages.find(group);
        if(group_queue == group_messages.end()) return;
        if(group_queue->second.empty()) return;
        group_queue->second.pop();
        lock.unlock();
    }

    void start() {
        int sleep_ms = 1;
        std::vector<std::pair<std::shared_ptr<Client>, uint32_t>> triggered;
        dispatch(send_helos, std::ref(*this));
        dispatch(keep_alive_loop, std::ref(*this),std::ref(main_thread_lock));
        while (true) {
            // Start by initialiseing a connection with the instructor servers
            if (clients.size() == 0) {
                open_client_socket(std::ref(*this), "130.208.246.98", 5001, "Instr_1", std::ref(main_thread_lock));
                open_client_socket(std::ref(*this), "130.208.246.98", 5002, "Instr_2", std::ref(main_thread_lock));
            }

            int16_t num_events;
            if ((num_events = epoll_wait(epoll_fd, poll_events, MAX_EVENTS, EPOLL_BLOCK)) < 0) {
                server_log_message("A5_9" ,"ERROR","Failed to wait for events", "server.start");
                cleanup();
                exit(-1);
            }

            for (int16_t i = 0; i < num_events; ++i) {
                if (check_for_new_client(i)){
                    register_client(main_thread_lock);
                }
                else {
                    int64_t fd = get_event_socket(i);
                    uint32_t events = get_event_status(i);
                    std::shared_ptr<Client> client = get_server_client(fd, main_thread_lock);

                    if(client == nullptr){ continue; }

                    if(get_event_status(i) & EPOLLERR){
                        int error = 0;
                        socklen_t len = sizeof(error);
                        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
                        server_log_message(client->name, "ERROR","Error On Socket " + client->name + " Error Code: " + std::to_string(error), "server.start");
                        if(error == EPIPE) continue;// Write To Socket That Wasn't Ready
                        if(error == ETIMEDOUT) block_group(client->name);// Connection Timeout
                        if(error == ECONNREFUSED) block_group(client->name);// Connection Refused
                        if(error == ENETUNREACH) block_group(client->name);
                        if(error == ENETRESET && (client->name != "A5_9" || client->name != "A5_09")) block_group(client->name);

                        delete_client(client->fd, main_thread_lock);
                        continue;
                    }

                    if(get_event_status(i) & EPOLLHUP){
                        server_log_message(client->name, "INFO","Client " + client->name + " Hung Its Connection.");
                        delete_client(client->fd, main_thread_lock);
                        continue;
                    }

                    uint32_t new_events = 0;
                    if (get_event_status(i) & EPOLLIN) new_events |= EPOLLIN;
                    if ((get_event_status(i) & EPOLLOUT) && !client->outbound_messages.empty()) new_events |= EPOLLOUT;

                    if(new_events != 0){
                        triggered.push_back(std::make_pair(client, new_events));
                    }
                }
            }

            if(!triggered.empty()){
                if(!main_thread_lock.owns_lock()) main_thread_lock.lock();
                for(auto &[client, events] : triggered){
                    client->notify(events);
                }
                if(main_thread_lock.owns_lock()) main_thread_lock.unlock();
                triggered.clear();
                sleep_ms = 1;
            }
            else{
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                sleep_ms = std::min(sleep_ms * 2, 50);
            }

        }
    }

    /*Grabs The Socket FD For The Socket That Triggered Event At Index event_index*/
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

    void modify_write_events(int64_t client_fd, bool enable){
        epoll_event ev{};
        ev.data.fd = client_fd;
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        if(enable)
            ev.events |= EPOLLOUT;

        if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, & ev) < 0){
            server_log_message("EPOLL", "ERROR", "Write Event Toggle Failed");
        }
    }

    uint8_t add_to_epoll(const std::string name, int64_t client_fd) {
        // Add client socket to epoll
        event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
            server_log_message(name, "ERROR", "Failed to add client socket to epoll instance", "Server.add_to_epoll");
            close(client_fd);
            return -1;
        }

        return 0;
    }

    /*Accepts And Registers A New Client Into The Polling Event System*/
    void register_client(std::unique_lock<std::mutex>& lock) {
        if(!lock.owns_lock()){
            lock.lock();
        }

        if (clients.size() == MAX_CLIENTS) { // If client connections are at max capacity delete a client
            bool client_deleted = false;
            // If an already connected client is an instructor delete that client
            for (auto it = clients.begin(); it != clients.end(); ++it) {
                if (!it->second->name.empty() && it->second->name.rfind("A5_", 0) == std::string::npos) {
                    delete_client(it->second->fd, lock);
                    client_deleted = true;
                    break;
                }
            }
            // If none of the connected clients are instructors, delete the oldest client
            if (!client_deleted) {
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    // Deletes oldest client
                    if (it->second->client_num == 1) {
                        delete_client(it->second->fd, lock);
                        break;
                    }
                }
            }
        }


        // Accept new client connection
        std::shared_ptr<Client> client = std::make_shared<Client>();
        socklen_t client_add_len = sizeof(client->addr);

        // If the the server is blocked, do not accept the connection
        if (is_blocked(client->name)) {
            server_log_message(client->name, "INFO", "The client " + client->name + " is blocked!");
            return;
        }

        if ((client->fd = accept(server_fd, (struct sockaddr *)&client->addr, &client_add_len)) < 0) {
            server_log_message(client->name, "ERROR","Failed to accept client connection", "Server.register_client");
            return;
        } else {
            server_log_message(client->name, "INFO", std::string("Accepting Client [IP]: ").append(inet_ntoa(client->addr.sin_addr)).append(" [ID]: tmp [PORT]: ").append(std::to_string(client->addr.sin_port)));
        }

        client->client_num = clients.size();

        if(!lock.owns_lock()){
            lock.lock(); // Lock Again because of another critical section and the functions above do unlock
        }

        // Add client socket to epoll
        if (add_to_epoll(client->name, client->fd) != 0) {
            close(client->fd);
            return;
        }

        add_client(std::make_pair(client->fd, client), lock);

        dispatch(send_helo, std::ref(*client)); // Initial HELO (This Will Exit As Soon As It's Done Sending)
        dispatch(client_worker,std::ref(*this), client->fd);
    }

    void delete_client(int64_t client_fd, std::unique_lock<std::mutex>& lock) {
        if(!lock.owns_lock()){
            lock.lock();
        }
        std::shared_ptr<Client> client;
        // Hold server lock only while we modify server state
        auto it = clients.find(client_fd);
        if (it == clients.end())
            return;
        client = it->second;
        clients.erase(client->fd);
        client_names.erase(client->name);

        // adjust indices while still holding server lock
        for (auto it2 = clients.begin(); it2 != clients.end(); ++it2) {
            if (it2->second->client_num >= client->client_num) {
                it2->second->client_num--;
            }
        }
        // Now perform client-specific cleanup under the client mutex
        client->close_con(epoll_fd);
        if(lock.owns_lock()){
            lock.unlock();
        }
    }

    void add_client(std::pair<int64_t, std::shared_ptr<Client>> new_client, std::unique_lock<std::mutex>&lock){
        if(!lock.owns_lock()){
            lock.lock();
        }
        if (!new_client.second) {
            return;
        }
        new_client.second->set_epoll_fd(epoll_fd);
        clients.insert(new_client);
        client_names.insert(new_client.second->name);
        if(lock.owns_lock()){
            lock.unlock();
        }
    }

    /*Dispatches a new thread running handler with the supplied arguments*/
    template <typename Func, typename... Args>
    void dispatch(Func&& handler, Args&&... arguments) {
        std::thread client_thread(std::forward<Func>(handler), std::forward<Args>(arguments)...);
        client_thread.detach();
    }

    // Stores the messages for it's own client in a queue
    void store_message_for_cnc(const std::string &message, std::unique_lock<std::mutex>& lock) {
        if(!lock.owns_lock()){
            lock.lock();
        }

        message_store.push(message);

        if(lock.owns_lock()){
            lock.unlock();
        }
    }

    // Get's a message for own client to send to it if it sends GETMSG
    std::string get_message_for_cnc(std::unique_lock<std::mutex>& lock) {
        if(!lock.owns_lock()){
            lock.lock();
        }
        if (message_store.empty()) {
            return "No Messages!!";
        }

        std::string message = message_store.front();
        message_store.pop();
        if(lock.owns_lock()){
            lock.unlock();
        }
        return message;
    }

    // Gets a specific server connected to this server
    std::shared_ptr<Client> get_server_client(int64_t client_fd, std::unique_lock<std::mutex>& lock) { 
        if(!lock.owns_lock()){
            lock.lock();
        }

        auto it = clients.find(client_fd);
        if (it == clients.end()) {
            return nullptr;
        }
        auto client = it->second;
        if(lock.owns_lock()){
            lock.unlock();
        }

        return client; 
    }

    // Gets all servers connected to this server
    std::map<int64_t, std::shared_ptr<Client>> get_server_clients(std::unique_lock<std::mutex>& lock) {
        if(!lock.owns_lock()){
            lock.lock();
        }
        auto snap_shot = clients;
        if(lock.owns_lock()){
            lock.unlock();
        }
        return clients; 
    }

    std::mutex& get_mutex(){
        return server_mutex;
    }

    ~Server() { cleanup(); }
};

#endif
