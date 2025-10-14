#include "../include/helpers.h"
#include <cstdio>
#include <string>
#include <system_error>

void write_error(std::string msg, FILE *fd) {
    std::error_code ec(errno, std::system_category());
    fprintf(fd, "MESSAGE: %s\nERROR CODE: %d\nREASON: %s\n", msg.c_str(),
            ec.value(), ec.message().c_str());
}
