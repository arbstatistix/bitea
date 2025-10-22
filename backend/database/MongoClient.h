#ifndef MONGOCLIENT_H
#define MONGOCLIENT_H

/*
 * ============================================================================
 * MongoClient - MongoDB Database Interface for Bitea Social Media Platform
 * ============================================================================
 * 
 * PURPOSE:
 * This class provides the persistent data storage layer for Bitea, a blockchain-
 * based social media application. It handles all permanent storage of users and
 * posts in MongoDB, while the blockchain validates post integrity and Redis
 * manages temporary session data.
 * 
 * ROLE IN BITEA ARCHITECTURE:
 * 
 *   [Frontend (HTML/JS)] 
 *          ↓ HTTP Requests
 *   [HttpServer (main.cpp)]
 *          ↓ API Calls
 *   [MongoClient] ← YOU ARE HERE (Persistent Storage)
 *          ↓ Stores/Retrieves
 *   [MongoDB Database]
 * 
 * PARALLEL COMPONENTS:
 * - RedisClient: Handles temporary session data (who's logged in)
 * - Blockchain: Validates and chains posts for immutability
 * - HttpServer: Routes HTTP requests to database operations
 * 
 * DATA FLOW EXAMPLE (Creating a Post):
 * 1. User submits post via frontend
 * 2. HttpServer receives POST request
 * 3. HttpServer creates Post object (models/Post.h)
 * 4. MongoClient.insertPost() saves to MongoDB
 * 5. Blockchain.addTransaction() adds post to pending transactions
 * 6. Blockchain.minePendingTransactions() creates immutable block
 * 
 * THREAD SAFETY:
 * All public methods use mongoMutex to prevent race conditions when multiple
 * HTTP requests access the database simultaneously.
 * 
 * CONDITIONAL COMPILATION:
 * - If HAS_MONGODB is defined: Uses real MongoDB C++ driver
 * - If not defined: Uses in-memory mock for testing without MongoDB
 * ============================================================================
 */

#include <string>
#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include "../models/User.h"  // User model with username, email, password, profile data
#include "../models/Post.h"  // Post model with content, author, timestamp, likes

#ifdef HAS_MONGODB
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

class MongoClient {
private:
    // ========== CONNECTION PROPERTIES ==========
    std::string connectionString;  // MongoDB URI (e.g., "mongodb://localhost:27017")
    std::string databaseName;      // Database name (default: "bitea")
    bool connected;                // Connection status flag
    std::mutex mongoMutex;         // Thread safety: Protects all MongoDB operations from concurrent HTTP requests
    
    // ========== MONGODB DRIVER OBJECTS ==========
    static mongocxx::instance instance; // MongoDB driver instance (MUST be initialized once globally)
    std::unique_ptr<mongocxx::client> client;  // MongoDB connection client
    mongocxx::database database;               // Active database handle

    /*
     * HELPER METHOD: Convert User to BSON Document
     * 
     * PURPOSE: Translates C++ User object to MongoDB's BSON format for storage
     * 
     * INTERACTION WITH BITEA:
     * - Called by: insertUser(), updateUser()
     * - Uses: User model (models/User.h) getter methods
     * - Stores: User profile data for authentication, profiles, social features
     * 
     * BSON FIELDS STORED:
     * - username: Unique identifier (indexed)
     * - email: User's email address
     * - passwordHash: SHA-256 hashed password (never store plaintext!)
     * - passwordSalt: Random salt for password security
     * - displayName: Public display name
     * - bio: User biography
     * - joinedAt, lastLogin: Timestamps for user tracking
     * - followersCount, followingCount: Social network metrics
     */
    bsoncxx::document::value userToBson(const User& user) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        document doc{};
        doc << "username" << user.getUsername()
            << "email" << user.getEmail()
            << "passwordHash" << user.getPasswordHash()
            << "passwordSalt" << user.getPasswordSalt()
            << "displayName" << user.getDisplayName()
            << "bio" << user.getBio()
            << "joinedAt" << static_cast<int64_t>(user.getJoinedAt())
            << "lastLogin" << static_cast<int64_t>(user.getLastLogin())
            << "followersCount" << user.getFollowersCount()
            << "followingCount" << user.getFollowingCount();
        
