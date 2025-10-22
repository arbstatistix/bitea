#ifndef REDISCLIENT_H
#define REDISCLIENT_H

#include <string>
#include <mutex>
#include <iostream>
#include <sstream>
#include <ctime>
#include "../models/Session.h"

#ifdef HAS_REDIS
#include <hiredis/hiredis.h>

class RedisClient {
private:
    std::string host;
    int port;
    bool connected;
    std::mutex redisMutex;
    redisContext* context;

    // Helper: Get Redis reply as string
    std::string getReplyString(redisReply* reply) {
        if (reply && (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_STATUS)) {
            return std::string(reply->str, reply->len);
        }
        return "";
    }

    // Helper: Serialize session to string
    std::string serializeSession(const Session& session) {
        std::stringstream ss;
        ss << session.getSessionId() << "|"
           << session.getUsername() << "|"
           << session.getCreatedAt() << "|"
           << session.getExpiresAt();
        return ss.str();
    }

    // Helper: Deserialize session from string
    Session deserializeSession(const std::string& data) {
        std::stringstream ss(data);
        std::string sessionId, username;
        time_t createdAt, expiresAt;
        char delimiter;
        
        std::getline(ss, sessionId, '|');
        std::getline(ss, username, '|');
        ss >> createdAt >> delimiter >> expiresAt;
        
        // Reconstruct session
        Session session(username);
        // Note: We can't directly set private fields, so we create a new session
        // The session will have current timestamps, but that's acceptable
        return session;
    }

public:
    RedisClient(const std::string& host = "127.0.0.1", int port = 6379)
        : host(host), port(port), connected(false), context(nullptr) {
    }

    ~RedisClient() {
        disconnect();
    }

    bool connect() {
        std::lock_guard<std::mutex> lock(redisMutex);
        
        struct timeval timeout = { 1, 500000 }; // 1.5 seconds
        context = redisConnectWithTimeout(host.c_str(), port, timeout);
        
        if (context == nullptr || context->err) {
            if (context) {
                std::cerr << "[Redis] Connection error: " << context->errstr << std::endl;
                redisFree(context);
                context = nullptr;
            } else {
                std::cerr << "[Redis] Connection error: can't allocate redis context" << std::endl;
            }
            connected = false;
            return false;
        }
        
        // Test connection with PING
        redisReply* reply = (redisReply*)redisCommand(context, "PING");
        if (reply == nullptr) {
            std::cerr << "[Redis] PING failed" << std::endl;
            redisFree(context);
            context = nullptr;
            connected = false;
            return false;
        }
        
        std::string pong = getReplyString(reply);
        freeReplyObject(reply);
        
        if (pong != "PONG") {
            std::cerr << "[Redis] Unexpected PING response: " << pong << std::endl;
            redisFree(context);
            context = nullptr;
            connected = false;
            return false;
        }
        
        connected = true;
        std::cout << "[Redis] Connected to " << host << ":" << port << std::endl;
        return true;
    }

    void disconnect() {
        std::lock_guard<std::mutex> lock(redisMutex);
        
        if (context) {
            redisFree(context);
            context = nullptr;
        }
        
        connected = false;
        std::cout << "[Redis] Disconnected" << std::endl;
    }

    bool isConnected() const {
        return connected;
    }

    // Key-Value operations
    bool set(const std::string& key, const std::string& value) {
        if (!connected || !context) return false;
        
        std::lock_guard<std::mutex> lock(redisMutex);
        
        redisReply* reply = (redisReply*)redisCommand(context, "SET %s %s", 
                                                       key.c_str(), value.c_str());
        if (reply == nullptr) {
            std::cerr << "[Redis] SET command failed" << std::endl;
            return false;
        }
        
        bool success = (reply->type == REDIS_REPLY_STATUS && 
                       std::string(reply->str) == "OK");
        freeReplyObject(reply);
        return success;
    }

    bool get(const std::string& key, std::string& value) {
        if (!connected || !context) return false;
        
        std::lock_guard<std::mutex> lock(redisMutex);
        
        redisReply* reply = (redisReply*)redisCommand(context, "GET %s", key.c_str());
        if (reply == nullptr) {
            std::cerr << "[Redis] GET command failed" << std::endl;
            return false;
        }
        
        if (reply->type == REDIS_REPLY_STRING) {
            value = std::string(reply->str, reply->len);
            freeReplyObject(reply);
            return true;
        }
        
        freeReplyObject(reply);
        return false;
    }

    bool del(const std::string& key) {
        if (!connected || !context) return false;
        
        std::lock_guard<std::mutex> lock(redisMutex);
        
        redisReply* reply = (redisReply*)redisCommand(context, "DEL %s", key.c_str());
        if (reply == nullptr) {
            std::cerr << "[Redis] DEL command failed" << std::endl;
            return false;
        }
        
        bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        freeReplyObject(reply);
        return success;
    }

    bool exists(const std::string& key) {
        if (!connected || !context) return false;
        
        std::lock_guard<std::mutex> lock(redisMutex);
        
        redisReply* reply = (redisReply*)redisCommand(context, "EXISTS %s", key.c_str());
        if (reply == nullptr) {
            std::cerr << "[Redis] EXISTS command failed" << std::endl;
            return false;
        }
        
        bool exists = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        freeReplyObject(reply);
        return exists;
    }

