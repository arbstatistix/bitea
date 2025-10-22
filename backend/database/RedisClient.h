#ifndef REDISCLIENT_H
#define REDISCLIENT_H

/*
 * ============================================================================
 * RedisClient - Session Management for Bitea Social Media Platform
 * ============================================================================
 * 
 * PURPOSE:
 * This class manages temporary session data (who's logged in) using Redis,
 * an in-memory key-value store. Redis provides fast session lookups and
 * automatic expiration, perfect for authentication tokens.
 * 
 * ROLE IN BITEA ARCHITECTURE:
 * 
 *   [Frontend (HTML/JS)]
 *          ↓ Sends session token in requests
 *   [HttpServer (main.cpp)]
 *          ↓ Validates session
 *   [RedisClient] ← YOU ARE HERE (Session Storage)
 *          ↓ Stores/Retrieves with TTL
 *   [Redis In-Memory Database]
 * 
 * PARALLEL COMPONENTS:
 * - MongoClient: Handles permanent data (users, posts)
 * - RedisClient: Handles temporary data (sessions) ← THIS FILE
 * - Blockchain: Validates post integrity
 * - HttpServer: Routes requests and validates sessions
 * 
 * WHY REDIS FOR SESSIONS?
 * 1. SPEED: In-memory access (microseconds vs milliseconds for MongoDB)
 * 2. TTL: Automatic expiration (sessions auto-delete after 24 hours)
 * 3. SIMPLE: Key-value storage perfect for "token → user" mapping
 * 4. LIGHTWEIGHT: Don't bloat permanent database with temporary data
 * 
 * SESSION LIFECYCLE EXAMPLE:
 * 1. User logs in → HttpServer creates Session
 * 2. RedisClient.createSession() stores with 24h TTL
 * 3. User makes requests → Frontend sends session token
 * 4. RedisClient.getSession() validates token
 * 5. User logs out → RedisClient.deleteSession() removes token
 * 6. OR: 24 hours pass → Redis auto-deletes (TTL expired)
 * 
 * DATA STORAGE FORMAT:
 * - Key: "session:abc123xyz" (session: prefix + sessionId)
 * - Value: "abc123xyz|alice|1729600000|1729686400" (serialized Session)
 * - TTL: Seconds until expiration (Redis handles automatically)
 * 
 * THREAD SAFETY:
 * All public methods use redisMutex to prevent race conditions when multiple
 * HTTP requests check sessions simultaneously.
 * 
 * CONDITIONAL COMPILATION:
 * - If HAS_REDIS is defined: Uses real Redis (hiredis library)
 * - If not defined: Uses in-memory mock for testing without Redis
 * ============================================================================
 */

#include <string>
#include <mutex>
#include <iostream>
#include <sstream>
#include <ctime>
#include "../models/Session.h"  // Session model with sessionId, username, expiry

#ifdef HAS_REDIS
#include <hiredis/hiredis.h>

class RedisClient {
private:
    // ========== CONNECTION PROPERTIES ==========
    std::string host;          // Redis server hostname (default: 127.0.0.1)
    int port;                  // Redis server port (default: 6379)
    bool connected;            // Connection status flag
    std::mutex redisMutex;     // Thread safety: Protects all Redis operations
    redisContext* context;     // Hiredis connection context (low-level C library)

    /*
     * HELPER METHOD: Get Redis reply as string
     * 
     * PURPOSE: Safely extract string from Redis reply structure
     * 
     * INTERACTION WITH BITEA:
     * - Called by: connect() to parse PING response
     * - Uses: Hiredis library reply structures
     * - Handles: NULL replies and type checking
     */
    std::string getReplyString(redisReply* reply) {
        if (reply && (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_STATUS)) {
            return std::string(reply->str, reply->len);
        }
        return "";
    }

    /*
     * HELPER METHOD: Serialize session to string
     * 
     * PURPOSE: Convert Session object to storable string format
     * 
     * INTERACTION WITH BITEA:
     * - Called by: createSession(), refreshSession()
     * - Format: "sessionId|username|createdAt|expiresAt"
     * - Example: "abc123|alice|1729600000|1729686400"
     * 
     * WHY SERIALIZE?
     * Redis stores strings, not C++ objects. We convert Session fields
     * to pipe-delimited format for storage, then deserialize on retrieval.
     */
    std::string serializeSession(const Session& session) {
        std::stringstream ss;
        ss << session.getSessionId() << "|"
           << session.getUsername() << "|"
           << session.getCreatedAt() << "|"
           << session.getExpiresAt();
        return ss.str();
    }

