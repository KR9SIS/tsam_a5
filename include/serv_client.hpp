#ifndef SERVER_CLIENT
#define SERVER_CLIENT

#include <cstdint>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/epoll.h>
#include <vector>
#include <mutex>

// WARNING: Not Sure If We Should Use This Or 
// Just Count For How Many Minutes Each Client Has Remained Dormant(AKA No KeepAlive)
enum class ClientState{ 
    ACTIVE = 0,
    INACTIVE = 1
};


class Client {
    std::mutex client_event_mutex;
    int events;

  public:
    struct Msg {
        int16_t total_sent;
        int16_t length;
        char *stream_data;
    };
    std::queue<Msg> commands;
    ClientState status;

    std::string name;
    sockaddr_in addr = {0};
    int64_t fd;
    int8_t client_num;
    uint16_t pending_messages;
    std::vector<std::string> connected;

    Client() : name(""), fd(-1), status(ClientState::ACTIVE), events(0) {};
    Client(const Client& other): name(other.name), fd(other.fd), status(other.status), events(other.events),
        addr(other.addr), client_num(other.client_num), connected(other.connected), pending_messages(other.pending_messages),
        commands(other.commands){}
    
    int get_event_status(){
        return events;
    }

    void set_read(){
        client_event_mutex.lock();
        events |= EPOLLIN;
        client_event_mutex.unlock();
    }

    void reset_read(){
        client_event_mutex.lock();
        events &= ~EPOLLIN;
        client_event_mutex.unlock();
    }

    void set_write(){
        client_event_mutex.lock();
        events |= EPOLLOUT;
        client_event_mutex.unlock();
    }

    void reset_write(){
        client_event_mutex.lock();
        events &= ~EPOLLOUT;
        client_event_mutex.unlock();
    }
};

#endif
