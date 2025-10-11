#include <cstdint>
#include <netinet/in.h>
#include <queue>
#include <string>

class Client {
  public:
    struct Msg {
        int16_t total_sent;
        int16_t length;
        char *stream_data;
    };
    std::queue<Msg> commands;

    std::string name;
    struct sockaddr_in addr = {0};
    int64_t fd;

    Client() : name(""), fd(-1) {};
};
