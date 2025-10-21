#ifndef BLOCK_H
#define BLOCK_H

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include "Transaction.h"

class Block {
private:
    int index;
    std::string previousHash;
    std::string hash;
    time_t timestamp;
    std::vector<Transaction> transactions;
    int nonce;
    int difficulty;

    std::string calculateHash() const {
        std::stringstream ss;
        ss << index << previousHash << timestamp << transactionsToString() << nonce;
        return sha256(ss.str());
    }

    std::string sha256(const std::string& data) const {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.c_str(), data.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    std::string transactionsToString() const {
        std::stringstream ss;
        for(const auto& tx : transactions) {
            ss << tx.serialize();
        }
        return ss.str();
    }

public:
    Block(int index, const std::string& previousHash, const std::vector<Transaction>& transactions, int difficulty = 4)
        : index(index), previousHash(previousHash), transactions(transactions), difficulty(difficulty) {
        timestamp = std::time(nullptr);
        nonce = 0;
        hash = calculateHash();
    }

    void mineBlock() {
        std::string target(difficulty, '0');
        
        do {
            nonce++;
            hash = calculateHash();
        } while (hash.substr(0, difficulty) != target);
        
        // std::cout << "Block mined: " << hash << std::endl;
    }

    bool isValid() const {
        std::string target(difficulty, '0');
        return hash.substr(0, difficulty) == target && hash == calculateHash();
    }

    // Getters
    int getIndex() const { return index; }
    std::string getHash() const { return hash; }
    std::string getPreviousHash() const { return previousHash; }
    time_t getTimestamp() const { return timestamp; }
    const std::vector<Transaction>& getTransactions() const { return transactions; }
    int getNonce() const { return nonce; }
    int getDifficulty() const { return difficulty; }

    std::string toString() const {
        std::stringstream ss;
        ss << "Block #" << index << " [" << std::endl
           << "  Hash: " << hash << std::endl
           << "  Previous Hash: " << previousHash << std::endl
           << "  Timestamp: " << timestamp << std::endl
           << "  Nonce: " << nonce << std::endl
           << "  Transactions: " << transactions.size() << std::endl
           << "]";
        return ss.str();
    }
};

#endif // BLOCK_H

