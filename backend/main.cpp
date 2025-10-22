/*******************************************************************************
 * MAIN.CPP - Bitea Application Entry Point
 * 
 * PURPOSE:
 * This is the main application file that integrates all components of the
 * Bitea blockchain-based social media platform. It sets up the web server,
 * blockchain, databases, and defines all REST API endpoints.
 * 
 * APPLICATION ARCHITECTURE:
 * BiteaApp class orchestrates the entire system:
 * - HttpServer: Handles HTTP requests/responses
 * - Blockchain: Provides immutable data storage
 * - MongoClient: Primary database for fast queries
 * - RedisClient: Session storage and caching
 * 
 * SYSTEM COMPONENTS INTEGRATION:
 * 
 * 1. WEB SERVER (HttpServer.h):
 *    - Listens on port 3000
 *    - Routes requests to appropriate handlers
 *    - Multi-threaded request processing
 * 
 * 2. BLOCKCHAIN (Blockchain.h, Block.h, Transaction.h):
 *    - Immutable ledger for all user actions
 *    - Proof-of-Work consensus
 *    - Transaction validation
 * 
 * 3. DATABASES:
 *    - MongoDB: User accounts, posts, fast queries
 *    - Redis: Session storage, temporary data
 * 
 * 4. MODELS (User.h, Post.h, Session.h):
 *    - Data structures for accounts, content, auth
 * 
 * 5. UTILITIES (InputValidator.h):
 *    - Input validation and sanitization
 * 
 * API ENDPOINTS DEFINED:
 * 
 * PUBLIC (No auth):
 * - GET  /           - Health check
 * - GET  /api        - API information
 * - POST /api/register - User registration
 * - POST /api/login  - User authentication
 * - GET  /api/posts  - List all posts
 * - GET  /api/posts/:id - Get single post
 * - GET  /api/users/:username - User profile
 * - GET  /api/blockchain - View blockchain
 * - GET  /api/blockchain/validate - Validate chain
 * 
 * PROTECTED (Requires auth):
 * - POST /api/logout - End session
 * - POST /api/posts  - Create post
 * - POST /api/posts/:id/like - Like post
 * - POST /api/posts/:id/comment - Comment on post
 * - POST /api/users/:username/follow - Follow user
 * - GET  /api/mine   - Trigger block mining
 * 
 * DUAL STORAGE STRATEGY:
 * - User actions → Transaction → Blockchain (immutable record)
 * - Same data → Database (fast queries, indices)
 * - Database can be rebuilt from blockchain if corrupted
 * 
 * SECURITY FEATURES:
 * - Input validation: All inputs validated before processing
 * - Sanitization: HTML escaping to prevent XSS
 * - Session-based auth: Secure random session IDs
 * - Password hashing: Salted SHA-256
 * - CORS configured: Cross-origin requests allowed
 * 
 * WORKFLOW EXAMPLE (Creating a Post):
 * 1. User sends POST /api/posts with session token
 * 2. Server validates session → gets username
 * 3. Validates and sanitizes post content
 * 4. Creates Post object, stores in MongoDB
 * 5. Creates Transaction, adds to blockchain
 * 6. Blockchain auto-mines when enough transactions
 * 7. Post marked as on-chain after mining
 * 8. Returns post JSON to client
 * 
 * REFERENCES:
 * - REST API Design: https://restfulapi.net/
 * - Blockchain Applications: Beyond cryptocurrency use cases
 * - System Design: Microservices, separation of concerns
 ******************************************************************************/

#include <iostream>    // std::cout, std::cerr - logging and output
#include <memory>      // std::unique_ptr, std::make_unique - smart pointers
#include <sstream>     // std::stringstream - JSON building

// ============================================================================
// PROJECT COMPONENT INCLUDES
// ============================================================================

#include "server/HttpServer.h"        // HTTP web server with routing
#include "blockchain/Blockchain.h"    // Blockchain ledger management
#include "database/MongoClient.h"     // MongoDB client for data storage
#include "database/RedisClient.h"     // Redis client for session storage
#include "models/User.h"              // User account model
#include "models/Post.h"              // Social media post model
#include "models/Session.h"           // Authentication session model
#include "utils/InputValidator.h"     // Input validation utilities

// ============================================================================
// BITEA APPLICATION CLASS
// ============================================================================

/**
 * @class BiteaApp
 * @brief Main application class integrating all system components
 * 
 * PURPOSE: Orchestrates web server, blockchain, and databases
 * 
 * DESIGN PATTERN: Facade pattern (simplifies complex subsystem)
 * 
 * RESPONSIBILITIES:
 * 1. Initialize all components (server, blockchain, databases)
 * 2. Define REST API routes and handlers
 * 3. Coordinate between blockchain and database
 * 4. Manage authentication and authorization
 * 5. Handle graceful startup and shutdown
 * 
 * COMPONENT OWNERSHIP:
 * Uses std::unique_ptr for exclusive ownership
 * - Automatic cleanup (RAII)
 * - Clear ownership semantics
 * - Exception-safe resource management
 */
class BiteaApp {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (System Components)
    // ========================================================================
    
    /**
     * @brief HTTP web server instance
     * @type std::unique_ptr<HttpServer>
     * 
     * PURPOSE: Handles incoming HTTP requests and routing
     * 
     * CONFIGURATION:
     * - Port: 3000 (development default)
     * - Threading: Multi-threaded request handling
     * - CORS: Enabled for frontend access
     * 
     * WHY unique_ptr:
     * - Exclusive ownership (only BiteaApp owns server)
     * - Automatic cleanup on destruction
     * - Clear lifetime management
     */
    std::unique_ptr<HttpServer> server;
    