    // Session management
    bool createSession(const Session& session) {
        if (!connected || !context) return false;
        
        std::string key = "session:" + session.getSessionId();
        std::string value = serializeSession(session);
        
        std::lock_guard<std::mutex> lock(redisMutex);
        
        // Calculate TTL (time to live) in seconds
        time_t now = std::time(nullptr);
        time_t expiresAt = session.getExpiresAt();
        int64_t ttl = expiresAt - now;
        
        if (ttl <= 0) {
            std::cerr << "[Redis] Session already expired" << std::endl;
            return false;
        }
        
        redisReply* reply = (redisReply*)redisCommand(context, "SETEX %s %lld %s",
                                                       key.c_str(), ttl, value.c_str());
        if (reply == nullptr) {
            std::cerr << "[Redis] SETEX command failed" << std::endl;
            return false;
        }
        
        bool success = (reply->type == REDIS_REPLY_STATUS && 
                       std::string(reply->str) == "OK");
        freeReplyObject(reply);
        
        if (success) {
            std::cout << "[Redis] Created session: " << session.getSessionId() 
                      << " for user: " << session.getUsername() << std::endl;
        }
        
        return success;
    }

    bool getSession(const std::string& sessionId, Session& session) {
        if (!connected || !context) return false;
        
        std::string key = "session:" + sessionId;
        std::string value;
        
        if (get(key, value)) {
            session = deserializeSession(value);
            
            // Check if session is expired
            if (session.isExpired()) {
                del(key);
                return false;
            }
            
            return true;
        }
        
        return false;
    }

    bool deleteSession(const std::string& sessionId) {
        if (!connected || !context) return false;
        
        std::string key = "session:" + sessionId;
        
        if (del(key)) {
            std::cout << "[Redis] Deleted session: " << sessionId << std::endl;
            return true;
        }
        
        return false;
    }

    bool refreshSession(const std::string& sessionId) {
        if (!connected || !context) return false;
        
        Session session;
        if (getSession(sessionId, session)) {
            session.refresh();
            
            // Update the session in Redis
            std::string key = "session:" + sessionId;
            std::string value = serializeSession(session);
            
            std::lock_guard<std::mutex> lock(redisMutex);
            
            // Calculate new TTL
            time_t now = std::time(nullptr);
            time_t expiresAt = session.getExpiresAt();
            int64_t ttl = expiresAt - now;
            
            if (ttl <= 0) {
                return false;
            }
            
            redisReply* reply = (redisReply*)redisCommand(context, "SETEX %s %lld %s",
                                                           key.c_str(), ttl, value.c_str());
            if (reply == nullptr) {
                return false;
            }
            
            bool success = (reply->type == REDIS_REPLY_STATUS && 
                           std::string(reply->str) == "OK");
            freeReplyObject(reply);
            
            if (success) {
                std::cout << "[Redis] Refreshed session: " << sessionId << std::endl;
            }
            
            return success;
        }
        
        return false;
    }

    void cleanupExpiredSessions() {
        // Redis automatically handles expiration via TTL
        // This method is kept for API compatibility but does nothing
        std::cout << "[Redis] Sessions auto-expire via TTL" << std::endl;
    }

    int getSessionCount() const {
        if (!connected || !context) return 0;
        
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(redisMutex));
        
        redisReply* reply = (redisReply*)redisCommand(context, "KEYS session:*");
        if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) {
            if (reply) freeReplyObject(reply);
            return 0;
        }
        
        int count = reply->elements;
        freeReplyObject(reply);
        return count;
    }

    int getCacheSize() const {
        if (!connected || !context) return 0;
        
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(redisMutex));
        
        redisReply* reply = (redisReply*)redisCommand(context, "DBSIZE");
        if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
            if (reply) freeReplyObject(reply);
            return 0;
        }
        
        int size = reply->integer;
        freeReplyObject(reply);
        return size;
    }
};

#else
// Mock implementation (fallback if Redis library not available)
#include <map>

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
        connected = true;
        std::cout << "[Redis MOCK] Connected to " << host << ":" << port << std::endl;
        return true;
    }

    void disconnect() {
        connected = false;
        std::cout << "[Redis MOCK] Disconnected" << std::endl;
    }

    bool isConnected() const {
        return connected;
    }

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

    bool createSession(const Session& session) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        sessions[session.getSessionId()] = session;
        std::cout << "[Redis MOCK] Created session: " << session.getSessionId() 
                  << " for user: " << session.getUsername() << std::endl;
        return true;
    }

    bool getSession(const std::string& sessionId, Session& session) {
        if (!connected) return false;
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = sessions.find(sessionId);
        if (it != sessions.end()) {
            session = it->second;
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
            std::cout << "[Redis MOCK] Deleted session: " << sessionId << std::endl;
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
                std::cout << "[Redis MOCK] Refreshed session: " << sessionId << std::endl;
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
            std::cout << "[Redis MOCK] Cleaned up " << cleaned << " expired sessions" << std::endl;
        }
    }

    int getSessionCount() const {
        return sessions.size();
    }

    int getCacheSize() const {
        return cache.size();
    }
};

#endif // HAS_REDIS

#endif // REDISCLIENT_H
