#include <block_bots.hpp>
#include <client_logs.hpp>
#include <chrono>

std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> blocklist;

bool is_blocked(const std::string &name) {
    auto now = std::chrono::system_clock::now();
    if (blocklist.find(name) != blocklist.end()) {
        if (blocklist[name] > now) {
            return true;
        } else {
            blocklist.erase(name);
        }
    }
    return false;
}

void block_group(const std::string &name) {
    auto now = std::chrono::system_clock::now();
    blocklist[name] = now + std::chrono::minutes(BLOCK_TIME_MINUTES);
    server_log_message(name,"INFO", "Blocked group: " + name + " for " + std::to_string(BLOCK_TIME_MINUTES) + " minutes.");
}

void reset_blocklist() {
    blocklist.clear();
    server_log_message("All", "INFO", "Blocklist has been cleared.");
}

void remove_expired_blocks() {
    auto now = std::chrono::system_clock::now();
    for (auto it = blocklist.begin(); it != blocklist.end();) {
        if (it->second < now) {
            it = blocklist.erase(it);
        } else {
            ++it;
        }
    }
}
