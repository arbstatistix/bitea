#ifndef MONGOCLIENT_H
#define MONGOCLIENT_H

#include <string>
#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include "../models/User.h"
#include "../models/Post.h"

#ifdef HAS_MONGODB
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

class MongoClient {
private:
    std::string connectionString;
    std::string databaseName;
    bool connected;
    std::mutex mongoMutex;  // Protect MongoDB operations from concurrent access
    
    static mongocxx::instance instance; // MongoDB driver instance (must be initialized once)
    std::unique_ptr<mongocxx::client> client;
    mongocxx::database database;

    // Helper: Convert User to BSON document
    bsoncxx::document::value userToBson(const User& user) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        document doc{};
        doc << "username" << user.getUsername()
            << "email" << user.getEmail()
            << "passwordHash" << user.getPasswordHash()
            << "displayName" << user.getDisplayName()
            << "bio" << user.getBio()
            << "joinedAt" << static_cast<int64_t>(user.getJoinedAt())
            << "lastLogin" << static_cast<int64_t>(user.getLastLogin())
            << "followersCount" << user.getFollowersCount()
            << "followingCount" << user.getFollowingCount();
        
        return doc << finalize;
    }

    // Helper: Convert BSON to User
    User bsonToUser(const bsoncxx::document::view& doc) {
        std::string username = std::string(doc["username"].get_string().value);
        std::string email = std::string(doc["email"].get_string().value);
        std::string passwordHash = std::string(doc["passwordHash"].get_string().value);
        
        User user(username, email, ""); // Create user with empty password
        user.setPasswordHash(passwordHash); // Set the stored hash
        
        if (doc["displayName"]) {
            user.setDisplayName(std::string(doc["displayName"].get_string().value));
        }
        
        if (doc["bio"]) {
            user.setBio(std::string(doc["bio"].get_string().value));
        }
        
        return user;
    }

    // Helper: Convert Post to BSON document
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

    // Helper: Convert BSON to Post
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
    MongoClient(const std::string& connStr = "mongodb://localhost:27017", 
                const std::string& dbName = "bitea")
        : connectionString(connStr), databaseName(dbName), connected(false) {
    }

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

    void disconnect() {
        connected = false;
        client.reset();
        std::cout << "[MongoDB] Disconnected" << std::endl;
    }

    bool isConnected() const {
        return connected;
    }

    // User operations
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

    // Post operations
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
// Mock implementation (fallback if MongoDB driver not available)
#include <map>

class MongoClient {
private:
    std::string connectionString;
    std::string databaseName;
    bool connected;
    
    // In-memory storage (mock database)
    std::map<std::string, User> users;
    std::map<std::string, Post> posts;

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
