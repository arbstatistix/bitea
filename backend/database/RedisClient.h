#ifndef REDISCLIENT_H
#define REDISCLIENT_H

#include <string>
#include <map>
#include <mutex>
#include <iostream>
#include "../models/Session.h"

// Simple mock implementation for Redis
// In production, use hiredis library
class RedisClient {
private:
    std::string host;
    int port;
    bool connected;
    std::mutex cacheMutex;
    
    // In-memory storage (mock Redis)
    std::map<std::string, std::string> cache;
    std::map<std::string, Session> sessions;

public:
    RedisClient(const std::string& host = "127.0.0.1", int port = 6379)
        : host(host), port(port), connected(false) {
    }

    bool connect() {
        // In production, actually connect to Redis
        connected = true;
        std::cout << "[Redis] Connected to " << host << ":" << port << std::endl;
        return true;
    }

    void disconnect() {
        connected = false;
        std::cout << "[Redis] Disconnected" << std::endl;
    }

    bool isConnected() const {
        return connected;
    }

    // Key-Value operations
    bool set(const std::string& key, const std::string& value) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        cache[key] = value;
        return true;
    }

    bool get(const std::string& key, std::string& value) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    bool del(const std::string& key) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        return cache.erase(key) > 0;
    }

    bool exists(const std::string& key) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        return cache.find(key) != cache.end();
    }

    // Session management
    bool createSession(const Session& session) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        sessions[session.getSessionId()] = session;
        std::cout << "[Redis] Created session: " << session.getSessionId() 
                  << " for user: " << session.getUsername() << std::endl;
        return true;
    }

    bool getSession(const std::string& sessionId, Session& session) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = sessions.find(sessionId);
        if (it != sessions.end()) {
            session = it->second;
            // Check if session is still valid
            if (session.isExpired()) {
                sessions.erase(it);
                return false;
            }
            return true;
        }
        return false;
    }

    bool deleteSession(const std::string& sessionId) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto result = sessions.erase(sessionId) > 0;
        if (result) {
            std::cout << "[Redis] Deleted session: " << sessionId << std::endl;
        }
        return result;
    }

    bool refreshSession(const std::string& sessionId) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = sessions.find(sessionId);
        if (it != sessions.end()) {
            if (!it->second.isExpired()) {
                it->second.refresh();
                std::cout << "[Redis] Refreshed session: " << sessionId << std::endl;
                return true;
            } else {
                sessions.erase(it);
            }
        }
        return false;
    }

    void cleanupExpiredSessions() {
        if (!connected) return;
        std::lock_guard<std::mutex> lock(cacheMutex);
        
        auto it = sessions.begin();
        int cleaned = 0;
        while (it != sessions.end()) {
            if (it->second.isExpired()) {
                it = sessions.erase(it);
                cleaned++;
            } else {
                ++it;
            }
        }
        
        if (cleaned > 0) {
            std::cout << "[Redis] Cleaned up " << cleaned << " expired sessions" << std::endl;
        }
    }

    int getSessionCount() const {
        return sessions.size();
    }

    int getCacheSize() const {
        return cache.size();
    }
};

#endif // REDISCLIENT_H

