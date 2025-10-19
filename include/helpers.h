#ifndef HELPER_FUNCTIONS
#define HELPER_FUNCTIONS

#include <cstdio>
#include <string>
#include <vector>
class Client;

extern void write_error(std::string msg, FILE *fd = stderr);
extern int16_t send_stream(const std::string &stream_str, Client &client);
extern std::vector<std::string> extract_parameters(const std::string payload, const std::string delimiter);
extern std::vector<std::string> extract_frames(const std::string &message);
extern std::vector<std::string>
extract_payload(const std::vector<std::string> &messages);

#endif
