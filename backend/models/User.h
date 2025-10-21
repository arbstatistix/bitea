#ifndef USER_H
#define USER_H

#include <string>
#include <vector>
#include <set>
#include <ctime>
#include <sstream>
#include <openssl/sha.h>
#include <iomanip>

class User {
private:
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string displayName;
    std::string bio;
    std::set<std::string> followers;
    std::set<std::string> following;
    time_t createdAt;
    time_t lastLogin;

    std::string hashPassword(const std::string& password) const {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, password.c_str(), password.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

public:
    User() : createdAt(std::time(nullptr)), lastLogin(std::time(nullptr)) {}

    User(const std::string& username, const std::string& email, const std::string& password)
        : username(username), email(email), displayName(username) {
        passwordHash = hashPassword(password);
        createdAt = std::time(nullptr);
        lastLogin = std::time(nullptr);
    }

    // Getters
    std::string getUsername() const { return username; }
    std::string getEmail() const { return email; }
    std::string getDisplayName() const { return displayName; }
    std::string getBio() const { return bio; }
    const std::set<std::string>& getFollowers() const { return followers; }
    const std::set<std::string>& getFollowing() const { return following; }
    time_t getCreatedAt() const { return createdAt; }
    time_t getLastLogin() const { return lastLogin; }
    int getFollowerCount() const { return followers.size(); }
    int getFollowingCount() const { return following.size(); }

    // Setters
    void setDisplayName(const std::string& name) { displayName = name; }
    void setBio(const std::string& b) { bio = b; }
    void updateLastLogin() { lastLogin = std::time(nullptr); }

    // Password verification
    bool verifyPassword(const std::string& password) const {
        return passwordHash == hashPassword(password);
    }

    void changePassword(const std::string& newPassword) {
        passwordHash = hashPassword(newPassword);
    }

    // Follow/Unfollow
    void follow(const std::string& username) {
        following.insert(username);
    }

    void unfollow(const std::string& username) {
        following.erase(username);
    }

    void addFollower(const std::string& username) {
        followers.insert(username);
    }

    void removeFollower(const std::string& username) {
        followers.erase(username);
    }

    bool isFollowing(const std::string& username) const {
        return following.find(username) != following.end();
    }

    bool hasFollower(const std::string& username) const {
        return followers.find(username) != followers.end();
    }

    // JSON serialization
    std::string toJson(bool includePrivate = false) const {
        std::stringstream ss;
        ss << "{";
        ss << "\"username\":\"" << username << "\",";
        ss << "\"displayName\":\"" << displayName << "\",";
        ss << "\"bio\":\"" << bio << "\",";
        ss << "\"followers\":" << followers.size() << ",";
        ss << "\"following\":" << following.size() << ",";
        ss << "\"createdAt\":" << createdAt;
        
        if (includePrivate) {
            ss << ",\"email\":\"" << email << "\"";
            ss << ",\"lastLogin\":" << lastLogin;
        }
        
        ss << "}";
        return ss.str();
    }
};

#endif // USER_H

