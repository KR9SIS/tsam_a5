#include <cstdint>
#include <netinet/in.h>
#include <queue>
#include <string>

class Client {
    struct TotalSent {
        std::string commmand;
        int16_t total_sent;
    };

  public:
    std::queue<TotalSent> commands;

    std::string name;
    struct sockaddr_in addr = {0};
    int64_t fd;

    Client() : name(""), fd(-1) {};
};