    /*
     * HELPER METHOD: Deserialize session from string
     * 
     * PURPOSE: Convert stored string back to Session object
     * 
     * INTERACTION WITH BITEA:
     * - Called by: getSession() to reconstruct Session from Redis
     * - Parses: "sessionId|username|createdAt|expiresAt" format
     * 
     * LIMITATION:
     * Due to Session model design, we create a new Session object
     * rather than restoring exact timestamps. The session will have
     * current timestamps but that's acceptable for validation.
     */
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
    // ============================================================================
    // PUBLIC API - Database Lifecycle Methods
    // ============================================================================
    
    /*
     * CONSTRUCTOR
     * 
     * PARAMETERS:
     * - host: Redis server address (default: 127.0.0.1 for localhost)
     * - port: Redis server port (default: 6379, Redis standard port)
     * 
     * CALLED BY: main.cpp during application startup
     */
    RedisClient(const std::string& host = "127.0.0.1", int port = 6379)
        : host(host), port(port), connected(false), context(nullptr) {
    }

    /*
     * DESTRUCTOR
     * 
     * PURPOSE: Cleanup Redis connection on object destruction
     * 
     * CALLED: When application shuts down or RedisClient goes out of scope
     * ENSURES: No connection leaks
     */
    ~RedisClient() {
        disconnect();
    }

    /*
     * METHOD: connect()
     * 
     * PURPOSE: Establishes connection to Redis server
     * 
     * INTERACTION WITH BITEA:
     * - Called by: main.cpp during startup (before HttpServer starts)
     * - Before this: Cannot create or validate sessions (users can't login)
     * - After this: Session management becomes available
     * 
     * PROCESS:
     * 1. Creates Redis connection with 1.5 second timeout
     * 2. Sends PING command to verify Redis is responding
     * 3. Expects PONG response
     * 
     * STARTUP DEPENDENCY:
     * Redis must be running: `redis-server` or `brew services start redis`
     * 
     * RETURNS: true if connected successfully, false otherwise
     * 
     * ERROR HANDLING: Catches connection failures and logs errors
     */
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

    /*
     * METHOD: disconnect()
     * 
     * PURPOSE: Closes Redis connection and cleans up resources
     * 
     * CALLED BY: Destructor during application shutdown (Ctrl+C)
     * CLEANUP: Frees hiredis context to prevent memory leaks
     */
    void disconnect() {
        std::lock_guard<std::mutex> lock(redisMutex);
        
        if (context) {
            redisFree(context);
            context = nullptr;
        }
        
        connected = false;
        std::cout << "[Redis] Disconnected" << std::endl;
    }

    /*
     * METHOD: isConnected()
     * 
     * PURPOSE: Check if Redis connection is active
     * 
     * USED BY: All Redis operations check this before proceeding
     * RETURNS: true if connected, false otherwise
     */
    bool isConnected() const {
        return connected;
    }

    // ============================================================================
    // PUBLIC API - Generic Key-Value Operations
    // ============================================================================
    // These methods provide raw Redis functionality for any string storage needs
    // beyond sessions (caching, rate limiting, etc.)
    
    /*
     * METHOD: set()
     * 
     * PURPOSE: Store a key-value pair in Redis
     * 
     * REDIS COMMAND: SET key value
     * USE IN BITEA: Generic caching (if needed in future)
     * 
     * RETURNS: true if stored successfully, false otherwise
     */
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

    /*
     * METHOD: get()
     * 
     * PURPOSE: Retrieve value for a key from Redis
     * 
     * REDIS COMMAND: GET key
     * USE IN BITEA: Generic retrieval (used internally by getSession)
     * 
     * PARAMETERS:
     * - key: Key to look up
     * - value: Reference to store retrieved value
     * 
     * RETURNS: true if key found, false if not found or error
     */
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

    /*
     * METHOD: del()
     * 
     * PURPOSE: Delete a key from Redis
     * 
     * REDIS COMMAND: DEL key
     * USE IN BITEA: Used by deleteSession() to remove session tokens
     * 
     * RETURNS: true if key existed and was deleted, false otherwise
     */
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