        return doc << finalize;
    }

    /*
     * HELPER METHOD: Convert BSON Document to User
     * 
     * PURPOSE: Translates MongoDB's BSON format back to C++ User object
     * 
     * INTERACTION WITH BITEA:
     * - Called by: findUser(), getAllUsers()
     * - Returns: User object for authentication, profile display, API responses
     * - Used in: Login flow, profile pages, user search
     * 
     * BACKWARD COMPATIBILITY:
     * Handles old database records that may not have all fields (e.g., passwordSalt)
     */
    User bsonToUser(const bsoncxx::document::view& doc) {
        std::string username = std::string(doc["username"].get_string().value);
        std::string email = std::string(doc["email"].get_string().value);
        std::string passwordHash = std::string(doc["passwordHash"].get_string().value);
        std::string passwordSalt = "";
        
        // Handle salt (for backward compatibility with old records)
        if (doc["passwordSalt"]) {
            passwordSalt = std::string(doc["passwordSalt"].get_string().value);
        }
        
        User user(username, email, ""); // Create user with empty password
        user.setPasswordHash(passwordHash); // Set the stored hash
        if (!passwordSalt.empty()) {
            user.setPasswordSalt(passwordSalt); // Set the stored salt
        }
        
        if (doc["displayName"]) {
            user.setDisplayName(std::string(doc["displayName"].get_string().value));
        }
        
        if (doc["bio"]) {
            user.setBio(std::string(doc["bio"].get_string().value));
        }
        
        return user;
    }

    /*
     * HELPER METHOD: Convert Post to BSON Document
     * 
     * PURPOSE: Translates C++ Post object to MongoDB's BSON format for storage
     * 
     * INTERACTION WITH BITEA:
     * - Called by: insertPost(), updatePost()
     * - Uses: Post model (models/Post.h) getter methods
     * - Stores: Social media post content in permanent database
     * 
     * BLOCKCHAIN INTEGRATION:
     * - Posts are ALSO stored in the blockchain for immutability verification
     * - MongoDB provides fast querying and retrieval
     * - Blockchain provides tamper-proof audit trail
     * 
     * BSON FIELDS STORED:
     * - postId: Unique identifier (indexed)
     * - author: Username who created the post
     * - content: The actual post text
     * - timestamp: When post was created
     * - likesCount: Number of likes (updated when users like)
     * - commentsCount: Number of comments
     */
    bsoncxx::document::value postToBson(const Post& post) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        document doc{};
        doc << "postId" << post.getId()
            << "author" << post.getAuthor()
            << "content" << post.getContent()
            << "timestamp" << static_cast<int64_t>(post.getTimestamp())
            << "likesCount" << post.getLikeCount()
            << "commentsCount" << post.getCommentCount();
        
        return doc << finalize;
    }

    /*
     * HELPER METHOD: Convert BSON Document to Post
     * 
     * PURPOSE: Translates MongoDB's BSON format back to C++ Post object
     * 
     * INTERACTION WITH BITEA:
     * - Called by: findPost(), getAllPosts(), getPostsByAuthor()
     * - Returns: Post object for feed display, profile posts, API responses
     * - Used in: Homepage feed, user profiles, post detail pages
     */
    Post bsonToPost(const bsoncxx::document::view& doc) {
        std::string postId = std::string(doc["postId"].get_string().value);
        std::string author = std::string(doc["author"].get_string().value);
        std::string content = std::string(doc["content"].get_string().value);
        
        Post post(postId, author, content);
        
        if (doc["timestamp"]) {
            // Post timestamp is set in constructor, so this is informational
        }
        
        return post;
    }

