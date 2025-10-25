#include <block_bots.hpp>
#include <client_logs.hpp>
#include <chrono>

std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> blocklist;

// Checks if a group id is in the map of blocked servers
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

// Adds the name of the server to be blocked in the map of blocked servers, with the current time
void block_group(const std::string &name) {
    auto now = std::chrono::system_clock::now();
    blocklist[name] = now + std::chrono::minutes(BLOCK_TIME_MINUTES);
    server_log_message(name,"INFO", "Blocked group: " + name + " for " + std::to_string(BLOCK_TIME_MINUTES) + " minutes.");
}

void reset_blocklist() {
    blocklist.clear();
    server_log_message("All", "INFO", "Blocklist has been cleared.");
}

// Removes a server from the blocked servers map if it has been blocked for more than 30 minutes
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