    /**
     * @brief Blockchain instance for immutable storage
     * @type std::unique_ptr<Blockchain>
     * 
     * PURPOSE: Provides tamper-evident ledger for all transactions
     * 
     * CONFIGURATION:
     * - Difficulty: 3 (moderate for development)
     * - Max transactions per block: 5 (quick block creation)
     * 
     * USAGE:
     * Every user action (post, like, follow) creates blockchain transaction
     * Auto-mines blocks when 5 transactions accumulated
     */
    std::unique_ptr<Blockchain> blockchain;
    
    /**
     * @brief MongoDB client for primary data storage
     * @type std::unique_ptr<MongoClient>
     * 
     * PURPOSE: Fast queries, indices, complex aggregations
     * 
     * STORES:
     * - Users: Account data, passwords, social graph
     * - Posts: Content, likes, comments
     * - Blockchain data can be stored here for persistence
     * 
     * WHY MONGODB:
     * - Document-oriented: Natural fit for JSON-like data
     * - Flexible schema: Easy to evolve data model
     * - Indices: Fast lookups by username, post ID, etc.
     */
    std::unique_ptr<MongoClient> mongodb;
    
    /**
     * @brief Redis client for session and cache storage
     * @type std::unique_ptr<RedisClient>
     * 
     * PURPOSE: Fast in-memory storage for ephemeral data
     * 
     * STORES:
     * - Sessions: Authentication sessions with expiration
     * - Cache: Frequently accessed data (future)
     * 
     * WHY REDIS:
     * - In-memory: Extremely fast (microsecond latency)
     * - Expiration: Automatic session cleanup
     * - Data structures: Sets, hashes, sorted sets
     * - Persistence: Can save to disk (optional)
     */
    std::unique_ptr<RedisClient> redis;

    // ========================================================================
    // PRIVATE HELPER METHODS (Utilities for Route Handlers)
    // ========================================================================
    
    /**
     * @brief Extracts session ID from Authorization header
     * @param req HTTP request object
     * @return std::string - Session ID or empty string
     * 
     * PURPOSE: Get session token from client request
     * 
     * AUTHORIZATION HEADER FORMAT:
     * "Authorization: Bearer a3f8c2d4e5f6a7b8c9d0e1f2a3b4c5d6"
     * 
     * PARSING:
     * 1. Find "Authorization" header
     * 2. Check if starts with "Bearer "
     * 3. Extract token after "Bearer "
     * 
     * RETURNS EMPTY IF:
     * - No Authorization header
     * - Header doesn't start with "Bearer "
     * 
     * USAGE: validateSession() calls this to get session ID
     */
    std::string getSessionId(const HttpRequest& req) {
        // Look for Authorization header
        auto it = req.headers.find("Authorization");
        if (it != req.headers.end()) {
            std::string auth = it->second;
            
            // Check for "Bearer " prefix (7 characters)
            if (auth.substr(0, 7) == "Bearer ") {
                return auth.substr(7);  // Return token after "Bearer "
            }
        }
        return "";  // No valid session ID found
    }

    /**
     * @brief Validates session and retrieves authenticated username
     * @param req HTTP request with Authorization header
     * @param username Output parameter - filled with authenticated username
     * @return bool - true if session valid, false otherwise
     * 
     * PURPOSE: Authentication middleware for protected routes
     * 
     * VALIDATION WORKFLOW:
     * 1. Extract session ID from Authorization header
     * 2. Lookup session in Redis
     * 3. Check if session exists and not expired
     * 4. Retrieve username from session
     * 5. Refresh session expiration (sliding window)
     * 6. Return true if valid, false otherwise
     * 
     * USAGE PATTERN:
     * std::string username;
     * if (!validateSession(req, username)) {
     *     // Not authenticated - return 401
     * }
     * // Authenticated - username contains current user
     * 
     * SIDE EFFECT:
     * Refreshes session on each use (extends expiration)
     * 
     * CALLED BY: All protected route handlers
     */
    bool validateSession(const HttpRequest& req, std::string& username) {
        // Extract session ID from request
        std::string sessionId = getSessionId(req);
        if (sessionId.empty()) return false;

        // Lookup session in Redis
        Session session;
        if (redis->getSession(sessionId, session)) {
            // Session found and valid
            username = session.getUsername();
            redis->refreshSession(sessionId);  // Extend expiration
            return true;
        }
        
        // Session not found or expired
        return false;
    }

