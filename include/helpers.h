#ifndef HELPER_FUNCTIONS
#define HELPER_FUNCTIONS

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
class Client;

constexpr int8_t SOH = 0x01;
constexpr int8_t STX = 0x02;
constexpr int8_t ETX = 0x03;

extern int16_t send_stream(const std::string &stream_str, Client &client, std::unique_lock<std::mutex>& lock);
extern std::vector<std::string> extract_parameters(const std::string payload, const std::string delimiter);
extern std::vector<std::string> extract_frames(const std::string &message);
extern std::vector<std::string>
extract_payload(const std::vector<std::string> &messages);
extern uint16_t get_message_length(const std::string& message);

#endif
