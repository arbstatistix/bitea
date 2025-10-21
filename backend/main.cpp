#include <iostream>
#include <memory>
#include <sstream>
#include <algorithm>
#include "server/HttpServer.h"
#include "blockchain/Blockchain.h"
#include "database/MongoClient.h"
#include "database/RedisClient.h"
#include "models/User.h"
#include "models/Post.h"
#include "models/Session.h"

class BiteaApp {
private:
    std::unique_ptr<HttpServer> server;
    std::unique_ptr<Blockchain> blockchain;
    std::unique_ptr<MongoClient> mongodb;
    std::unique_ptr<RedisClient> redis;

    // Helper: Extract session from request
    std::string getSessionId(const HttpRequest& req) {
        auto it = req.headers.find("Authorization");
        if (it != req.headers.end()) {
            std::string auth = it->second;
            if (auth.substr(0, 7) == "Bearer ") {
                return auth.substr(7);
            }
        }
        return "";
    }

    // Helper: Validate session and get username
    bool validateSession(const HttpRequest& req, std::string& username) {
        std::string sessionId = getSessionId(req);
        if (sessionId.empty()) return false;

        Session session;
        if (redis->getSession(sessionId, session)) {
            username = session.getUsername();
            redis->refreshSession(sessionId);
            return true;
        }
        return false;
    }

    // Helper: URL decode
    std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value;
                std::istringstream is(str.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                }
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }

    // Helper: Parse JSON (very simple, for production use a real JSON library)
    std::string getJsonValue(const std::string& json, const std::string& key) {
        std::string pattern = "\"" + key + "\":\"";
        size_t start = json.find(pattern);
        if (start == std::string::npos) return "";
        
        start += pattern.length();
        size_t end = json.find("\"", start);
        if (end == std::string::npos) return "";
        
        return json.substr(start, end - start);
    }

