#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

enum class TransactionType {
    POST,
    LIKE,
    COMMENT,
    FOLLOW,
    USER_REGISTRATION,
    TOPIC_CREATE,
    TOPIC_COMMENT,
    TOPIC_LIKE,
    TOPIC_RESHARE
};

class Transaction {
private:
    std::string id;
    std::string sender;
    TransactionType type;
    std::string data;  // JSON string containing transaction data
    time_t timestamp;

public:
    Transaction(const std::string& sender, TransactionType type, const std::string& data)
        : sender(sender), type(type), data(data) {
        timestamp = std::time(nullptr);
        generateId();
    }

    void generateId() {
        std::stringstream ss;
        ss << sender << "-" << static_cast<int>(type) << "-" << timestamp;
        id = ss.str();
    }

    std::string getId() const { return id; }
    std::string getSender() const { return sender; }
    TransactionType getType() const { return type; }
    std::string getData() const { return data; }
    time_t getTimestamp() const { return timestamp; }

    std::string toString() const {
        std::stringstream ss;
        ss << "Transaction{id=" << id 
           << ", sender=" << sender 
           << ", type=" << static_cast<int>(type)
           << ", timestamp=" << timestamp
           << ", data=" << data << "}";
        return ss.str();
    }

    // Serialize to string for hashing
    std::string serialize() const {
        std::stringstream ss;
        ss << sender << static_cast<int>(type) << timestamp << data;
        return ss.str();
    }
};

#endif // TRANSACTION_H

