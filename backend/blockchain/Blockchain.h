#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <vector>
#include <memory>
#include <mutex>
#include <iostream>
#include "Block.h"
#include "Transaction.h"

class Blockchain {
private:
    std::vector<std::shared_ptr<Block>> chain;
    std::vector<Transaction> pendingTransactions;
    int difficulty;
    int miningReward;
    std::mutex chainMutex;
    int maxTransactionsPerBlock;

    std::shared_ptr<Block> createGenesisBlock() {
        std::vector<Transaction> genesisTxs;
        genesisTxs.emplace_back("SYSTEM", TransactionType::USER_REGISTRATION, 
                                "{\"message\":\"Genesis Block - Bitea Social Media Blockchain\"}");
        auto block = std::make_shared<Block>(0, "0", genesisTxs, difficulty);
        block->mineBlock();
        return block;
    }

public:
    Blockchain(int difficulty = 4, int maxTxPerBlock = 10) 
        : difficulty(difficulty), miningReward(100), maxTransactionsPerBlock(maxTxPerBlock) {
        chain.push_back(createGenesisBlock());
    }

    void addTransaction(const Transaction& transaction) {
        std::lock_guard<std::mutex> lock(chainMutex);
        pendingTransactions.push_back(transaction);
        
        // Auto-mine if we have enough pending transactions
        if (pendingTransactions.size() >= maxTransactionsPerBlock) {
            minePendingTransactions();
        }
    }

    void minePendingTransactions() {
        // Already locked by caller or lock here if called directly
        if (pendingTransactions.empty()) {
            std::cout << "No pending transactions to mine." << std::endl;
            return;
        }

        // Take up to maxTransactionsPerBlock transactions
        size_t txCount = std::min(static_cast<size_t>(maxTransactionsPerBlock), pendingTransactions.size());
        std::vector<Transaction> blockTransactions(pendingTransactions.begin(), 
                                                    pendingTransactions.begin() + txCount);
        
        auto newBlock = std::make_shared<Block>(
            chain.size(), 
            getLatestBlock()->getHash(), 
            blockTransactions,
            difficulty
        );
        
        std::cout << "Mining block " << newBlock->getIndex() << " with " 
                  << blockTransactions.size() << " transactions..." << std::endl;
        
        newBlock->mineBlock();
        chain.push_back(newBlock);
        
        // Remove mined transactions from pending
        pendingTransactions.erase(pendingTransactions.begin(), pendingTransactions.begin() + txCount);
        
        std::cout << "Block mined successfully! Hash: " << newBlock->getHash() << std::endl;
    }

    void minePendingTransactionsPublic() {
        std::lock_guard<std::mutex> lock(chainMutex);
        minePendingTransactions();
    }

    std::shared_ptr<Block> getLatestBlock() const {
        return chain.back();
    }

    bool isChainValid() const {
        for (size_t i = 1; i < chain.size(); i++) {
            const auto& currentBlock = chain[i];
            const auto& previousBlock = chain[i - 1];

            // Check if block is valid (proof of work)
            if (!currentBlock->isValid()) {
                std::cout << "Block " << i << " has invalid hash." << std::endl;
                return false;
            }

            // Check if previous hash matches
            if (currentBlock->getPreviousHash() != previousBlock->getHash()) {
                std::cout << "Block " << i << " has invalid previous hash." << std::endl;
                return false;
            }
        }
        return true;
    }

    const std::vector<std::shared_ptr<Block>>& getChain() const {
        return chain;
    }

    const std::vector<Transaction>& getPendingTransactions() const {
        return pendingTransactions;
    }

    size_t getChainLength() const {
        return chain.size();
    }

    int getPendingTransactionCount() const {
        return pendingTransactions.size();
    }

    std::string getChainInfo() const {
        std::stringstream ss;
        ss << "Blockchain Info:" << std::endl
           << "  Blocks: " << chain.size() << std::endl
           << "  Pending Transactions: " << pendingTransactions.size() << std::endl
           << "  Difficulty: " << difficulty << std::endl
           << "  Valid: " << (isChainValid() ? "Yes" : "No") << std::endl;
        return ss.str();
    }

    void printChain() const {
        for (const auto& block : chain) {
            std::cout << block->toString() << std::endl;
        }
    }
};

#endif // BLOCKCHAIN_H

