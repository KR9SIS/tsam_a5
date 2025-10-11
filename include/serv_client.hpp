#include <cstdint>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <vector>

// WARNING: Not Sure If We Should Use This Or 
// Just Count For How Many Minutes Each Client Has Remained Dormant(AKA No KeepAlive)
enum class ClientState{ 
    ACTIVE = 0,
    INACTIVE = 1
};

class Client {
  public:
    struct Msg {
        int16_t total_sent;
        int16_t length;
        char *stream_data;
    };
    std::queue<Msg> commands;
    ClientState status;

    std::string name;
    struct sockaddr_in addr = {0};
    int64_t fd;
    int8_t client_num;
    uint16_t pending_messages;
    std::vector<std::string> connected;

    Client() : name(""), fd(-1), status(ClientState::ACTIVE) {};
};
