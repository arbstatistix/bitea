#ifndef SESSION_H
#define SESSION_H

#include <string>
#include <ctime>
#include <random>
#include <sstream>
#include <iomanip>

class Session {
private:
    std::string sessionId;
    std::string username;
    time_t createdAt;
    time_t expiresAt;
    int expirationSeconds;

    std::string generateSessionId() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        const char* hexChars = "0123456789abcdef";
        
        for (int i = 0; i < 32; i++) {
            ss << hexChars[dis(gen)];
        }
        
        return ss.str();
    }

public:
    Session() : expirationSeconds(86400) {  // 24 hours default
        sessionId = generateSessionId();
        createdAt = std::time(nullptr);
        expiresAt = createdAt + expirationSeconds;
    }

    Session(const std::string& username, int expirationSeconds = 86400)
        : username(username), expirationSeconds(expirationSeconds) {
        sessionId = generateSessionId();
        createdAt = std::time(nullptr);
        expiresAt = createdAt + expirationSeconds;
    }

    // Getters
    std::string getSessionId() const { return sessionId; }
    std::string getUsername() const { return username; }
    time_t getCreatedAt() const { return createdAt; }
    time_t getExpiresAt() const { return expiresAt; }

    bool isValid() const {
        return std::time(nullptr) < expiresAt;
    }

    bool isExpired() const {
        return !isValid();
    }

    void refresh() {
        expiresAt = std::time(nullptr) + expirationSeconds;
    }

    std::string toJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"sessionId\":\"" << sessionId << "\",";
        ss << "\"username\":\"" << username << "\",";
        ss << "\"createdAt\":" << createdAt << ",";
        ss << "\"expiresAt\":" << expiresAt << ",";
        ss << "\"valid\":" << (isValid() ? "true" : "false");
        ss << "}";
        return ss.str();
    }
};

#endif // SESSION_H