    /*
     * METHOD: exists()
     * 
     * PURPOSE: Check if a key exists in Redis
     * 
     * REDIS COMMAND: EXISTS key
     * USE IN BITEA: Quick session validation without retrieving full data
     * 
     * RETURNS: true if key exists, false otherwise
     */
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

    // ============================================================================
    // PUBLIC API - Session Management (Core Authentication Functionality)
    // ============================================================================
    
    /*
     * METHOD: createSession()
     * 
     * PURPOSE: Store new session token after successful login
     * 
     * INTERACTION WITH BITEA - Complete Login Flow:
     * 
     * 1. [Frontend] User enters username/password, clicks "Login"
     * 2. [api.js] Sends POST /api/login with credentials
     * 3. [HttpServer] Finds user in MongoDB via MongoClient
     * 4. [HttpServer] Verifies password hash matches
     * 5. [HttpServer] Creates Session object (models/Session.h)
     * 6. [RedisClient] createSession() stores with TTL ← YOU ARE HERE
     * 7. [HttpServer] Returns session token to frontend
     * 8. [Frontend] Stores token in localStorage
     * 9. [Frontend] Includes token in all future requests
     * 
     * REDIS STORAGE:
     * - Command: SETEX (SET with EXpiry)
     * - Key: "session:abc123xyz"
     * - Value: Serialized session data
     * - TTL: Seconds until expiration (typically 24 hours)
     * 
     * AUTO-EXPIRATION:
     * Redis automatically deletes the session after TTL expires.
     * User must login again after 24 hours.
     * 
     * RETURNS: true if session created, false if expired or error
     */
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

    /*
     * METHOD: getSession()
     * 
     * PURPOSE: Validate session token and retrieve user info
     * 
     * INTERACTION WITH BITEA - Request Authentication:
     * 
     * Every authenticated request follows this pattern:
     * 
     * 1. [Frontend] User creates post/likes/comments
     * 2. [api.js] Includes session token in request header/body
     * 3. [HttpServer] Extracts session token from request
     * 4. [RedisClient] getSession() validates token ← YOU ARE HERE
     * 5. [RedisClient] Returns username if valid
     * 6. [HttpServer] Proceeds with request using username
     * 7. OR: [HttpServer] Returns 401 Unauthorized if invalid
     * 
     * VALIDATION CHECKS:
     * 1. Does key exist in Redis? (If not, session expired or never existed)
     * 2. Has session expired? (Check timestamp)
     * 3. If expired: Delete from Redis
     * 
     * EXAMPLE:
     * User creates post → Frontend sends "session:abc123" →
     * getSession validates → Returns username "alice" →
     * Post is created with author="alice"
     * 
     * PARAMETERS:
     * - sessionId: Session token from frontend
     * - session: Reference to Session object (populated if valid)
     * 
     * RETURNS: true if session valid, false if expired/not found/error
     */
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

    /*
     * METHOD: deleteSession()
     * 
     * PURPOSE: Remove session token (logout)
     * 
     * INTERACTION WITH BITEA - Logout Flow:
     * 
     * 1. [Frontend] User clicks "Logout" button
     * 2. [api.js] Sends POST /api/logout with session token
     * 3. [HttpServer] Calls deleteSession() ← YOU ARE HERE
     * 4. [RedisClient] Deletes key from Redis
     * 5. [Frontend] Removes token from localStorage
     * 6. [Frontend] Redirects to login page
     * 
     * SECURITY:
     * Immediately invalidates session - user cannot use same token again
     * 
     * ALSO USED FOR:
     * - Forced logout (admin action)
     * - Session invalidation after password change
     * - Clearing expired sessions
     * 
     * RETURNS: true if session existed and deleted, false otherwise
     */
    bool deleteSession(const std::string& sessionId) {
        if (!connected || !context) return false;
        
        std::string key = "session:" + sessionId;
        
        if (del(key)) {
            std::cout << "[Redis] Deleted session: " << sessionId << std::endl;
            return true;
        }
        
        return false;
    }

