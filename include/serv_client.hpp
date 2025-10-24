#ifndef SERVER_CLIENT
#define SERVER_CLIENT

#include "./helpers.h"
#include "client_logs.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

class Client {
    std::atomic<int32_t> events{0};
    std::condition_variable event_fired;
    std::atomic<bool> closed;
    int64_t epoll_fd;

    struct conn {
        std::string name;
        std::string ip;
        uint16_t port;
    };

    void toggle_epoll_write(bool enable){
        epoll_event ev{};
        ev.data.fd = fd;
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        if(enable) ev.events |= EPOLLOUT;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0){
            server_log_message(name, "ERROR", "FAILED To Enable EPOLL Write Events");
        }
    }

  public:
    std::mutex client_mutex;
    std::queue<std::string> outbound_messages;
    std::string inbuf; // Accumulates partial incoming data between recv() calls

    std::string name;
    sockaddr_in addr = {0};
    int64_t fd;
    int8_t client_num;
    uint16_t pending_messages;
    std::map<std::string, conn> connections;
    std::vector<std::string> messages_for_client;

    Client() : name("tmp"), fd(-1), events(0) {};

    void set_epoll_fd(int64_t fd){
        epoll_fd = fd;
    }

    int32_t wait_for_events(){
        std::unique_lock<std::mutex> lk(client_mutex);
        event_fired.wait(lk, [&]{
            return events.load(std::memory_order_acquire) != 0 
            || closed.load(std::memory_order_acquire);
        });

        if(closed.load(std::memory_order_acquire)){
            return -1;
        }

        return events.exchange(0,std::memory_order_acq_rel);
    }

    void notify(int new_events){
        events.fetch_or(new_events, std::memory_order_release);
        event_fired.notify_one();
    }

    void close_con(int64_t epoll_fd){
        closed.store(true, std::memory_order_release);
        if(fd <= 0){
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            fd = -1;
        }

        event_fired.notify_one();
    }

    bool is_closed(){
        return closed.load(std::memory_order_acquire);
    }

    void update_connections(std::vector<std::string> conns) {
        server_log_message(name, "PROCESSING", "Updating server connections");
        
        connections.clear();

        if (conns.empty()) {
            return;
        }

        for (std::string conn : conns) {
            std::vector<std::string> parts = extract_parameters(conn, ",");
            if (conn.empty()) {
                break;
            }


            if (parts.empty())
                break;

            if(conn.find("A5_9") != std::string::npos){
                int16_t port_numb = std::stoi(parts[2]);
                addr.sin_port = htonl(port_numb);
                continue;
            }

            else if (this->name != parts[0] && parts.size() >= 3 && parts[1] != "?") {
                struct conn new_conn;
                new_conn.name = parts[0];
                new_conn.ip = parts[1];
                new_conn.port = std::stoi(parts[2]);
                connections[name] = new_conn;
            }
        }
    }

    std::mutex& get_mutex(){
        return client_mutex;
    }

    void push_message(const std::string &message) {
        outbound_messages.push(message);
        toggle_epoll_write(true);
    }

    void pop_message() {
        if (!outbound_messages.empty()) {
            outbound_messages.pop();

            if(outbound_messages.empty()){
                toggle_epoll_write(false);
            }
        }
    }

    std::string front_message() {
        if (!outbound_messages.empty()) {
            std::string message = outbound_messages.front();
            return message;
        }
        return "";
    }

    void append_message(const std::string &message) {
        messages_for_client.push_back(message);
    }

    void remove_first_message() {
        if (!messages_for_client.empty()) {
            messages_for_client.erase(messages_for_client.begin());
        }
    }

    std::string get_message_for_client() {
        if (messages_for_client.empty()) {
            return "";
        }

        std::string message = messages_for_client.front();
        remove_first_message();
        return message;
    }
};

#endif