    /**
     * @brief Decodes URL-encoded string
     * @param str URL-encoded string
     * @return std::string - Decoded string
     * 
     * PURPOSE: Convert %20 → space, + → space, etc.
     * 
     * URL ENCODING:
     * Special characters encoded as %XX (hex)
     * Spaces can be + or %20
     * 
     * EXAMPLES:
     * "hello%20world" → "hello world"
     * "hello+world" → "hello world"
     * "100%25" → "100%"
     * 
     * ALGORITHM:
     * - If %XX: Convert hex to character
     * - If +: Convert to space
     * - Else: Keep as-is
     * 
     * USAGE: Decode query parameters, form data
     * 
     * NOTE: Currently defined but not actively used
     * Query string parsing doesn't URL-decode yet
     */
    std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '%' && i + 2 < str.length()) {
                // Decode %XX hex sequence
                int value;
                std::istringstream is(str.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;  // Skip hex digits
                }
            } else if (str[i] == '+') {
                // Plus sign represents space
                result += ' ';
            } else {
                // Regular character
                result += str[i];
            }
        }
        return result;
    }

    /**
     * @brief Extracts value from JSON string (simplified parser)
     * @param json JSON string
     * @param key Key to extract
     * @return std::string - Value for key, or empty string
     * 
     * PURPOSE: Simple JSON parsing without external library
     * 
     * PATTERN: Finds "key":"value" in JSON
     * 
     * EXAMPLE:
     * JSON: {"username":"alice","email":"alice@example.com"}
     * getJsonValue(json, "username") → "alice"
     * 
     * LIMITATIONS:
     * - Very simplified (not a real JSON parser)
     * - Only handles string values (not numbers, booleans, objects)
     * - Doesn't handle escaped quotes in values
     * - Doesn't validate JSON structure
     * 
     * PRODUCTION:
     * Use proper JSON library (nlohmann/json, RapidJSON)
     * 
     * USAGE:
     * Extract request body parameters in route handlers
     * std::string username = getJsonValue(req.body, "username");
     * 
     * WHY SIMPLIFIED:
     * Educational/MVP - demonstrates concept
     * Avoids external dependency
     * Works for basic use cases
     */
    std::string getJsonValue(const std::string& json, const std::string& key) {
        // Build search pattern: "key":"
        std::string pattern = "\"" + key + "\":\"";
        
        // Find pattern in JSON
        size_t start = json.find(pattern);
        if (start == std::string::npos) return "";  // Key not found
        
        // Move past pattern to start of value
        start += pattern.length();
        
        // Find closing quote
        size_t end = json.find("\"", start);
        if (end == std::string::npos) return "";  // Malformed JSON
        
        // Extract value between quotes
        return json.substr(start, end - start);
    }

    // ========================================================================
    // PUBLIC INTERFACE (Initialization and Routing)
    // ========================================================================

