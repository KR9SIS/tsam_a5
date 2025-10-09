#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <mutex>

// WARN: FIX THIS, IT IS FROM CHAT AND I DON'T THINK I LIKE IT

class Logger {
public:
    explicit Logger(const std::string& csv_path)
        : path_(csv_path)
    {
        std::filesystem::create_directories(std::filesystem::path(csv_path).parent_path());
        out_.open(csv_path, std::ios::app);
        if (!out_) throw std::runtime_error("Cannot open log file: " + csv_path);
        if (out_.tellp() == 0) {
            out_ << "ts,direction,peer_ip,peer_port,command,payload_len,payload_preview,note\n";
            out_.flush();
        }
    }

    // Log a command (works for both client and server)
    void logCommand(const std::string& direction,   // "SENT" or "RECV"
                    const std::string& peer_ip,
                    uint16_t peer_port,
                    const std::string& command,
                    const std::string& payload,
                    const std::string& note = "")
    {
        std::lock_guard<std::mutex> lk(m_);
        writeRow(isoUtc(), direction, peer_ip, peer_port, command,
                 payload.size(), preview(payload), note);
    }

    // log accept() with source IP/port
    void logAccept(const std::string& peer_ip, uint16_t peer_port,
                   const std::string& note = "accepted incoming connection")
    {
        std::lock_guard<std::mutex> lk(m_);
        writeRow(isoUtc(), "RECV", peer_ip, peer_port, "ACCEPT", 0, "", note);
    }

private:
    std::string path_;
    std::ofstream out_;
    std::mutex m_;

    static std::string isoUtc() {
        using clock = std::chrono::system_clock;
        auto now = clock::now();
        std::time_t t = clock::to_time_t(now);
        std::tm tm{};
    #if defined(_WIN32)
        gmtime_s(&tm, &t);
    #else
        gmtime_r(&t, &tm);
    #endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }

    static std::string csvq(std::string s) {
        for (char& c : s) if (c == '\n' || c == '\r') c = ' ';
        size_t pos = 0;
        while ((pos = s.find('"', pos)) != std::string::npos) { s.insert(pos, 1, '"'); pos += 2; }
        return "\"" + s + "\"";
    }

    static std::string preview(const std::string& s) {
        constexpr size_t N = 120;
        return s.size() <= N ? s : s.substr(0, N);
    }

    void writeRow(const std::string& ts,
                  const std::string& direction,
                  const std::string& peer_ip,
                  uint16_t peer_port,
                  const std::string& command,
                  size_t payload_len,
                  const std::string& payload_preview,
                  const std::string& note)
    {
        out_ << csvq(ts) << ','
             << csvq(direction) << ','
             << csvq(peer_ip) << ','
             << peer_port << ','
             << csvq(command) << ','
             << payload_len << ','
             << csvq(payload_preview) << ','
             << csvq(note) << '\n';
        out_.flush();
    }
};