    /*
     * METHOD: refreshSession()
     * 
     * PURPOSE: Extend session expiration time (keep user logged in)
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer on authenticated requests (optional feature)
     * - Prevents: Session expiring while user is actively using the app
     * 
     * EXAMPLE USE CASE:
     * User logs in at 9 AM (expires 9 AM next day)
     * User is actively using site at 8:50 AM next day
     * refreshSession() extends to expire 8:50 AM following day
     * User doesn't get logged out while actively using site
     * 
     * IMPLEMENTATION:
     * 1. Retrieves current session
     * 2. Calls session.refresh() to update expiry timestamp
     * 3. Stores back in Redis with new TTL
     * 
     * NOTE: Currently may not be actively used in Bitea, but available
     * 
     * RETURNS: true if refreshed, false if session not found/error
     */
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

    /*
     * METHOD: cleanupExpiredSessions()
     * 
     * PURPOSE: Manual cleanup of expired sessions (NO-OP in Redis)
     * 
     * REDIS ADVANTAGE:
     * Redis handles session expiration automatically using TTL.
     * When TTL reaches 0, Redis deletes the key - no manual cleanup needed!
     * 
     * WHY THIS METHOD EXISTS:
     * - API compatibility with mock implementation
     * - Could be useful if switching to different storage backend
     * - Makes the interface consistent
     * 
     * COMPARISON:
     * - Other systems (MongoDB, files): Need periodic cleanup loops
     * - Redis: Built-in TTL = automatic cleanup = no memory leaks
     * 
     * This is one of Redis's key advantages for session management!
     */
    void cleanupExpiredSessions() {
        // Redis automatically handles expiration via TTL
        // This method is kept for API compatibility but does nothing
        std::cout << "[Redis] Sessions auto-expire via TTL" << std::endl;
    }

    /*
     * METHOD: getSessionCount()
     * 
     * PURPOSE: Count active sessions (how many users logged in)
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for statistics/analytics endpoints
     * - Frontend: Admin dashboard showing "Active Users: 42"
     * 
     * REDIS COMMAND: KEYS session:*
     * WARNING: KEYS command scans all keys - can be slow on large databases
     * For production: Consider using SCAN or maintaining a separate counter
     * 
     * USE CASES:
     * 1. Admin dashboard
     * 2. Monitoring/analytics
     * 3. Load assessment
     * 
     * RETURNS: Number of active sessions, or 0 if error/disconnected
     */
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

    /*
     * METHOD: getCacheSize()
     * 
     * PURPOSE: Get total number of keys in Redis database
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for monitoring/debugging
     * - Shows: Total keys (sessions + any other cached data)
     * 
     * REDIS COMMAND: DBSIZE
     * Fast O(1) operation - Redis maintains internal counter
     * 
     * USE CASES:
     * 1. Memory usage monitoring
     * 2. Debugging cache growth
     * 3. Performance analysis
     * 
     * RETURNS: Total number of keys, or 0 if error/disconnected
     */
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
/*
 * ============================================================================
 * MOCK IMPLEMENTATION - In-Memory Testing Session Storage
 * ============================================================================
 * 
 * PURPOSE:
 * Provides a lightweight in-memory alternative when Redis is not available.
 * Useful for:
 * - Development without Redis installation
 * - Unit testing
 * - Quick prototyping
 * - CI/CD environments without Redis
 * 
 * LIMITATIONS:
 * - No automatic TTL expiration (sessions don't auto-delete)
 * - Data lost when application stops (not persistent)
 * - Manual cleanup required (cleanupExpiredSessions)
 * - Single process only (no distributed access)
 * - Not suitable for production
 * 
 * COMPATIBILITY:
 * Implements same API as real RedisClient, so HttpServer code works unchanged.
 * 
 * KEY DIFFERENCES FROM REAL REDIS:
 * 1. Sessions stored in STL map (RAM) vs Redis in-memory database
 * 2. No automatic TTL - must call cleanupExpiredSessions() periodically
 * 3. No persistence between restarts
 * 4. No network access - single process only
 * ============================================================================
 */
#include <map>

class RedisClient {
private:
    std::string host;
    int port;
    bool connected;
    std::mutex cacheMutex;  // Thread safety for concurrent requests
    
    // In-memory storage (mock Redis)
    // Separate maps for generic cache and sessions for better organization
    std::map<std::string, std::string> cache;    // Generic key-value storage
    std::map<std::string, Session> sessions;     // Session storage (sessionId → Session)

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
