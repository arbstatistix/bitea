#ifndef MONGOCLIENT_H
#define MONGOCLIENT_H

#include <string>
#include <memory>
#include <iostream>
#include <map>
#include "../models/User.h"
#include "../models/Post.h"

// Simple mock implementation for MongoDB
// In production, use mongocxx driver
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
        // In production, actually connect to MongoDB
        // For now, just simulate connection
        connected = true;
        std::cout << "[MongoDB] Connected to " << connectionString << "/" << databaseName << std::endl;
        return true;
    }

    void disconnect() {
        connected = false;
        std::cout << "[MongoDB] Disconnected" << std::endl;
    }

    bool isConnected() const {
        return connected;
    }

    // User operations
    bool insertUser(const User& user) {
        if (!connected) return false;
        users[user.getUsername()] = user;
        std::cout << "[MongoDB] Inserted user: " << user.getUsername() << std::endl;
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
            std::cout << "[MongoDB] Updated user: " << user.getUsername() << std::endl;
            return true;
        }
        return false;
    }

    bool deleteUser(const std::string& username) {
        if (!connected) return false;
        auto it = users.find(username);
        if (it != users.end()) {
            users.erase(it);
            std::cout << "[MongoDB] Deleted user: " << username << std::endl;
            return true;
        }
        return false;
    }

    // Post operations
    bool insertPost(const Post& post) {
        if (!connected) return false;
        posts[post.getId()] = post;
        std::cout << "[MongoDB] Inserted post: " << post.getId() << std::endl;
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
            std::cout << "[MongoDB] Updated post: " << post.getId() << std::endl;
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
        
        // Sort by timestamp (newest first)
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
        
        // Sort by timestamp (newest first)
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

#endif // MONGOCLIENT_H

