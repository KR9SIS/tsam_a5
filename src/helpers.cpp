#include "helpers.h"
#include "client_logs.hpp"
#include "serv_client.hpp"
#include <cstdio>
#include <string>
#include <system_error>

void write_error(std::string msg, FILE *fd) {
    std::error_code ec(errno, std::system_category());
    fprintf(fd, "MESSAGE: %s\nERROR CODE: %d\nREASON: %s\n", msg.c_str(), ec.value(), ec.message().c_str());
}

int16_t send_stream(const std::string &stream_str, Client &client) {
    constexpr uint16_t frame_overhead =
        1 /*SOH*/ + 2 /*len*/ + 1 /*STX*/ + 1 /*ETX*/;
    constexpr uint16_t MAX_STREAM_LEN = 5000;

    if (stream_str.size() > MAX_STREAM_LEN - frame_overhead) {
        write_error("Tried To Send A Message Exceeding Max Length");
        server_log_message("ERROR", "Tried to send a message exceeding max length");
        return -1;
    }

    uint16_t total_len = static_cast<uint16_t>(stream_str.size() + frame_overhead);

    std::vector<char> stream;
    stream.reserve(total_len);
    stream.push_back(SOH);
    stream.push_back(static_cast<char>((total_len >> 8) & 0xFF)); // high byte
    stream.push_back(static_cast<char>(total_len & 0xFF));        // LOW byte
    stream.push_back(STX);
    stream.insert(stream.end(), stream_str.begin(), stream_str.end());
    stream.push_back(ETX);

    // Use the vector's size (host order) as the number of bytes to send
    ssize_t sent = 0;
    server_log_message("SENDING", stream_str);
    while ((sent += send(client.fd, stream.data() + sent, stream.size() - sent, 0)) != stream.size()) {
        if (sent < 0) {
            write_error("Failed to send stream(" + stream_str + ") to client " + std::to_string(client.fd));
            server_log_message("ERROR", "Failed to send message: " + stream_str);
            return -1;
        } else if (sent != stream.size()) {
            server_log_message("SENDING", "Sent " + std::to_string(sent) + " bytes of " + std::to_string(stream.size()));
        }
    }
    server_log_message("SENT", stream_str);
    return 0;
}

std::vector<std::string> extract_parameters(const std::string payload, const std::string delimiter) {

    std::vector<std::string> params;

    if (payload.empty() || delimiter.empty()) {
        return params;
    }

    size_t prev_delim = -delimiter.size();
    size_t delim_pos = 0;

    // Find all delimiters and extract substrings between them
    while ((delim_pos = payload.find(delimiter, prev_delim + delimiter.size())) != std::string::npos) {
        params.push_back(payload.substr(prev_delim + delimiter.size(), delim_pos - prev_delim - delimiter.size()));
        prev_delim = delim_pos;
    }

    // Push the last segment after the final delimiter
    params.push_back(payload.substr(prev_delim + delimiter.size()));

    return params;
}

/// Extracts The Packets From The Given Message
///
/// Example:
///     <SOH><length><STX><message><ETX><SOH><length><STX><message><ETX>
///
///     =>
///     {<SOH><length><STX><message><ETX>,<SOH><length><STX><message><ETX>}
std::vector<std::string> extract_frames(const std::string &message) {
    std::vector<std::string> packets;
    size_t pos = 0;

    while (true) {
        // Find start-of-header and end-of-text
        size_t soh = message.find(static_cast<char>(SOH), pos);
        if (soh == std::string::npos) break;

        size_t etx = message.find(static_cast<char>(ETX), soh + 1);
        if (etx == std::string::npos) break; // partial frame; wait for more

        // Push the complete framed packet [SOH ... ETX]
        packets.push_back(message.substr(soh, etx - soh + 1));

        // Continue after this frame
        pos = etx + 1;
    }
    return packets;
}

std::vector<std::string> extract_payload(const std::vector<std::string> &messages) {
    std::vector<std::string> payloads;
    for (const std::string &message : messages) {
        size_t STX_pos = message.find(STX);
        size_t ETX_pos = message.find(ETX);

        if (STX_pos == std::string::npos || ETX_pos == std::string::npos || STX_pos + 1 > ETX_pos) {
            continue; // malformed/partial; skip
        }

        payloads.push_back(message.substr(STX_pos + 1, ETX_pos - STX_pos - 1));
    }

    return payloads;
}
