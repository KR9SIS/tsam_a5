#ifndef SERVER_CLIENT
#define SERVER_CLIENT

#include "./helpers.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/epoll.h>
#include <vector>

// WARNING: Not Sure If We Should Use This Or
// Just Count For How Many Minutes Each Client Has Remained Dormant(AKA No
// KeepAlive)
enum class ClientState { ACTIVE = 0, INACTIVE = 1 };

class Client {
    std::mutex client_mutex;
    int events;

    struct conn {
        std::string name;
        std::string ip;
        uint16_t port;
    };

  public:
    std::queue<std::string> outbound_messages;
    ClientState status;

    std::string name;
    sockaddr_in addr = {0};
    int64_t fd;
    int8_t client_num;
    uint16_t pending_messages;
    std::map<std::string, conn> connections;

    Client() : name(""), fd(-1), status(ClientState::ACTIVE), events(0) {};
    Client(const Client &other)
        : name(other.name), fd(other.fd), status(other.status),
          events(other.events), addr(other.addr), client_num(other.client_num),
          connections(other.connections),
          pending_messages(other.pending_messages),
          outbound_messages(other.outbound_messages) {}

    int get_event_status() {
        std::lock_guard<std::mutex> lock(client_mutex);
        return events;
    }

    void set_read() {
        std::lock_guard<std::mutex> lock(client_mutex);
        events |= EPOLLIN;
    }

    void reset_read() {
        std::lock_guard<std::mutex> lock(client_mutex);
        events &= ~EPOLLIN;
    }

    void set_write() {
        std::lock_guard<std::mutex> lock(client_mutex);
        events |= EPOLLOUT;
    }

    void reset_write() {
        std::lock_guard<std::mutex> lock(client_mutex);
        events &= ~EPOLLOUT;
    }

    void push_message(const std::string &message) {
        std::lock_guard<std::mutex> lock(client_mutex);
        outbound_messages.push(message);
    }

    void pop_message() {
        std::lock_guard<std::mutex> lock(client_mutex);
        if (!outbound_messages.empty()) {
            outbound_messages.pop();
        }
    }
    std::string front_message() {
        std::lock_guard<std::mutex> lock(client_mutex);
        if (!outbound_messages.empty()) {
            std::string message = outbound_messages.front();
            return message;
        }
        return "";
    }
};

#endif
