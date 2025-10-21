#ifndef POST_H
#define POST_H

#include <string>
#include <vector>
#include <set>
#include <ctime>
#include <sstream>
#include <algorithm>

struct Comment {
    std::string id;
    std::string author;
    std::string content;
    time_t timestamp;

    Comment(const std::string& author, const std::string& content)
        : author(author), content(content) {
        timestamp = std::time(nullptr);
        id = author + "-" + std::to_string(timestamp);
    }

    std::string toJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << id << "\",";
        ss << "\"author\":\"" << author << "\",";
        ss << "\"content\":\"" << escapeJson(content) << "\",";
        ss << "\"timestamp\":" << timestamp;
        ss << "}";
        return ss.str();
    }

private:
    std::string escapeJson(const std::string& str) const {
        std::string result;
        for (char c : str) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        return result;
    }
};

class Post {
private:
    std::string id;
    std::string author;
    std::string content;
    time_t timestamp;
    std::set<std::string> likes;
    std::vector<Comment> comments;
    std::string blockchainHash;  // Hash of the block containing this post
    bool isOnChain;

    std::string escapeJson(const std::string& str) const {
        std::string result;
        for (char c : str) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        return result;
    }

public:
    Post() : timestamp(std::time(nullptr)), isOnChain(false) {}

    Post(const std::string& id, const std::string& author, const std::string& content)
        : id(id), author(author), content(content), isOnChain(false) {
        timestamp = std::time(nullptr);
    }

    // Getters
    std::string getId() const { return id; }
    std::string getAuthor() const { return author; }
    std::string getContent() const { return content; }
    time_t getTimestamp() const { return timestamp; }
    const std::set<std::string>& getLikes() const { return likes; }
    const std::vector<Comment>& getComments() const { return comments; }
    int getLikeCount() const { return likes.size(); }
    int getCommentCount() const { return comments.size(); }
    std::string getBlockchainHash() const { return blockchainHash; }
    bool getIsOnChain() const { return isOnChain; }

    // Setters
    void setBlockchainHash(const std::string& hash) {
        blockchainHash = hash;
        isOnChain = true;
    }

    // Like/Unlike
    bool addLike(const std::string& username) {
        auto result = likes.insert(username);
        return result.second;  // true if inserted, false if already existed
    }

    bool removeLike(const std::string& username) {
        return likes.erase(username) > 0;
    }

    bool hasLiked(const std::string& username) const {
        return likes.find(username) != likes.end();
    }

    // Comments
    void addComment(const std::string& author, const std::string& content) {
        comments.emplace_back(author, content);
    }

    // JSON serialization
    std::string toJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << id << "\",";
        ss << "\"author\":\"" << author << "\",";
        ss << "\"content\":\"" << escapeJson(content) << "\",";
        ss << "\"timestamp\":" << timestamp << ",";
        ss << "\"likes\":" << likes.size() << ",";
        ss << "\"comments\":" << comments.size() << ",";
        ss << "\"isOnChain\":" << (isOnChain ? "true" : "false");
        
        if (!blockchainHash.empty()) {
            ss << ",\"blockchainHash\":\"" << blockchainHash << "\"";
        }
        
        ss << "}";
        return ss.str();
    }

    std::string toDetailedJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << id << "\",";
        ss << "\"author\":\"" << author << "\",";
        ss << "\"content\":\"" << escapeJson(content) << "\",";
        ss << "\"timestamp\":" << timestamp << ",";
        ss << "\"likes\":" << likes.size() << ",";
        ss << "\"isOnChain\":" << (isOnChain ? "true" : "false") << ",";
        
        if (!blockchainHash.empty()) {
            ss << "\"blockchainHash\":\"" << blockchainHash << "\",";
        }
        
        // Add comments array
        ss << "\"comments\":[";
        for (size_t i = 0; i < comments.size(); i++) {
            ss << comments[i].toJson();
            if (i < comments.size() - 1) ss << ",";
        }
        ss << "]";
        
        ss << "}";
        return ss.str();
    }
};

#endif // POST_H