public:
    BiteaApp() {
        server = std::make_unique<HttpServer>(3000);
        blockchain = std::make_unique<Blockchain>(3, 5); // difficulty 3, 5 tx per block
        mongodb = std::make_unique<MongoClient>();
        redis = std::make_unique<RedisClient>();
    }

    void setupRoutes() {
        // Health check
        server->get("/", [](const HttpRequest& req, HttpResponse& res) {
            res.json("{\"message\":\"Bitea API Server\",\"status\":\"running\"}");
        });

        // API Info
        server->get("/api", [this](const HttpRequest& req, HttpResponse& res) {
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

        // Register
        server->post("/api/register", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username = getJsonValue(req.body, "username");
            std::string email = getJsonValue(req.body, "email");
            std::string password = getJsonValue(req.body, "password");

            if (username.empty() || email.empty() || password.empty()) {
                res.statusCode = 400;
                res.json("{\"error\":\"Missing required fields\"}");
                return;
            }

            // Check if user exists
            User existingUser;
            if (mongodb->findUser(username, existingUser)) {
                res.statusCode = 400;
                res.json("{\"error\":\"Username already exists\"}");
                return;
            }

            // Create user
            User newUser(username, email, password);
            mongodb->insertUser(newUser);

            // Add to blockchain
            std::stringstream txData;
            txData << "{\"action\":\"register\",\"username\":\"" << username << "\"}";
            Transaction tx(username, TransactionType::USER_REGISTRATION, txData.str());
            blockchain->addTransaction(tx);

            res.statusCode = 201;
            res.json(newUser.toJson(true));
        });

        // Login
        server->post("/api/login", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username = getJsonValue(req.body, "username");
            std::string password = getJsonValue(req.body, "password");

            User user;
            if (!mongodb->findUser(username, user)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Invalid credentials\"}");
                return;
            }

            if (!user.verifyPassword(password)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Invalid credentials\"}");
                return;
            }

            // Create session
            Session session(username);
            redis->createSession(session);

            user.updateLastLogin();
            mongodb->updateUser(user);

            std::stringstream ss;
            ss << "{\"sessionId\":\"" << session.getSessionId() << "\",";
            ss << "\"user\":" << user.toJson(true) << "}";
            res.json(ss.str());
        });

        // Logout
        server->post("/api/logout", [this](const HttpRequest& req, HttpResponse& res) {
            std::string sessionId = getSessionId(req);
            if (!sessionId.empty()) {
                redis->deleteSession(sessionId);
            }
            res.json("{\"message\":\"Logged out successfully\"}");
        });

        // Create post
        server->post("/api/posts", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username;
            if (!validateSession(req, username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            std::string content = getJsonValue(req.body, "content");
            if (content.empty()) {
                res.statusCode = 400;
                res.json("{\"error\":\"Content is required\"}");
                return;
            }

            // Create post
            std::string postId = username + "-" + std::to_string(std::time(nullptr));
            Post post(postId, username, content);
            mongodb->insertPost(post);

            // Add to blockchain
            std::stringstream txData;
            txData << "{\"action\":\"post\",\"postId\":\"" << postId 
                   << "\",\"author\":\"" << username << "\"}";
            Transaction tx(username, TransactionType::POST, txData.str());
            blockchain->addTransaction(tx);

            res.statusCode = 201;
            res.json(post.toJson());
        });

        // Get all posts
        server->get("/api/posts", [this](const HttpRequest& req, HttpResponse& res) {
            auto posts = mongodb->getAllPosts();
            
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < posts.size(); i++) {
                ss << posts[i].toJson();
                if (i < posts.size() - 1) ss << ",";
            }
            ss << "]";
            
            res.json(ss.str());
        });

        // Get post by ID
        server->get("/api/posts/:id", [this](const HttpRequest& req, HttpResponse& res) {
            std::string postId = req.params.at("id");
            
            Post post;
            if (!mongodb->findPost(postId, post)) {
                res.statusCode = 404;
                res.json("{\"error\":\"Post not found\"}");
                return;
            }

            res.json(post.toDetailedJson());
        });

        // Like post
        server->post("/api/posts/:id/like", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username;
            if (!validateSession(req, username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            std::string postId = req.params.at("id");
            Post post;
            if (!mongodb->findPost(postId, post)) {
                res.statusCode = 404;
                res.json("{\"error\":\"Post not found\"}");
                return;
            }

            post.addLike(username);
            mongodb->updatePost(post);

            // Add to blockchain
            std::stringstream txData;
            txData << "{\"action\":\"like\",\"postId\":\"" << postId << "\"}";
            Transaction tx(username, TransactionType::LIKE, txData.str());
            blockchain->addTransaction(tx);

            res.json(post.toJson());
        });

        // Comment on post
        server->post("/api/posts/:id/comment", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username;
            if (!validateSession(req, username)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            std::string postId = req.params.at("id");
            std::string content = getJsonValue(req.body, "content");

            if (content.empty()) {
                res.statusCode = 400;
                res.json("{\"error\":\"Comment content is required\"}");
                return;
            }

            Post post;
            if (!mongodb->findPost(postId, post)) {
                res.statusCode = 404;
                res.json("{\"error\":\"Post not found\"}");
                return;
            }

            post.addComment(username, content);
            mongodb->updatePost(post);

            // Add to blockchain
            std::stringstream txData;
            txData << "{\"action\":\"comment\",\"postId\":\"" << postId << "\"}";
            Transaction tx(username, TransactionType::COMMENT, txData.str());
            blockchain->addTransaction(tx);

            res.json(post.toDetailedJson());
        });

        // Get user profile
        server->get("/api/users/:username", [this](const HttpRequest& req, HttpResponse& res) {
            std::string username = req.params.at("username");
            
            User user;
            if (!mongodb->findUser(username, user)) {
                res.statusCode = 404;
                res.json("{\"error\":\"User not found\"}");
                return;
            }

            res.json(user.toJson(false));
        });

        // Follow user
        server->post("/api/users/:username/follow", [this](const HttpRequest& req, HttpResponse& res) {
            std::string currentUser;
            if (!validateSession(req, currentUser)) {
                res.statusCode = 401;
                res.json("{\"error\":\"Unauthorized\"}");
                return;
            }

            std::string targetUsername = req.params.at("username");
            
            User user, targetUser;
            if (!mongodb->findUser(currentUser, user) || !mongodb->findUser(targetUsername, targetUser)) {
                res.statusCode = 404;
                res.json("{\"error\":\"User not found\"}");
                return;
            }

            user.follow(targetUsername);
            targetUser.addFollower(currentUser);
            
            mongodb->updateUser(user);
            mongodb->updateUser(targetUser);

            // Add to blockchain
            std::stringstream txData;
            txData << "{\"action\":\"follow\",\"target\":\"" << targetUsername << "\"}";
            Transaction tx(currentUser, TransactionType::FOLLOW, txData.str());
            blockchain->addTransaction(tx);

            res.json("{\"message\":\"Followed successfully\"}");
        });

        // Get blockchain
        server->get("/api/blockchain", [this](const HttpRequest& req, HttpResponse& res) {
            const auto& chain = blockchain->getChain();
            
            std::stringstream ss;
            ss << "{\"blocks\":[";
            for (size_t i = 0; i < chain.size(); i++) {
                const auto& block = chain[i];
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

        // Validate blockchain
        server->get("/api/blockchain/validate", [this](const HttpRequest& req, HttpResponse& res) {
            bool valid = blockchain->isChainValid();
            std::stringstream ss;
            ss << "{\"valid\":" << (valid ? "true" : "false") << "}";
            res.json(ss.str());
        });

        // Mine pending transactions
        server->get("/api/mine", [this](const HttpRequest& req, HttpResponse& res) {
            blockchain->minePendingTransactionsPublic();
            
            std::stringstream ss;
            ss << "{\"message\":\"Block mined successfully\",";
            ss << "\"blocks\":" << blockchain->getChainLength() << ",";
            ss << "\"pending\":" << blockchain->getPendingTransactionCount() << "}";
            res.json(ss.str());
        });
    }

    void run() {
        std::cout << "=== Bitea Social Media Blockchain ===" << std::endl;
        std::cout << "Initializing..." << std::endl;

        // Connect to databases
        if (!mongodb->connect()) {
            std::cerr << "Failed to connect to MongoDB" << std::endl;
            return;
        }

        if (!redis->connect()) {
            std::cerr << "Failed to connect to Redis" << std::endl;
            return;
        }

        std::cout << "Blockchain initialized with genesis block" << std::endl;
        std::cout << blockchain->getChainInfo() << std::endl;

        // Setup routes
        setupRoutes();

        // Start server
        server->start();
    }
};

int main() {
    BiteaApp app;
    app.run();
    return 0;
}