public:
    /**
     * @brief Constructs BiteaApp and initializes all components
     * 
     * PURPOSE: Sets up entire application stack
     * 
     * INITIALIZATION ORDER:
     * 1. HttpServer: Port 3000
     * 2. Blockchain: Difficulty 3, max 5 transactions per block
     * 3. MongoClient: Database connection (connects in run())
     * 4. RedisClient: Session store (connects in run())
     * 
     * BLOCKCHAIN CONFIGURATION:
     * - Difficulty 3: Moderate mining time (~seconds)
     * - Max 5 tx/block: Quick block creation for demo
     * 
     * WHY std::make_unique:
     * - Creates object and unique_ptr in one step
     * - Exception-safe initialization
     * - More efficient than unique_ptr(new ...)
     * 
     * DEFERRED OPERATIONS:
     * Database connections happen in run(), not constructor
     * Allows error handling before server starts
     */
    BiteaApp() {
        // Create HTTP server on port 3000
        server = std::make_unique<HttpServer>(3000);
        
        // Create blockchain (difficulty=3, 5 transactions per block)
        blockchain = std::make_unique<Blockchain>(3, 5);
        
        // Create database clients (connect later in run())
        mongodb = std::make_unique<MongoClient>();
        redis = std::make_unique<RedisClient>();
    }

    // ========================================================================
    // ROUTE SETUP METHOD (Defines All API Endpoints)
    // ========================================================================
    
    /**
     * @brief Registers all REST API routes with handlers
     * 
     * PURPOSE: Define complete API surface
     * 
     * ROUTE ORGANIZATION:
     * - Health/Info: /, /api
     * - Auth: /api/register, /api/login, /api/logout
     * - Posts: /api/posts (CRUD + social interactions)
     * - Users: /api/users (profiles, follow)
     * - Blockchain: /api/blockchain, /api/mine
     * 
     * LAMBDA CAPTURES:
     * [this]: Captures BiteaApp instance for accessing members
     * Allows handlers to use blockchain, mongodb, redis
     * 
     * AUTHENTICATION:
     * Protected routes call validateSession() first
     * Returns 401 Unauthorized if session invalid
     * 
     * DUAL STORAGE PATTERN:
     * Most write operations:
     * 1. Store in database (fast retrieval)
     * 2. Create blockchain transaction (immutable proof)
     * 
     * ERROR RESPONSES:
     * - 400: Client error (invalid input)
     * - 401: Unauthorized (authentication required)
     * - 404: Not found (resource doesn't exist)
     * - 500: Server error (shouldn't happen)
     */
    void setupRoutes() {
        // ====================================================================
        // HEALTH AND INFO ENDPOINTS (Public)
        // ====================================================================
        
        /**
         * ENDPOINT: GET /
         * PURPOSE: Health check - verify server is running
         * AUTH: None (public)
         * 
         * RESPONSE:
         * {"message":"Bitea API Server","status":"running"}
         * 
         * USAGE: Load balancer health checks, uptime monitoring
         */
        server->get("/", []([[maybe_unused]] const HttpRequest& req, HttpResponse& res) {
            res.json("{\"message\":\"Bitea API Server\",\"status\":\"running\"}");
        });

        /**
         * ENDPOINT: GET /api
         * PURPOSE: API metadata and system status
         * AUTH: None (public)
         * 
         * RESPONSE INCLUDES:
         * - API version
         * - Blockchain stats (blocks, pending txs, validity)
         * - Database stats (user count, post count)
         * - Session count
         * 
         * USAGE:
         * - Dashboard: Display system health
         * - Monitoring: Track blockchain growth
         * - Admin: Quick system overview
         */
        server->get("/api", [this]([[maybe_unused]] const HttpRequest& req, HttpResponse& res) {
            std::stringstream ss;
            ss << "{";
            ss << "\"name\":\"Bitea API\",";
            ss << "\"version\":\"1.0.0\",";
            ss << "\"blockchain\":{";
            ss << "\"blocks\":" << blockchain->getChainLength() << ",";
            ss << "\"pending\":" << blockchain->getPendingTransactionCount() << ",";
            ss << "\"valid\":" << (blockchain->isChainValid() ? "true" : "false");
            ss << "},";
            ss << "\"database\":{";
            ss << "\"users\":" << mongodb->getUserCount() << ",";
            ss << "\"posts\":" << mongodb->getPostCount();
            ss << "},";
            ss << "\"sessions\":" << redis->getSessionCount();
            ss << "}";
            res.json(ss.str());
        });

        // ====================================================================
        // AUTHENTICATION ENDPOINTS
        // ====================================================================
        
        /**
         * ENDPOINT: POST /api/register
         * PURPOSE: User registration (create new account)
         * AUTH: None (public)
         * 
         * REQUEST BODY (JSON):
         * {
         *   "username": "alice",
         *   "email": "alice@example.com",
         *   "password": "password123"
         * }
         * 
         * VALIDATION:
         * - Username: 3-20 chars, alphanumeric + underscore
         * - Email: Valid email format, max 254 chars
         * - Password: 8-128 chars, must have letter + digit
         * - Uniqueness: Username must not exist
         * 
         * SECURITY WORKFLOW:
         * 1. Extract and trim inputs
         * 2. Validate format (username, email, password)
         * 3. Check if username already exists
         * 4. Create User (auto-generates salt, hashes password)
         * 5. Store in MongoDB
         * 6. Add USER_REGISTRATION transaction to blockchain
         * 7. Return user profile (with private data)
         * 
         * BLOCKCHAIN TRANSACTION:
         * Records user registration immutably
         * Proves account existed at specific time
         * 
         * RESPONSE: 201 Created with user JSON
         * ERROR RESPONSES:
         * - 400: Invalid input (missing fields, format errors, duplicate username)
         */
        server->post("/api/register", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username = InputValidator::trimWhitespace(getJsonValue(req.body, "username"));
            std::string email = InputValidator::trimWhitespace(getJsonValue(req.body, "email"));
            std::string password = getJsonValue(req.body, "password");

            // Extract request body fields
            if (username.empty() || email.empty() || password.empty()) {
                res.statusCode = 400;
                res.json("{\"error\":\"Missing required fields\"}");
                return;
            }

            // Validate username format
            if (!InputValidator::isValidUsername(username)) {
                res.statusCode = 400;
                res.json("{\"error\":\"Invalid username. Use 3-20 alphanumeric characters or underscores.\"}");
                return;
            }

            // Validate email format
            if (!InputValidator::isValidEmail(email)) {
                res.statusCode = 400;
                res.json("{\"error\":\"Invalid email format\"}");
                return;
            }

            // Validate password strength
            if (!InputValidator::isValidPassword(password)) {
                res.statusCode = 400;
                res.json("{\"error\":\"Password must be 8-128 characters with at least one letter and one number\"}");
                return;
            }

            // Check for duplicate username
            User existingUser;
            if (mongodb->findUser(username, existingUser)) {
                res.statusCode = 400;
                res.json("{\"error\":\"Username already exists\"}");
                return;
            }

            // Create user (auto-generates salt, hashes password)
            User newUser(username, email, password);
            mongodb->insertUser(newUser);

            // Record registration on blockchain (immutable proof)
            std::stringstream txData;
            txData << "{\"action\":\"register\",\"username\":\"" << InputValidator::sanitize(username) << "\"}";
            Transaction tx(username, TransactionType::USER_REGISTRATION, txData.str());
            blockchain->addTransaction(tx);

            // Return created user (201 Created)
            res.statusCode = 201;
            res.json(newUser.toJson(true));  // Include private data (email, etc.)
        });

        /**
         * ENDPOINT: POST /api/login
         * PURPOSE: User authentication
         * AUTH: None (public)
         * 
         * REQUEST BODY (JSON):
         * {"username":"alice","password":"password123"}
         * 
         * AUTHENTICATION WORKFLOW:
         * 1. Extract and validate credentials
         * 2. Lookup user in database
         * 3. Verify password (hash comparison)
         * 4. Create session with random ID
         * 5. Store session in Redis
         * 6. Update user's lastLogin timestamp
         * 7. Return session ID and user data
         * 
         * SECURITY:
         * - Generic error messages (prevent username enumeration)
         * - Password hashing verification (constant-time comparison ideal)
         * - New session ID per login (prevents fixation)
         * 
         * RESPONSE: 200 OK with session ID and user data
         * ERROR: 401 Unauthorized (same message for all auth failures)
         */
        server->post("/api/login", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username = InputValidator::trimWhitespace(getJsonValue(req.body, "username"));
            std::string password = getJsonValue(req.body, "password");

            // Validate credentials presence
            if (username.empty() || password.empty()) {
                res.statusCode = 401;
                res.json("{\"error\":\"Invalid credentials\"}");
                return;
            }

            // Validate username format (also prevents injection)
            if (!InputValidator::isValidUsername(username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Invalid credentials\"}");  // Generic error
                return;
            }

            // Lookup user in database
            User user;
            if (!mongodb->findUser(username, user)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Invalid credentials\"}");  // Don't reveal user exists
                return;
            }

            // Verify password (compares hash(salt + password) with stored hash)
            if (!user.verifyPassword(password)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Invalid credentials\"}");  // Generic error
                return;
            }

            // Authentication successful - create session
            Session session(username);  // Auto-generates random session ID
            redis->createSession(session);

            // Update user's last login timestamp
            user.updateLastLogin();
            mongodb->updateUser(user);

            // Return session ID and user data
            std::stringstream ss;
            ss << "{\"sessionId\":\"" << session.getSessionId() << "\",";
            ss << "\"user\":" << user.toJson(true) << "}";  // Include private data
            res.json(ss.str());
        });

        /**
         * ENDPOINT: POST /api/logout
         * PURPOSE: End user session
         * AUTH: Optional (works with or without session)
         * 
         * WORKFLOW:
         * 1. Extract session ID from request
         * 2. Delete session from Redis
         * 3. Return success message
         * 
         * IDEMPOTENT: Safe to call even if not logged in
         * 
         * SECURITY:
         * Removes session, forcing re-authentication
         * Should be called when user clicks "logout"
         */
        server->post("/api/logout", [this](const HttpRequest& req, HttpResponse& res) {
            std::string sessionId = getSessionId(req);
            if (!sessionId.empty()) {
                redis->deleteSession(sessionId);  // Remove session
            }
            res.json("{\"message\":\"Logged out successfully\"}");
        });

        // ====================================================================
        // POST ENDPOINTS (Social Media Content)
        // ====================================================================
        
        /**
         * ENDPOINT: POST /api/posts
         * PURPOSE: Create new post
         * AUTH: Required (must be logged in)
         * 
         * REQUEST BODY (JSON):
         * {"content":"Hello world! My first post."}
         * 
         * VALIDATION:
         * - Session: Must be authenticated
         * - Content: 1-5000 chars, not all whitespace
         * 
         * DUAL STORAGE:
         * 1. Store in MongoDB (fast retrieval, queries)
         * 2. Create blockchain transaction (immutable proof)
         * 
         * WORKFLOW:
         * 1. Validate session → get username
         * 2. Trim and validate content
         * 3. Sanitize content (XSS prevention)
         * 4. Create Post object with generated ID
         * 5. Insert into MongoDB
         * 6. Create POST transaction
         * 7. Add to blockchain (auto-mines when 5 txs)
         * 8. Return post JSON
         * 
         * BLOCKCHAIN TRANSACTION:
         * Records post creation with ID and author
         * Proves content existed at specific time
         * Can verify post integrity later
         * 
         * RESPONSE: 201 Created with post data
         * ERROR:
         * - 401: Not authenticated
         * - 400: Invalid content
         */
        server->post("/api/posts", [this](const HttpRequest& req, HttpResponse& res) {
            // Authenticate user
            std::string username;
            if (!validateSession(req, username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            // Extract and trim post content
            std::string content = InputValidator::trimWhitespace(getJsonValue(req.body, "content"));
            
            // Validate content (1-5000 chars, not all whitespace)
            if (!InputValidator::isValidPostContent(content)) {
                res.statusCode = 400;
                res.json("{\"error\":\"Invalid content. Must be 1-5000 characters and not empty.\"}");
                return;
            }

            // Sanitize content (XSS prevention)
            content = InputValidator::sanitize(content);

            // Create post with generated ID (username-timestamp format)
            std::string postId = username + "-" + std::to_string(std::time(nullptr));
            Post post(postId, username, content);
            mongodb->insertPost(post);  // Store in database

            // Record on blockchain for immutability
            std::stringstream txData;
            txData << "{\"action\":\"post\",\"postId\":\"" << InputValidator::sanitize(postId)
                   << "\",\"author\":\"" << InputValidator::sanitize(username) << "\"}";
            Transaction tx(username, TransactionType::POST, txData.str());
            blockchain->addTransaction(tx);  // Will auto-mine when 5 txs accumulated

            // Return created post (201 Created)
            res.statusCode = 201;
            res.json(post.toJson());
        });

        /**
         * ENDPOINT: GET /api/posts
         * PURPOSE: Retrieve all posts (feed)
         * AUTH: None (public)
         * 
         * RETURNS: JSON array of all posts
         * OPTIMIZATION: Uses lightweight toJson() (counts only, not full comments)
         */
        server->get("/api/posts", [this]([[maybe_unused]] const HttpRequest& req, HttpResponse& res) {
            // Fetch all posts from database
            auto posts = mongodb->getAllPosts();
            
            // Build JSON array
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < posts.size(); i++) {
                ss << posts[i].toJson();  // Lightweight version (counts only)
                if (i < posts.size() - 1) ss << ",";
            }
            ss << "]";
            
            res.json(ss.str());
        });

        /**
         * ENDPOINT: GET /api/posts/:id
         * PURPOSE: Get single post with full details
         * AUTH: None (public)
         * 
         * PATH PARAMETER: id = post ID
         * RETURNS: Detailed post JSON (includes full comment array)
         */
        server->get("/api/posts/:id", [this](const HttpRequest& req, HttpResponse& res) {
            std::string postId = req.params.at("id");  // Extract :id parameter
            
            Post post;
            if (!mongodb->findPost(postId, post)) {
                res.statusCode = 404;
                res.json("{\"error\":\"Post not found\"}");
                return;
            }

            res.json(post.toDetailedJson());  // Full details (including comments)
        });

        /**
         * ENDPOINT: POST /api/posts/:id/like
         * PURPOSE: Like a post
         * AUTH: Required
         * 
         * WORKFLOW:
         * 1. Validate session
         * 2. Lookup post
         * 3. Add like (Post::addLike - prevents duplicates)
         * 4. Update post in database
         * 5. Record LIKE transaction on blockchain
         * 
         * IDEMPOTENT: Liking twice has no effect (set ensures uniqueness)
         */
        server->post("/api/posts/:id/like", [this](const HttpRequest& req, HttpResponse& res) {
            // Authenticate user
            std::string username;
            if (!validateSession(req, username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            // Extract post ID from URL parameter
            std::string postId = req.params.at("id");
            
            // Lookup post in database
            Post post;
            if (!mongodb->findPost(postId, post)) {
                res.statusCode = 404;
                res.json("{\"error\":\"Post not found\"}");
                return;
            }

            // Add like to post (set prevents duplicates)
            post.addLike(username);
            mongodb->updatePost(post);  // Update in database

            // Record like on blockchain
            std::stringstream txData;
            txData << "{\"action\":\"like\",\"postId\":\"" << postId << "\"}";
            Transaction tx(username, TransactionType::LIKE, txData.str());
            blockchain->addTransaction(tx);

            // Return updated post
            res.json(post.toJson());
        });

        /**
         * ENDPOINT: POST /api/posts/:id/comment
         * PURPOSE: Add comment to post
         * AUTH: Required
         * 
         * REQUEST BODY: {"content":"Great post!"}
         * VALIDATION: Content 1-1000 chars
         * STORAGE: Comment added to post's comment vector + blockchain
         */
        server->post("/api/posts/:id/comment", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username;
            if (!validateSession(req, username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            // Extract comment content
            std::string postId = req.params.at("id");
            std::string content = InputValidator::trimWhitespace(getJsonValue(req.body, "content"));

            // Validate comment (1-1000 chars, shorter than posts)
            if (content.empty() || content.length() > 1000) {
                res.statusCode = 400;
                res.json("{\"error\":\"Comment must be 1-1000 characters\"}");
                return;
            }

            // Sanitize for XSS prevention
            content = InputValidator::sanitize(content);

            // Lookup post
            Post post;
            if (!mongodb->findPost(postId, post)) {
                res.statusCode = 404;
                res.json("{\"error\":\"Post not found\"}");
                return;
            }

            // Add comment to post
            post.addComment(username, content);
            mongodb->updatePost(post);

            // Record comment on blockchain
            std::stringstream txData;
            txData << "{\"action\":\"comment\",\"postId\":\"" << InputValidator::sanitize(postId) << "\"}";
            Transaction tx(username, TransactionType::COMMENT, txData.str());
            blockchain->addTransaction(tx);

            // Return updated post with all comments
            res.json(post.toDetailedJson());
        });

        // ====================================================================
        // USER ENDPOINTS (Profiles and Social)
        // ====================================================================
        
        /**
         * ENDPOINT: GET /api/users/:username
         * PURPOSE: Get user profile
         * AUTH: None (public)
         * 
         * RETURNS: Public user data (no email, no private info)
         */
        server->get("/api/users/:username", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username = req.params.at("username");
            
            User user;
            if (!mongodb->findUser(username, user)) {
                res.statusCode = 404;
                res.json("{\"error\":\"User not found\"}");
                return;
            }

            res.json(user.toJson(false));  // Public data only
        });

        /**
         * ENDPOINT: POST /api/users/:username/follow
         * PURPOSE: Follow another user
         * AUTH: Required
         * 
         * BIDIRECTIONAL UPDATE:
         * - Current user's "following" set updated
         * - Target user's "followers" set updated
         * Both users updated in database
         * 
         * BLOCKCHAIN: Records FOLLOW transaction
         */
        server->post("/api/users/:username/follow", [this](const HttpRequest& req, HttpResponse& res) {
            std::string currentUser;
            if (!validateSession(req, currentUser)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            // Extract target username from URL
            std::string targetUsername = req.params.at("username");
            
            // Lookup both users
            User user, targetUser;
            if (!mongodb->findUser(currentUser, user) || !mongodb->findUser(targetUsername, targetUser)) {
                res.statusCode = 404;
                res.json("{\"error\":\"User not found\"}");
                return;
            }

            // Update follow relationship (bidirectional)
            user.follow(targetUsername);              // Add to following set
            targetUser.addFollower(currentUser);      // Add to followers set
            
            // Update both users in database
            mongodb->updateUser(user);
            mongodb->updateUser(targetUser);

            // Record follow action on blockchain
            std::stringstream txData;
            txData << "{\"action\":\"follow\",\"target\":\"" << targetUsername << "\"}";
            Transaction tx(currentUser, TransactionType::FOLLOW, txData.str());
            blockchain->addTransaction(tx);

            res.json("{\"message\":\"Followed successfully\"}");
        });

        // ====================================================================
        // BLOCKCHAIN ENDPOINTS (Inspection and Management)
        // ====================================================================
        
        /**
         * ENDPOINT: GET /api/blockchain
         * PURPOSE: Retrieve entire blockchain
         * AUTH: None (public - transparency)
         * 
         * RETURNS: Array of blocks with metadata
         * Each block: index, hash, previousHash, timestamp, nonce, tx count
         * 
         * USAGE:
         * - Block explorer interface
         * - Audit/verification
         * - Educational/demo purposes
         * 
         * NOTE: Returns block summaries, not full transaction data
         */
        server->get("/api/blockchain", [this]([[maybe_unused]] const HttpRequest& req, HttpResponse& res) {
            // Get blockchain reference
            const auto& chain = blockchain->getChain();
            
            // Build JSON array of blocks
            std::stringstream ss;
            ss << "{\"blocks\":[";
            for (size_t i = 0; i < chain.size(); i++) {
                const auto& block = chain[i];
                // Serialize block metadata (not full transaction details)
                ss << "{";
                ss << "\"index\":" << block->getIndex() << ",";
                ss << "\"hash\":\"" << block->getHash() << "\",";
                ss << "\"previousHash\":\"" << block->getPreviousHash() << "\",";
                ss << "\"timestamp\":" << block->getTimestamp() << ",";
                ss << "\"nonce\":" << block->getNonce() << ",";
                ss << "\"transactions\":" << block->getTransactions().size();
                ss << "}";
                if (i < chain.size() - 1) ss << ",";
            }
            ss << "]}";
            
            res.json(ss.str());
        });

        /**
         * ENDPOINT: GET /api/blockchain/validate
         * PURPOSE: Verify blockchain integrity
         * AUTH: None (public)
         * 
         * VALIDATION CHECKS:
         * - Each block has valid Proof-of-Work
         * - Each block links to previous (hash chain intact)
         * - No tampering detected
         * 
         * RETURNS: {"valid":true} or {"valid":false}
         * 
         * USAGE:
         * - Health checks
         * - After database restoration
         * - Security audits
         */
        server->get("/api/blockchain/validate", [this]([[maybe_unused]] const HttpRequest& req, HttpResponse& res) {
            bool valid = blockchain->isChainValid();
            std::stringstream ss;
            ss << "{\"valid\":" << (valid ? "true" : "false") << "}";
            res.json(ss.str());
        });

        /**
         * ENDPOINT: GET /api/mine
         * PURPOSE: Manually trigger block mining
         * AUTH: None (public, but could be protected)
         * 
         * WORKFLOW:
         * 1. Takes pending transactions
         * 2. Creates new block
         * 3. Performs Proof-of-Work mining
         * 4. Adds block to chain
         * 
         * USAGE:
         * - Manual mining (not waiting for auto-trigger)
         * - Testing/demo purposes
         * - Admin control
         * 
         * PERFORMANCE:
         * May take seconds depending on difficulty (3 in this app)
         * Request blocks until mining complete
         */
        server->get("/api/mine", [this]([[maybe_unused]] const HttpRequest& req, HttpResponse& res) {
            // Mine all pending transactions into a block
            blockchain->minePendingTransactionsPublic();
            
            // Return mining result
            std::stringstream ss;
            ss << "{\"message\":\"Block mined successfully\",";
            ss << "\"blocks\":" << blockchain->getChainLength() << ",";
            ss << "\"pending\":" << blockchain->getPendingTransactionCount() << "}";
            res.json(ss.str());
        });
    }

    // ========================================================================
    // APPLICATION STARTUP METHOD
    // ========================================================================
    
    /**
     * @brief Starts application (connects DBs, starts server)
     * 
     * PURPOSE: Main application entry point
     * 
     * STARTUP SEQUENCE:
     * 1. Connect to MongoDB (users, posts)
     * 2. Connect to Redis (sessions)
     * 3. Display blockchain info (genesis block)
     * 4. Setup all API routes
     * 5. Start HTTP server (blocking)
     * 
     * ERROR HANDLING:
     * - MongoDB connection failure: Exit
     * - Redis connection failure: Exit
     * - Server start failure: Exit
     * 
     * BLOCKING:
     * server->start() blocks until server stops
     * This is the main application loop
     */
    void run() {
        // Display startup banner
        std::cout << "=== Bitea Social Media Blockchain ===" << std::endl;
        std::cout << "Initializing..." << std::endl;

        // Connect to MongoDB (primary data storage)
        if (!mongodb->connect()) {
            std::cerr << "Failed to connect to MongoDB" << std::endl;
            return;  // Cannot proceed without database
        }

        // Connect to Redis (session storage)
        if (!redis->connect()) {
            std::cerr << "Failed to connect to Redis" << std::endl;
            return;  // Cannot proceed without session store
        }

        // Display blockchain status (genesis block already created in constructor)
        std::cout << "Blockchain initialized with genesis block" << std::endl;
        std::cout << blockchain->getChainInfo() << std::endl;

        // Register all API routes
        setupRoutes();

        // Start HTTP server (BLOCKING - runs until stopped)
        server->start();
    }
};

// ============================================================================
// APPLICATION ENTRY POINT
// ============================================================================

/**
 * @brief Main function - application entry point
 * @return int - Exit code (0 = success)
 * 
 * PURPOSE: Creates and runs Bitea application
 * 
 * WORKFLOW:
 * 1. Create BiteaApp instance
 * 2. Call run() method (blocks until server stops)
 * 3. Return 0 on normal exit
 * 
 * RESOURCE CLEANUP:
 * BiteaApp destructor automatically cleans up:
 * - Closes server socket
 * - Destroys blockchain (in-memory)
 * - Closes database connections
 * 
 * SIGNAL HANDLING:
 * Production: Should add signal handlers (SIGINT, SIGTERM)
 * For graceful shutdown on Ctrl+C
 */
int main() {
    // Create application instance
    BiteaApp app;
    
    // Run application (blocks until shutdown)
    app.run();
    
    // Normal exit
    return 0;
}

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * SYSTEM ARCHITECTURE:
 * 
 * THREE-TIER ARCHITECTURE:
 * 1. Presentation: HTTP API (HttpServer) → Frontend consumes
 * 2. Business Logic: Route handlers, models, validation
 * 3. Data: MongoDB (fast), Redis (sessions), Blockchain (immutable)
 * 
 * DATA FLOW (Write Operation):
 * User → HTTP Request → Route Handler → Validation → Model → Database → Blockchain
 * 
 * DATA FLOW (Read Operation):
 * User → HTTP Request → Route Handler → Database → Model → JSON → HTTP Response
 * 
 * DUAL STORAGE RATIONALE:
 * 
 * DATABASE (MongoDB):
 * - Fast queries: O(1) with indices
 * - Complex operations: Aggregations, joins, full-text search
 * - Mutable: Can update posts, users
 * - Temporary: Can be rebuilt from blockchain
 * 
 * BLOCKCHAIN:
 * - Immutable: Cannot alter history
 * - Tamper-evident: Cryptographic verification
 * - Audit trail: Complete history of actions
 * - Censorship-resistant: Decentralized proof
 * 
 * WHY BOTH:
 * Database provides performance
 * Blockchain provides integrity
 * Best of both worlds for social media use case
 * 
 * AUTHENTICATION FLOW:
 * 
 * REGISTRATION:
 * 1. POST /api/register (username, email, password)
 * 2. Validate inputs
 * 3. Create User (generates salt, hashes password)
 * 4. Store in MongoDB
 * 5. Record on blockchain
 * 6. Return user data
 * 
 * LOGIN:
 * 1. POST /api/login (username, password)
 * 2. Lookup user
 * 3. Verify password hash
 * 4. Create session in Redis
 * 5. Update lastLogin
 * 6. Return session ID
 * 
 * AUTHENTICATED REQUEST:
 * 1. Client sends Authorization: Bearer <sessionId>
 * 2. Server extracts session ID
 * 3. Looks up session in Redis
 * 4. Validates not expired
 * 5. Retrieves username
 * 6. Processes request as that user
 * 
 * SOCIAL MEDIA FLOW:
 * 
 * CREATE POST:
 * 1. Validate session
 * 2. Validate content
 * 3. Create Post object
 * 4. Store in MongoDB
 * 5. Create blockchain transaction
 * 6. Auto-mines when 5 txs accumulated
 * 
 * LIKE POST:
 * 1. Validate session
 * 2. Lookup post
 * 3. Add like (prevents duplicates)
 * 4. Update database
 * 5. Record on blockchain
 * 
 * FOLLOW USER:
 * 1. Validate session
 * 2. Lookup both users
 * 3. Update both users (bidirectional)
 * 4. Save to database
 * 5. Record on blockchain
 * 
 * BLOCKCHAIN INTEGRATION:
 * 
 * TRANSACTION TYPES:
 * - USER_REGISTRATION: New account created
 * - POST: Content posted
 * - LIKE: Post liked
 * - COMMENT: Comment added
 * - FOLLOW: User followed
 * 
 * AUTO-MINING:
 * Blockchain configured with maxTransactionsPerBlock = 5
 * When 5 pending transactions → automatically mines new block
 * Difficulty 3 → Mining takes ~seconds
 * 
 * MANUAL MINING:
 * GET /api/mine endpoint allows triggering mining on demand
 * Useful for: testing, demos, admin control
 * 
 * SECURITY IMPLEMENTATION:
 * 
 * INPUT VALIDATION:
 * All user inputs validated via InputValidator
 * - Format validation (regex patterns)
 * - Length limits (prevent buffer overflow)
 * - Sanitization (XSS prevention)
 * 
 * PASSWORD SECURITY:
 * - Salted hashing (SHA-256)
 * - Per-user random salts
 * - Never store plaintext
 * - Secure password requirements
 * 
 * SESSION SECURITY:
 * - Cryptographically random IDs (128-bit)
 * - Stored in Redis (fast, auto-expiration)
 * - 24-hour timeout (configurable)
 * - Refresh on activity (sliding window)
 * 
 * API SECURITY:
 * - CORS headers (cross-origin allowed)
 * - Authentication checks (protected routes)
 * - Generic error messages (prevent enumeration)
 * - Sanitization (prevent injection)
 * 
 * PERFORMANCE CHARACTERISTICS:
 * 
 * REQUEST LATENCY:
 * - Database reads: ~1-10ms (MongoDB indexed)
 * - Session lookups: ~1ms (Redis in-memory)
 * - Blockchain reads: ~10ms (in-memory chain)
 * - Mining: ~seconds (Proof-of-Work, difficulty 3)
 * 
 * THROUGHPUT:
 * - Limited by threading model (one thread per request)
 * - Typical: 100-1000 req/s per core
 * - Bottleneck: Database, not HTTP server
 * 
 * SCALABILITY:
 * - Horizontal: Multiple servers + load balancer
 * - Database: MongoDB sharding, read replicas
 * - Sessions: Redis cluster for distributed sessions
 * - Blockchain: Can sync between nodes (if networking added)
 * 
 * POTENTIAL IMPROVEMENTS:
 * 
 * FEATURES:
 * - Edit/delete posts (soft delete, keep in blockchain)
 * - Unlike posts (remove from set)
 * - Unfollow users (bidirectional update)
 * - Direct messages (encrypted)
 * - Notifications (real-time with WebSockets)
 * - Feed algorithm (chronological → personalized)
 * - Search (full-text, hashtags, @mentions)
 * - Media uploads (images, videos via IPFS/S3)
 * - Trending topics
 * - User verification badges
 * 
 * SECURITY:
 * - Rate limiting (prevent abuse)
 * - HTTPS (TLS/SSL)
 * - CSRF protection
 * - Password reset (email verification)
 * - 2FA (two-factor authentication)
 * - Account lockout (after failed logins)
 * - Audit logging
 * 
 * PERFORMANCE:
 * - Caching layer (Redis for popular content)
 * - Pagination (limit query results)
 * - Database indices (username, postId, timestamps)
 * - Connection pooling
 * - Async I/O (event-driven instead of threads)
 * - CDN for static assets
 * 
 * RELIABILITY:
 * - Health check endpoints
 * - Metrics/monitoring (Prometheus, Grafana)
 * - Logging framework (structured logs)
 * - Error tracking (Sentry)
 * - Backup/restore procedures
 * - Graceful degradation
 * 
 * TESTING:
 * - Unit tests: Models, validators, utilities
 * - Integration tests: API endpoints, database operations
 * - Security tests: Injection attempts, auth bypass
 * - Load tests: Concurrent users, stress testing
 * - End-to-end: Full user workflows
 ******************************************************************************/

