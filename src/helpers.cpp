#include "helpers.h"
#include <cstdio>
#include <string>
#include <system_error>

void write_error(std::string msg, FILE *fd) {
    std::error_code ec(errno, std::system_category());
    fprintf(fd, "MESSAGE: %s\n ERROR CODE: %d\n REASON: %s", msg.c_str(),
            ec.value(), ec.message().c_str());
}
