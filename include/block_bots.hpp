#ifndef BLOCK_BOTS_H
#define BLOCK_BOTS_H
#define BLOCK_TIME_MINUTES 30 

#include <string>
#include <chrono>
#include <map>

extern std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> blocklist;

bool is_blocked(const std::string &name);
void block_group(const std::string &name);
void reset_blocklist();
void remove_expired_blocks();

#endif