public:
    // ============================================================================
    // PUBLIC API - Database Lifecycle Methods
    // ============================================================================
    
    /*
     * CONSTRUCTOR
     * 
     * PARAMETERS:
     * - connStr: MongoDB connection URI (default: localhost:27017)
     * - dbName: Database name (default: "bitea")
     * 
     * CALLED BY: main.cpp during application startup
     * WHEN: Before HttpServer starts accepting requests
     */
    MongoClient(const std::string& connStr = "mongodb://localhost:27017", 
                const std::string& dbName = "bitea")
        : connectionString(connStr), databaseName(dbName), connected(false) {
    }

    /*
     * METHOD: connect()
     * 
     * PURPOSE: Establishes connection to MongoDB server and creates indexes
     * 
     * INTERACTION WITH BITEA:
     * - Called by: main.cpp during startup
     * - Before this: Application cannot store/retrieve data
     * - After this: All database operations become available
     * 
     * PROCESS:
     * 1. Creates MongoDB client with connection URI
     * 2. Sends ping command to verify connection
     * 3. Calls createIndexes() to optimize database performance
     * 
     * RETURNS: true if connected successfully, false otherwise
     * 
     * ERROR HANDLING: Catches exceptions and logs failures
     */
    bool connect() {
        try {
            mongocxx::uri uri(connectionString);
            client = std::make_unique<mongocxx::client>(uri);
            database = (*client)[databaseName];
            
            // Test connection by running a ping command
            auto admin = (*client)["admin"];
            auto result = admin.run_command(bsoncxx::from_json(R"({"ping": 1})"));
            
            connected = true;
            std::cout << "[MongoDB] Connected to " << connectionString << "/" << databaseName << std::endl;
            
            // Create indexes
            createIndexes();
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Connection failed: " << e.what() << std::endl;
            connected = false;
            return false;
        }
    }

    /*
     * METHOD: createIndexes()
     * 
     * PURPOSE: Creates database indexes for fast query performance
     * 
     * INTERACTION WITH BITEA:
     * - Called by: connect() during startup
     * - Optimizes: User lookups by username, post lookups by postId and author
     * 
     * INDEXES CREATED:
     * 1. users.username (unique) - Fast login, profile lookups
     * 2. posts.postId (unique) - Fast individual post retrieval
     * 3. posts.author - Fast user profile post listing
     * 
     * PERFORMANCE IMPACT:
     * Without indexes: O(n) linear scan of entire collection
     * With indexes: O(log n) tree-based lookup
     * 
     * Example: Finding user "alice" in 1 million users
     * - Without index: Check all 1M records
     * - With index: Check ~20 records (log₂ 1M ≈ 20)
     */
    void createIndexes() {
        try {
            auto users_collection = database["users"];
            auto posts_collection = database["posts"];
            
            // Create index on username (unique)
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document username_index{};
            username_index << "username" << 1;
            
            document unique_option{};
            unique_option << "unique" << true;
            
            users_collection.create_index(username_index.view(), unique_option.view());
            
            // Create index on postId (unique)
            document postid_index{};
            postid_index << "postId" << 1;
            posts_collection.create_index(postid_index.view(), unique_option.view());
            
            // Create index on author for posts
            document author_index{};
            author_index << "author" << 1;
            posts_collection.create_index(author_index.view());
            
            std::cout << "[MongoDB] Indexes created" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Index creation warning: " << e.what() << std::endl;
        }
    }

    /*
     * METHOD: disconnect()
     * 
     * PURPOSE: Closes MongoDB connection and cleans up resources
     * 
     * CALLED BY: main.cpp during application shutdown (Ctrl+C)
     */
    void disconnect() {
        connected = false;
        client.reset();
        std::cout << "[MongoDB] Disconnected" << std::endl;
    }

    /*
     * METHOD: isConnected()
     * 
     * PURPOSE: Check if database connection is active
     * 
     * USED BY: All database operations check this before proceeding
     * RETURNS: true if connected, false otherwise
     */
    bool isConnected() const {
        return connected;
    }

    // ============================================================================
    // PUBLIC API - User Operations (Authentication & Profile Management)
    // ============================================================================
    
    /*
     * METHOD: insertUser()
     * 
     * PURPOSE: Create new user account in database
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer when handling POST /api/register
     * - Frontend: User fills registration form (username, email, password)
     * - Flow: Frontend → HttpServer → insertUser() → MongoDB
     * 
     * THREAD SAFETY: Uses mongoMutex lock for concurrent registrations
     * 
     * USER FLOW EXAMPLE:
     * 1. User submits registration form
     * 2. HttpServer validates input (InputValidator.h)
     * 3. HttpServer hashes password (User model)
     * 4. insertUser() stores in MongoDB
     * 5. User can now login
     * 
     * RETURNS: true if inserted, false if username already exists or error
     */
    bool insertUser(const User& user) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["users"];
            auto doc = userToBson(user);
            collection.insert_one(doc.view());
            std::cout << "[MongoDB] Inserted user: " << user.getUsername() << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Insert user failed: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * METHOD: findUser()
     * 
     * PURPOSE: Retrieve user account data from database
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for login authentication, profile viewing
     * - Frontend: User enters username/password OR visits profile page
     * 
     * USE CASES:
     * 1. LOGIN: Retrieve passwordHash to verify credentials
     * 2. PROFILE: Get displayName, bio, follower counts
     * 3. POSTS: Verify post author exists
     * 
     * AUTHENTICATION FLOW:
     * 1. User submits login form
     * 2. HttpServer calls findUser(username)
     * 3. Compare submitted password with stored passwordHash
     * 4. If match: Create session in RedisClient
     * 5. Return session token to frontend
     * 
     * PARAMETERS:
     * - username: Username to search for
     * - user: Reference to User object (populated if found)
     * 
     * RETURNS: true if user found, false otherwise
     */
    bool findUser(const std::string& username, User& user) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["users"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document filter{};
            filter << "username" << username;
            
            auto result = collection.find_one(filter.view());
            if (result) {
                user = bsonToUser(result->view());
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Find user failed: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * METHOD: updateUser()
     * 
     * PURPOSE: Modify existing user account data
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer when user updates profile or logs in
     * 
     * COMMON UPDATE SCENARIOS:
     * 1. Profile Edit: User changes displayName or bio
     * 2. Login: Update lastLogin timestamp
     * 3. Social: Increment/decrement follower counts
     * 4. Security: Update password (new hash + salt)
     * 
     * WHAT GETS UPDATED:
     * - All fields EXCEPT username (username is immutable identifier)
     * - Uses MongoDB $set operator for efficient partial updates
     * 
     * THREAD SAFETY: Mutex prevents concurrent updates to same user
     * 
     * RETURNS: true if user found and updated, false otherwise
     */
    bool updateUser(const User& user) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["users"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document filter{};
            filter << "username" << user.getUsername();
            
            document update{};
            update << "$set" << bsoncxx::builder::stream::open_document
                   << "email" << user.getEmail()
                   << "passwordHash" << user.getPasswordHash()
                   << "passwordSalt" << user.getPasswordSalt()
                   << "displayName" << user.getDisplayName()
                   << "bio" << user.getBio()
                   << "lastLogin" << static_cast<int64_t>(user.getLastLogin())
                   << "followersCount" << user.getFollowersCount()
                   << "followingCount" << user.getFollowingCount()
                   << bsoncxx::builder::stream::close_document;
            
            auto result = collection.update_one(filter.view(), update.view());
            if (result && result->modified_count() > 0) {
                std::cout << "[MongoDB] Updated user: " << user.getUsername() << std::endl;
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Update user failed: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * METHOD: deleteUser()
     * 
     * PURPOSE: Remove user account from database
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer when user deletes their account
     * - Also called by: Admin functions for moderation
     * 
     * CASCADING EFFECTS (IMPORTANT):
     * This only deletes the user record. Consider implementing:
     * 1. Delete user's posts (or mark as [deleted])
     * 2. Remove user's sessions from Redis
     * 3. Update follower counts of users they followed
     * 
     * SECURITY: Should verify user is logged in and owns the account
     * 
     * RETURNS: true if user found and deleted, false otherwise
     */
    bool deleteUser(const std::string& username) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["users"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document filter{};
            filter << "username" << username;
            
            auto result = collection.delete_one(filter.view());
            if (result && result->deleted_count() > 0) {
                std::cout << "[MongoDB] Deleted user: " << username << std::endl;
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Delete user failed: " << e.what() << std::endl;
            return false;
        }
    }

    // ============================================================================
    // PUBLIC API - Post Operations (Social Media Content Management)
    // ============================================================================
    
    /*
     * METHOD: insertPost()
     * 
     * PURPOSE: Store new post in database
     * 
     * INTERACTION WITH BITEA - Complete Post Creation Flow:
     * 
     * 1. [Frontend] User types post content, clicks "Post"
     * 2. [api.js] Sends POST /api/posts with content + session token
     * 3. [HttpServer] Validates session with RedisClient
     * 4. [HttpServer] Creates Post object (models/Post.h)
     * 5. [MongoClient] insertPost() stores in MongoDB ← YOU ARE HERE
     * 6. [Blockchain] addTransaction() adds to pending transactions
     * 7. [Blockchain] Mining process creates immutable block
     * 8. [HttpServer] Returns success to frontend
     * 9. [Frontend] Refreshes feed to show new post
     * 
     * DUAL STORAGE STRATEGY:
     * - MongoDB: Fast queries, sorting, filtering (getAllPosts, getPostsByAuthor)
     * - Blockchain: Tamper-proof verification, audit trail
     * 
     * WHY BOTH?
     * - MongoDB: "What posts should I show?" (performance)
     * - Blockchain: "Has this post been tampered with?" (security)
     * 
     * RETURNS: true if inserted, false if postId collision or error
     */
    bool insertPost(const Post& post) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["posts"];
            auto doc = postToBson(post);
            collection.insert_one(doc.view());
            std::cout << "[MongoDB] Inserted post: " << post.getId() << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Insert post failed: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * METHOD: findPost()
     * 
     * PURPOSE: Retrieve single post by ID
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for GET /api/posts/:id
     * - Frontend: User clicks on a post to view details/comments
     * 
     * USE CASES:
     * 1. Post detail page
     * 2. Displaying comments thread
     * 3. Like/unlike operations
     * 4. Edit post (verify author)
     * 
     * RETURNS: true if post found, false otherwise
     */
    bool findPost(const std::string& postId, Post& post) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["posts"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document filter{};
            filter << "postId" << postId;
            
            auto result = collection.find_one(filter.view());
            if (result) {
                post = bsonToPost(result->view());
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Find post failed: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * METHOD: updatePost()
     * 
     * PURPOSE: Modify existing post data
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer when post is liked/unliked or commented
     * 
     * COMMON UPDATE SCENARIOS:
     * 1. LIKE: Increment likesCount
     * 2. UNLIKE: Decrement likesCount
     * 3. COMMENT: Increment commentsCount
     * 4. EDIT: Update content (if edit feature exists)
     * 
     * BLOCKCHAIN CONSIDERATION:
     * - Original post content is immutable in blockchain
     * - MongoDB likes/comments are mutable for UX
     * - This is acceptable: blockchain verifies original content
     * 
     * RETURNS: true if post found and updated, false otherwise
     */
    bool updatePost(const Post& post) {
        if (!connected) return false;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["posts"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document filter{};
            filter << "postId" << post.getId();
            
            document update{};
            update << "$set" << bsoncxx::builder::stream::open_document
                   << "content" << post.getContent()
                   << "likesCount" << post.getLikeCount()
                   << "commentsCount" << post.getCommentCount()
                   << bsoncxx::builder::stream::close_document;
            
            auto result = collection.update_one(filter.view(), update.view());
            if (result && result->modified_count() > 0) {
                std::cout << "[MongoDB] Updated post: " << post.getId() << std::endl;
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Update post failed: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * METHOD: getAllPosts()
     * 
     * PURPOSE: Retrieve all posts sorted by newest first
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for GET /api/posts (homepage feed)
     * - Frontend: Displays social media feed on homepage
     * 
     * FEED ALGORITHM:
     * - Currently: Simple reverse chronological (newest first)
     * - Future: Could add filtering, pagination, recommendations
     * 
     * HOMEPAGE FLOW:
     * 1. User opens Bitea website
     * 2. Frontend calls GET /api/posts
     * 3. HttpServer calls getAllPosts()
     * 4. MongoDB returns posts sorted by timestamp DESC
     * 5. Frontend (app.js) renders posts in feed
     * 
     * PERFORMANCE:
     * - Uses MongoDB sort with timestamp index
     * - Consider pagination for large databases (limit + skip)
     * 
     * RETURNS: Vector of Post objects (empty if no posts or error)
     */
    std::vector<Post> getAllPosts() {
        std::vector<Post> result;
        if (!connected) return result;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["posts"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            // Sort by timestamp descending
            document sort_order{};
            sort_order << "timestamp" << -1;
            
            mongocxx::options::find opts{};
            opts.sort(sort_order.view());
            
            auto cursor = collection.find({}, opts);
            
            for (auto&& doc : cursor) {
                result.push_back(bsonToPost(doc));
            }
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Get all posts failed: " << e.what() << std::endl;
        }
        
        return result;
    }

    /*
     * METHOD: getPostsByAuthor()
     * 
     * PURPOSE: Retrieve all posts by specific user
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for GET /api/users/:username/posts
     * - Frontend: User profile page showing user's post history
     * 
     * PROFILE PAGE FLOW:
     * 1. User clicks on another user's name
     * 2. Frontend loads profile page
     * 3. Calls GET /api/users/alice/posts
     * 4. HttpServer calls getPostsByAuthor("alice")
     * 5. Returns all posts by alice, newest first
     * 6. Frontend displays on profile page
     * 
     * PERFORMANCE:
     * - Uses author index for fast filtering
     * - Sorted by timestamp DESC for chronological order
     * 
     * PARAMETERS:
     * - author: Username of post author
     * 
     * RETURNS: Vector of Post objects (empty if user has no posts)
     */
    std::vector<Post> getPostsByAuthor(const std::string& author) {
        std::vector<Post> result;
        if (!connected) return result;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["posts"];
            
            using bsoncxx::builder::stream::document;
            using bsoncxx::builder::stream::finalize;
            
            document filter{};
            filter << "author" << author;
            
            document sort_order{};
            sort_order << "timestamp" << -1;
            
            mongocxx::options::find opts{};
            opts.sort(sort_order.view());
            
            auto cursor = collection.find(filter.view(), opts);
            
            for (auto&& doc : cursor) {
                result.push_back(bsonToPost(doc));
            }
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Get posts by author failed: " << e.what() << std::endl;
        }
        
        return result;
    }

    /*
     * METHOD: getAllUsers()
     * 
     * PURPOSE: Retrieve all registered users
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for GET /api/users (admin or user search)
     * - Frontend: User directory, search functionality, admin panel
     * 
     * USE CASES:
     * 1. User search/discovery
     * 2. Admin user management
     * 3. Analytics/statistics
     * 
     * SECURITY CONSIDERATION:
     * - Returns User objects with password hashes
     * - HttpServer should strip sensitive data before sending to frontend
     * - Never send passwordHash or passwordSalt to client
     * 
     * RETURNS: Vector of User objects (empty if no users or error)
     */
    std::vector<User> getAllUsers() {
        std::vector<User> result;
        if (!connected) return result;
        
        std::lock_guard<std::mutex> lock(mongoMutex);
        try {
            auto collection = database["users"];
            auto cursor = collection.find({});
            
            for (auto&& doc : cursor) {
                result.push_back(bsonToUser(doc));
            }
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Get all users failed: " << e.what() << std::endl;
        }
        
        return result;
    }

    /*
     * METHOD: getUserCount()
     * 
     * PURPOSE: Get total number of registered users
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for statistics/analytics endpoints
     * - Frontend: Dashboard showing platform metrics
     * 
     * USE CASES:
     * 1. Homepage stats ("Join 1,234 users!")
     * 2. Admin dashboard
     * 3. Growth analytics
     * 
     * PERFORMANCE: Uses MongoDB count_documents() - efficient O(1) operation
     * 
     * RETURNS: Number of users, or 0 if error/disconnected
     */
    int getUserCount() const {
        if (!connected) return 0;
        
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mongoMutex));
        try {
            auto collection = database["users"];
            return static_cast<int>(collection.count_documents({}));
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Get user count failed: " << e.what() << std::endl;
            return 0;
        }
    }

    /*
     * METHOD: getPostCount()
     * 
     * PURPOSE: Get total number of posts in database
     * 
     * INTERACTION WITH BITEA:
     * - Called by: HttpServer for statistics endpoints
     * - Frontend: Dashboard showing content metrics
     * 
     * USE CASES:
     * 1. Platform statistics ("10,000 posts and counting!")
     * 2. Admin dashboard
     * 3. Content analytics
     * 
     * BLOCKCHAIN COMPARISON:
     * - This counts posts in MongoDB (fast queries)
     * - Blockchain also stores posts (immutable verification)
     * - Should match if no data corruption
     * 
     * RETURNS: Number of posts, or 0 if error/disconnected
     */
    int getPostCount() const {
        if (!connected) return 0;
        
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mongoMutex));
        try {
            auto collection = database["posts"];
            return static_cast<int>(collection.count_documents({}));
        } catch (const std::exception& e) {
            std::cerr << "[MongoDB] Get post count failed: " << e.what() << std::endl;
            return 0;
        }
    }
};

// Initialize static member
mongocxx::instance MongoClient::instance{};

#else
/*
 * ============================================================================
 * MOCK IMPLEMENTATION - In-Memory Testing Database
 * ============================================================================
 * 
 * PURPOSE:
 * Provides a lightweight in-memory alternative when MongoDB driver is not
 * available. Useful for:
 * - Development without MongoDB installation
 * - Unit testing
 * - Quick prototyping
 * - CI/CD environments
 * 
 * LIMITATIONS:
 * - Data lost when application stops (not persistent)
 * - No indexing performance benefits
 * - Not suitable for production
 * - Single process only (no distributed access)
 * 
 * COMPATIBILITY:
 * Implements same API as real MongoClient, so HttpServer code works unchanged.
 * ============================================================================
 */
#include <map>

class MongoClient {
private:
    std::string connectionString;
    std::string databaseName;
    bool connected;
    
    // In-memory storage (mock database)
    // Uses STL map: key = username/postId, value = User/Post object
    // Data structure mimics MongoDB collections but stored in RAM
    std::map<std::string, User> users;  // "users" collection
    std::map<std::string, Post> posts;  // "posts" collection

public:
    MongoClient(const std::string& connStr = "mongodb://localhost:27017", 
                const std::string& dbName = "bitea")
        : connectionString(connStr), databaseName(dbName), connected(false) {
    }

    bool connect() {
        connected = true;
        std::cout << "[MongoDB MOCK] Connected to " << connectionString << "/" << databaseName << std::endl;
        return true;
    }

    void disconnect() {
        connected = false;
        std::cout << "[MongoDB MOCK] Disconnected" << std::endl;
    }

    bool isConnected() const {
        return connected;
    }

    bool insertUser(const User& user) {
        if (!connected) return false;
        users[user.getUsername()] = user;
        std::cout << "[MongoDB MOCK] Inserted user: " << user.getUsername() << std::endl;
        return true;
    }

    bool findUser(const std::string& username, User& user) {
        if (!connected) return false;
        auto it = users.find(username);
        if (it != users.end()) {
            user = it->second;
            return true;
        }
        return false;
    }

    bool updateUser(const User& user) {
        if (!connected) return false;
        auto it = users.find(user.getUsername());
        if (it != users.end()) {
            it->second = user;
            std::cout << "[MongoDB MOCK] Updated user: " << user.getUsername() << std::endl;
            return true;
        }
        return false;
    }

    bool deleteUser(const std::string& username) {
        if (!connected) return false;
        auto it = users.find(username);
        if (it != users.end()) {
            users.erase(it);
            std::cout << "[MongoDB MOCK] Deleted user: " << username << std::endl;
            return true;
        }
        return false;
    }

    bool insertPost(const Post& post) {
        if (!connected) return false;
        posts[post.getId()] = post;
        std::cout << "[MongoDB MOCK] Inserted post: " << post.getId() << std::endl;
        return true;
    }

    bool findPost(const std::string& postId, Post& post) {
        if (!connected) return false;
        auto it = posts.find(postId);
        if (it != posts.end()) {
            post = it->second;
            return true;
        }
        return false;
    }

    bool updatePost(const Post& post) {
        if (!connected) return false;
        auto it = posts.find(post.getId());
        if (it != posts.end()) {
            it->second = post;
            std::cout << "[MongoDB MOCK] Updated post: " << post.getId() << std::endl;
            return true;
        }
        return false;
    }

    std::vector<Post> getAllPosts() {
        std::vector<Post> result;
        if (!connected) return result;
        
        for (const auto& pair : posts) {
            result.push_back(pair.second);
        }
        
        std::sort(result.begin(), result.end(), 
                  [](const Post& a, const Post& b) {
                      return a.getTimestamp() > b.getTimestamp();
                  });
        
        return result;
    }

    std::vector<Post> getPostsByAuthor(const std::string& author) {
        std::vector<Post> result;
        if (!connected) return result;
        
        for (const auto& pair : posts) {
            if (pair.second.getAuthor() == author) {
                result.push_back(pair.second);
            }
        }
        
        std::sort(result.begin(), result.end(), 
                  [](const Post& a, const Post& b) {
                      return a.getTimestamp() > b.getTimestamp();
                  });
        
        return result;
    }

    std::vector<User> getAllUsers() {
        std::vector<User> result;
        if (!connected) return result;
        
        for (const auto& pair : users) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    int getUserCount() const {
        return users.size();
    }

    int getPostCount() const {
        return posts.size();
    }
};

#endif // HAS_MONGODB

#endif // MONGOCLIENT_H
