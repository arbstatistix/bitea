/*******************************************************************************
 * BLOCKCHAIN.H - Distributed Ledger Chain Management
 * 
 * PURPOSE:
 * This header defines the Blockchain class, which manages a chain of linked
 * blocks forming an immutable, tamper-evident distributed ledger. It handles
 * block creation, mining, validation, and transaction management.
 * 
 * BLOCKCHAIN ARCHITECTURE:
 * The blockchain is a data structure consisting of:
 * 1. Chain: Ordered sequence of blocks linked via cryptographic hashes
 * 2. Pending Transactions: Pool of unconfirmed transactions awaiting mining
 * 3. Consensus Rules: Mining difficulty and block size limits
 * 
 * KEY CONCEPTS:
 * - IMMUTABILITY: Once added, blocks cannot be changed without detection
 * - CHAIN INTEGRITY: Each block references previous block's hash
 * - PROOF-OF-WORK: Mining provides computational proof of block validity
 * - DISTRIBUTED CONSENSUS: All nodes can independently verify chain validity
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - Block.h: Individual block implementation with mining capabilities
 * - Transaction.h: Data entries (posts, likes) stored in blocks
 * - HttpServer.h (main.cpp): Exposes blockchain via REST API
 * - Database (MongoClient): Persistent storage of blockchain state
 * 
 * REAL-WORLD ANALOGY:
 * Think of blockchain as a ledger book where:
 * - Each page (block) contains transactions
 * - Pages are numbered sequentially (index)
 * - Each page references previous page's fingerprint (hash)
 * - Pages are sealed with a difficult puzzle (Proof-of-Work)
 * - Tearing out or altering pages is immediately detectable
 * 
 * APPLICATIONS IN BITEA:
 * - Social media posts stored immutably
 * - Like/unlike actions recorded transparently
 * - User registrations tracked chronologically
 * - Content authenticity verified cryptographically
 * 
 * SECURITY MODEL:
 * - Cryptographic linking prevents historical tampering
 * - Proof-of-Work makes forgery computationally expensive
 * - Public verifiability enables trustless validation
 * - No central authority needed for data integrity
 * 
 * REFERENCES:
 * - Bitcoin Whitepaper: https://bitcoin.org/bitcoin.pdf (Nakamoto, 2008)
 * - "Mastering Blockchain" by Imran Bashir
 * - Ethereum Documentation: https://ethereum.org/en/developers/docs/
 ******************************************************************************/

#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <vector>      // std::vector - dynamic array for storing chain of blocks
                       // WHY: Unknown number of blocks at compile time, efficient indexed access

#include <memory>      // std::shared_ptr, std::make_shared - smart pointers for memory management
                       // WHY: Automatic memory cleanup, shared ownership of blocks, exception safety

#include <mutex>       // std::mutex, std::lock_guard - thread synchronization primitives
                       // WHY: Protects blockchain from race conditions during concurrent access

#include <iostream>    // std::cout, std::endl - console output for logging
                       // WHY: User feedback during mining, debugging information

// ============================================================================
// PROJECT-SPECIFIC INCLUDES
// ============================================================================

#include "Block.h"        // Block class - individual blockchain blocks with mining capability
#include "Transaction.h"  // Transaction class - data entries stored in blocks

// ============================================================================
// BLOCKCHAIN CLASS DEFINITION
// ============================================================================

/**
 * @class Blockchain
 * @brief Manages the chain of blocks forming the distributed ledger
 * 
 * DESIGN PATTERN: Singleton-like behavior (typically one instance per application)
 * 
 * RESPONSIBILITIES:
 * 1. Chain Management: Add blocks, validate chain integrity
 * 2. Transaction Pool: Collect pending transactions for mining
 * 3. Mining Coordination: Trigger block mining when conditions met
 * 4. Consensus Enforcement: Ensure all blocks follow difficulty rules
 * 5. Thread Safety: Protect against concurrent modifications
 * 
 * DATA STRUCTURE:
 * - Vector of shared_ptr<Block>: Maintains chronological chain
 * - Vector of Transactions: Temporary storage before mining
 * 
 * LIFECYCLE:
 * 1. Construction: Create genesis block (first block, index 0)
 * 2. Operation: Accept transactions, mine blocks periodically
 * 3. Validation: Continuously verify chain integrity
 * 
 * THREAD SAFETY:
 * Uses std::mutex to serialize access to chain and pending transactions
 * Multiple threads can safely add transactions and mine blocks
 */
class Blockchain {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (Blockchain State)
    // ========================================================================
    
    /**
     * @brief The ordered sequence of blocks forming the blockchain
     * @type std::vector<std::shared_ptr<Block>> - Dynamic array of smart pointers to blocks
     * 
     * PURPOSE: Stores the complete history of all mined blocks
     * 
     * WHY std::vector:
     * - Dynamic sizing (grows as new blocks are added)
     * - Fast indexed access: O(1) to access any block by index
     * - Contiguous memory for cache efficiency
     * - Standard library support (iterators, algorithms)
     * 
     * WHY std::shared_ptr:
     * - Automatic memory management (no manual delete needed)
     * - Shared ownership (multiple references to same block possible)
     * - Exception safety (prevents memory leaks)
     * - nullptr safety (can check validity)
     * 
     * ALTERNATIVE CONSIDERED: std::unique_ptr
     * Shared_ptr chosen because blocks may be referenced by:
     * - The chain vector
     * - API responses being prepared
     * - Database serialization threads
     * 
     * CHAIN INVARIANT:
     * - chain[0] is always the genesis block
     * - chain[i].previousHash == chain[i-1].hash (for i > 0)
     * - chain.size() >= 1 (always has genesis block)
     * 
     * GROWTH PATTERN:
     * Starts with 1 block (genesis), grows unbounded
     * Production systems might implement pruning or archival
     */
    std::vector<std::shared_ptr<Block>> chain;
    
    /**
     * @brief Pool of transactions waiting to be included in a block
     * @type std::vector<Transaction> - Dynamic array of transaction objects
     * 
     * PURPOSE: Temporary storage for transactions before they're mined into blocks
     * 
     * TRANSACTION LIFECYCLE:
     * 1. User action (post, like) creates Transaction
     * 2. addTransaction() adds to pendingTransactions
     * 3. When count reaches maxTransactionsPerBlock, mining triggered
     * 4. minePendingTransactions() moves transactions from pending → block
     * 5. Successfully mined transactions removed from pending
     * 
     * WHY std::vector (not queue/deque):
     * - Need ability to take arbitrary number of transactions (batch)
     * - May implement priority ordering in future (high fees first)
     * - Erase range operation useful for removing mined transactions
     * 
     * CONCURRENCY:
     * Protected by chainMutex to prevent race conditions
     * Multiple threads may call addTransaction() simultaneously
     * 
     * SIZE MANAGEMENT:
     * Auto-triggers mining when reaches maxTransactionsPerBlock
     * Prevents unbounded growth of pending pool
     */
    std::vector<Transaction> pendingTransactions;
    
    /**
     * @brief Number of leading zeros required in block hashes (Proof-of-Work difficulty)
     * @type int (typically 1-10 for testing, 15-25 for production blockchains)
     * 
     * PURPOSE: Controls mining computational cost and block creation rate
     * 
     * DIFFICULTY EXPLAINED:
     * Higher value = more zeros required = harder mining
     * difficulty=4: hash must be "0000..." (~65K attempts)
     * difficulty=5: hash must be "00000..." (~1M attempts)
     * difficulty=6: hash must be "000000..." (~16M attempts)
     * 
     * SECURITY IMPLICATIONS:
     * - Higher difficulty = harder to attack (51% attack more expensive)
     * - Lower difficulty = faster blocks but easier to forge
     * 
     * BITCOIN COMPARISON:
     * Bitcoin adjusts difficulty every 2016 blocks to maintain ~10 min block time
     * Our implementation: Fixed difficulty (could be made adaptive)
     * 
     * PASSED TO BLOCKS:
     * Each Block constructor receives this value
     * All blocks in chain should ideally use same difficulty (consensus rule)
     * 
     * DEFAULT: Set in constructor (typically 4 for development)
     */
    int difficulty;
    
    /**
     * @brief Reward given to miners for successfully mining a block (currently unused)
     * @type int (arbitrary reward units)
     * 
     * PURPOSE: Incentive mechanism for miners in proof-of-work systems
     * 
     * CRYPTOCURRENCY CONTEXT:
     * In Bitcoin: Miners receive new coins + transaction fees
     * In our system: Currently not implemented (reward mechanism planned)
     * 
     * POTENTIAL FUTURE USE:
     * - Reputation points for users who contribute to network
     * - Token rewards in a cryptocurrency implementation
     * - Gamification incentives for network participation
     * 
     * WHY int: Simple integer reward, could be changed to double/float for decimals
     * 
     * CURRENT STATUS: Defined but not actively used in mining logic
     * 
     * NOTE: Intentionally unused in current implementation - reserved for future
     * cryptocurrency or reputation system features.
     */
    [[maybe_unused]] int miningReward;
    
    /**
     * @brief Mutex for thread-safe access to blockchain state
     * @type std::mutex (mutual exclusion lock)
     * 
     * PURPOSE: Prevents race conditions when multiple threads access blockchain
     * 
     * PROTECTED OPERATIONS:
     * - Adding transactions to pendingTransactions
     * - Mining blocks and adding to chain
     * - Reading chain state for API responses
     * 
     * WHY NEEDED:
     * Web server (HttpServer.h) handles multiple concurrent requests:
     * - Thread 1: User A creates post → addTransaction()
     * - Thread 2: User B likes post → addTransaction()
     * - Thread 3: Admin triggers mining → minePendingTransactions()
     * Without mutex, these could corrupt blockchain state
     * 
     * USAGE PATTERN:
     * std::lock_guard<std::mutex> lock(chainMutex);
     * - Automatically locks mutex on construction
     * - Automatically unlocks on destruction (RAII pattern)
     * - Exception-safe (unlocks even if exception thrown)
     * 
     * LOCKING STRATEGY:
     * - Coarse-grained locking (one mutex for entire blockchain)
     * - Simple but may create contention under high load
     * - Could be optimized with reader-writer locks or finer-grained locking
     * 
     * DEADLOCK PREVENTION:
     * Only one mutex used, so no deadlock possible
     * Lock held for minimal time (avoid long operations while locked)
     */
    std::mutex chainMutex;
    
    /**
     * @brief Maximum number of transactions allowed per block
     * @type int (configurable limit)
     * 
     * PURPOSE: Controls block size and triggers automatic mining
     * 
     * WHY LIMIT BLOCK SIZE:
     * - Prevents blocks from growing too large
     * - Ensures predictable mining time
     * - Facilitates network transmission (smaller blocks)
     * - Regular block creation for timely transaction confirmation
     * 
     * AUTO-MINING TRIGGER:
     * When pendingTransactions.size() >= maxTransactionsPerBlock:
     * → Automatically calls minePendingTransactions()
     * → Creates block with up to maxTransactionsPerBlock transactions
     * → Remaining transactions stay pending for next block
     * 
     * TYPICAL VALUES:
     * - Development/Testing: 10 (fast block creation)
     * - Production: 100-1000 (balance between throughput and latency)
     * - Bitcoin: ~2000-3000 transactions per block (varies with tx size)
     * 
     * DEFAULT: Set to 10 in constructor (configurable parameter)
     * 
     * RELATIONSHIP TO THROUGHPUT:
     * With maxTx=10 and 1 minute mining time:
     * Throughput = 10 tx/min = 0.167 tx/second
     * Can be scaled by adjusting this parameter or mining frequency
     */
    int maxTransactionsPerBlock;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Creates the first block in the blockchain (genesis block)
     * @return std::shared_ptr<Block> - Smart pointer to the mined genesis block
     * 
     * PURPOSE: Initializes the blockchain with a special first block
     * 
     * GENESIS BLOCK CHARACTERISTICS:
     * - Index: 0 (first block in chain)
     * - Previous Hash: "0" (no previous block exists)
     * - Transactions: Single system transaction marking blockchain creation
     * - Mined: Yes (undergoes Proof-of-Work like regular blocks)
     * 
     * WHY GENESIS BLOCK IS SPECIAL:
     * - Hard-coded in blockchain (not received from network)
     * - Provides anchor point for entire chain
     * - All subsequent blocks trace back to this block
     * - Defines initial state of the system
     * 
     * BITCOIN GENESIS BLOCK:
     * Contains message: "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks"
     * Our genesis: Contains metadata about Bitea blockchain
     * 
     * TRANSACTION CREATION:
     * emplace_back(): Constructs Transaction in-place (efficient, no copy)
     * Parameters: ("SYSTEM", USER_REGISTRATION, JSON_data)
     * - Sender: "SYSTEM" (special identifier for protocol-generated transactions)
     * - Type: USER_REGISTRATION (marks system initialization)
     * - Data: JSON with descriptive message
     * 
     * MINING:
     * Genesis block is mined before being returned
     * Ensures it meets same Proof-of-Work standards as other blocks
     * 
     * std::make_shared:
     * Creates shared_ptr and Block object in single allocation (efficient)
     * Alternative: std::shared_ptr<Block>(new Block(...)) (less efficient)
     * 
     * CALLED BY: Constructor (once during blockchain initialization)
     */
    std::shared_ptr<Block> createGenesisBlock() {
        // Create vector to hold genesis transactions
        std::vector<Transaction> genesisTxs;
        
        // Add system transaction marking blockchain creation
        // emplace_back constructs Transaction in-place (efficient)
        genesisTxs.emplace_back("SYSTEM", TransactionType::USER_REGISTRATION, 
                                "{\"message\":\"Genesis Block - Bitea Social Media Blockchain\"}");
        
        // Create genesis block: index=0, previousHash="0", with system transaction
        auto block = std::make_shared<Block>(0, "0", genesisTxs, difficulty);
        
        // Mine the genesis block (performs Proof-of-Work)
        block->mineBlock();
        
        return block;  // Return mined genesis block
    }

    // ========================================================================
    // PUBLIC INTERFACE (Constructor and Methods)
    // ========================================================================

public:
    /**
     * @brief Constructs a new Blockchain with genesis block
     * @param difficulty Number of leading zeros required for mining (default: 4)
     * @param maxTxPerBlock Maximum transactions per block (default: 10)
     * 
     * PURPOSE: Initializes blockchain with configuration parameters
     * 
     * INITIALIZATION SEQUENCE:
     * 1. Set difficulty (mining hardness)
     * 2. Set miningReward to 100 (currently unused)
     * 3. Set maxTransactionsPerBlock (block size limit)
     * 4. Create and mine genesis block
     * 5. Add genesis block to chain
     * 
     * MEMBER INITIALIZER LIST:
     * : difficulty(difficulty), miningReward(100), maxTransactionsPerBlock(maxTxPerBlock)
     * Initializes members before constructor body executes
     * More efficient than assignment in body
     * 
     * DEFAULT PARAMETERS:
     * difficulty = 4: Reasonable for development (quick mining)
     * maxTxPerBlock = 10: Small blocks for testing
     * 
     * Can be called as:
     * - Blockchain() → Uses defaults (difficulty=4, maxTx=10)
     * - Blockchain(6) → Custom difficulty, default maxTx
     * - Blockchain(6, 20) → Custom difficulty and maxTx
     * 
     * PARAMETER CHOICE GUIDE:
     * LOW DIFFICULTY (1-3):
     * - Fast mining (~milliseconds)
     * - Good for development/testing
     * - Low security (easy to attack)
     * 
     * MEDIUM DIFFICULTY (4-6):
     * - Moderate mining (~seconds)
     * - Good for demos and local testing
     * - Moderate security
     * 
     * HIGH DIFFICULTY (7+):
     * - Slow mining (~minutes to hours)
     * - Production-grade security
     * - Not practical for development
     * 
     * CHAIN STATE AFTER CONSTRUCTION:
     * - chain.size() == 1 (genesis block only)
     * - pendingTransactions.size() == 0 (no pending transactions)
     * - Blockchain is valid (passes isChainValid())
     * 
     * THREAD SAFETY:
     * Constructor not thread-safe (shouldn't be called concurrently)
     * Typically called once during application startup
     * 
     * EXCEPTION SAFETY:
     * If createGenesisBlock() throws, blockchain is not created (safe)
     * Vector operations may throw std::bad_alloc (rare)
     */
    Blockchain(int difficulty = 4, int maxTxPerBlock = 10) 
        : difficulty(difficulty), miningReward(100), maxTransactionsPerBlock(maxTxPerBlock) {
        // Create genesis block and add to chain
        // This is the only block in chain after construction
        chain.push_back(createGenesisBlock());
    }

    /**
     * @brief Adds a new transaction to the pending pool
     * @param transaction Transaction to add (passed by const reference)
     * @return void
     * 
     * PURPOSE: Accepts user actions (posts, likes) and queues them for mining
     * 
     * THREAD SAFETY:
     * std::lock_guard<std::mutex> lock(chainMutex);
     * - Acquires exclusive lock on entry
     * - Releases lock on function exit (RAII pattern)
     * - Prevents concurrent modification of pendingTransactions
     * 
     * PARAMETER PASSING:
     * const Transaction&: Pass by const reference
     * - Avoids copying entire Transaction object (efficient)
     * - const: Guarantees we won't modify caller's transaction
     * - Alternative: Pass by value (would copy), or rvalue reference (move semantics)
     * 
     * AUTO-MINING TRIGGER:
     * When pendingTransactions.size() >= maxTransactionsPerBlock:
     * → Automatically calls minePendingTransactions()
     * → Creates new block with pending transactions
     * → Provides regular block creation without manual intervention
     * 
     * USAGE FLOW:
     * 1. User creates post via API
     * 2. HttpServer creates Transaction object
     * 3. Calls blockchain.addTransaction(tx)
     * 4. Transaction added to pending pool
     * 5. If pool full → auto-mine new block
     * 6. Transaction confirmed and immutable in blockchain
     * 
     * size_t vs int:
     * pendingTransactions.size() returns size_t (unsigned)
     * maxTransactionsPerBlock is int (signed)
     * Comparison works due to implicit conversion
     * 
     * CALLED BY:
     * - main.cpp: HTTP endpoint handlers (POST /api/posts, /api/likes, etc.)
     * - Manual transaction injection for testing
     * 
     * ALTERNATIVE DESIGN:
     * Could return transaction ID or confirmation status
     * Current: void (fire-and-forget, transaction always accepted)
     */
    void addTransaction(const Transaction& transaction) {
        // Acquire lock to protect pendingTransactions from concurrent access
        std::lock_guard<std::mutex> lock(chainMutex);
        
        // Add transaction to pending pool
        pendingTransactions.push_back(transaction);
        
        // Auto-mine if we have enough pending transactions
        // size_t (unsigned) >= int (signed) comparison works via implicit conversion
        if (pendingTransactions.size() >= static_cast<size_t>(maxTransactionsPerBlock)) {
            minePendingTransactions();  // Trigger block creation
        }
    }

    /**
     * @brief Mines pending transactions into a new block and adds to chain
     * @return void
     * 
     * PURPOSE: Core mining operation that creates blocks from transaction pool
     * 
     * PRECONDITION:
     * - Caller must hold chainMutex lock (internal method called by addTransaction)
     * - OR called via minePendingTransactionsPublic() which acquires lock
     * 
     * ALGORITHM:
     * 1. Check if pending transactions exist (early return if none)
     * 2. Select up to maxTransactionsPerBlock transactions from pending pool
     * 3. Create new Block with selected transactions
     * 4. Perform Proof-of-Work mining (computationally expensive)
     * 5. Add mined block to chain
     * 6. Remove mined transactions from pending pool
     * 
     * BATCH SIZE CALCULATION:
     * txCount = std::min(maxTransactionsPerBlock, pendingTransactions.size())
     * - Takes up to maxTransactionsPerBlock transactions
     * - If fewer pending, takes all available
     * - Remaining transactions stay pending for next block
     * 
     * TRANSACTION SELECTION:
     * std::vector<Transaction> blockTransactions(pendingTransactions.begin(), 
     *                                            pendingTransactions.begin() + txCount);
     * - Creates new vector with first txCount transactions
     * - Iterator range: [begin, begin+txCount)
     * - Transactions copied from pending pool
     * 
     * BLOCK CREATION:
     * - Index: chain.size() (next sequential index)
     * - PreviousHash: getLatestBlock()->getHash() (links to last block)
     * - Transactions: blockTransactions (selected from pending)
     * - Difficulty: Global difficulty setting
     * 
     * MINING:
     * newBlock->mineBlock(): Performs Proof-of-Work
     * - CPU-intensive operation (seconds to minutes depending on difficulty)
     * - Mutex held during mining (blocks other operations)
     * - Alternative: Release lock during mining (more complex)
     * 
     * CHAIN UPDATE:
     * chain.push_back(newBlock): Adds mined block to end of chain
     * - Atomic operation (vector guarantees strong exception safety)
     * - Chain grows sequentially (no gaps in indices)
     * 
     * CLEANUP:
     * pendingTransactions.erase(begin, begin+txCount): Removes mined transactions
     * - Erases range [begin, begin+txCount)
     * - Remaining transactions shift to front
     * - O(n) operation where n = remaining transactions
     * 
     * LOGGING:
     * - Reports mining start with block index and transaction count
     * - Reports successful mining with resulting hash
     * - Useful for monitoring and debugging
     * 
     * CALLED BY:
     * - addTransaction(): Auto-triggers when pending pool reaches limit
     * - minePendingTransactionsPublic(): Manual mining via API
     * 
     * THREAD SAFETY NOTE:
     * Assumes caller holds lock! Does not acquire lock itself.
     * This design allows efficient calling from addTransaction (already locked).
     */
    void minePendingTransactions() {
        // Early return if no transactions to mine
        if (pendingTransactions.empty()) {
            std::cout << "No pending transactions to mine." << std::endl;
            return;
        }

        // Calculate how many transactions to include in this block
        // Take minimum of: max allowed, and actual pending count
        size_t txCount = std::min(static_cast<size_t>(maxTransactionsPerBlock), 
                                  pendingTransactions.size());
        
        // Create vector with first txCount transactions from pending pool
        // Range constructor: copies elements from [begin, begin+txCount)
        std::vector<Transaction> blockTransactions(pendingTransactions.begin(), 
                                                    pendingTransactions.begin() + txCount);
        
        // Create new block:
        // - Index: chain.size() (next sequential position)
        // - PrevHash: hash of current last block (creates chain link)
        // - Transactions: selected transactions
        // - Difficulty: mining difficulty setting
        auto newBlock = std::make_shared<Block>(
            chain.size(),                    // Block index
            getLatestBlock()->getHash(),     // Previous block's hash
            blockTransactions,               // Transactions to include
            difficulty                       // Mining difficulty
        );
        
        // Log mining start for monitoring
        std::cout << "Mining block " << newBlock->getIndex() << " with " 
                  << blockTransactions.size() << " transactions..." << std::endl;
        
        // Perform Proof-of-Work mining (CPU-intensive, may take time)
        newBlock->mineBlock();
        
        // Add successfully mined block to the end of the chain
        chain.push_back(newBlock);
        
        // Remove mined transactions from pending pool
        // Erase range: [begin, begin+txCount)
        pendingTransactions.erase(pendingTransactions.begin(), 
                                  pendingTransactions.begin() + txCount);
        
        // Log successful mining with resulting hash
        std::cout << "Block mined successfully! Hash: " << newBlock->getHash() << std::endl;
    }

    /**
     * @brief Public wrapper for mining with thread-safe locking
     * @return void
     * 
     * PURPOSE: Allows external callers to trigger mining safely
     * 
     * DIFFERENCE FROM minePendingTransactions():
     * - This method: Acquires lock, then calls mining logic
     * - minePendingTransactions(): Assumes caller holds lock
     * 
     * WHY TWO METHODS:
     * - addTransaction() already holds lock → calls minePendingTransactions() directly
     * - External callers (API) need thread safety → call this method
     * 
     * USAGE:
     * - HTTP endpoint: POST /api/blockchain/mine
     * - Admin interface: Manual block creation
     * - Testing: Trigger mining on demand
     * 
     * THREAD SAFETY:
     * std::lock_guard automatically locks chainMutex for entire scope
     */
    void minePendingTransactionsPublic() {
        std::lock_guard<std::mutex> lock(chainMutex);  // Acquire lock
        minePendingTransactions();                     // Call internal mining logic
    }

    /**
     * @brief Returns pointer to the most recently added block
     * @return std::shared_ptr<Block> - Smart pointer to latest block
     * 
     * PURPOSE: Access the last block in the chain (needed for creating next block)
     * 
     * IMPLEMENTATION:
     * chain.back(): Returns reference to last element in vector
     * - O(1) operation (constant time)
     * - Undefined behavior if chain is empty (but chain always has genesis)
     * 
     * WHY const:
     * Read-only operation, doesn't modify blockchain
     * Can be called on const Blockchain objects
     * 
     * USAGE:
     * - Creating new block: Need previous block's hash
     * - API queries: Show latest block info
     * - Validation: Check most recent block
     * 
     * INVARIANT:
     * chain.size() >= 1 (genesis block always present)
     * So chain.back() is always safe to call
     */
    std::shared_ptr<Block> getLatestBlock() const {
        return chain.back();  // Return last block in chain
    }

    /**
     * @brief Validates entire blockchain integrity
     * @return bool - true if chain is valid, false if corruption detected
     * 
     * PURPOSE: Ensures no blocks have been tampered with
     * 
     * VALIDATION CHECKS:
     * For each block (starting from index 1, skipping genesis):
     * 1. Block has valid Proof-of-Work (hash starts with required zeros)
     * 2. Block's stored hash matches recalculated hash (data not tampered)
     * 3. Block's previousHash matches previous block's hash (chain link intact)
     * 
     * ALGORITHM:
     * - Iterate from index 1 to end (genesis block unchecked - hard-coded)
     * - For each block, validate using Block::isValid()
     * - Verify chain link: currentBlock.previousHash == previousBlock.hash
     * - Return false immediately if any check fails
     * - Return true if all blocks pass
     * 
     * WHY START AT INDEX 1:
     * Genesis block (index 0) has no previous block, so skip it
     * Genesis block is hard-coded and trusted by definition
     * 
     * DETECTING ATTACKS:
     * SCENARIO 1: Attacker modifies transaction in block 5
     * - Block 5 hash changes (due to SHA-256 avalanche effect)
     * - Block 5 isValid() fails (stored hash ≠ recalculated hash)
     * - Detected!
     * 
     * SCENARIO 2: Attacker modifies block 5 and recalculates its hash
     * - Block 5 hash now correct, but different from original
     * - Block 6 previousHash still points to old block 5 hash
     * - Chain link broken: Block 6 previousHash ≠ Block 5 new hash
     * - Detected!
     * 
     * SCENARIO 3: Attacker modifies block 5 and re-mines blocks 5,6,7...
     * - Requires enormous computational power (re-mine all subsequent blocks)
     * - This is the "51% attack" (need majority of network hash power)
     * - Prevented by Proof-of-Work difficulty
     * 
     * EFFICIENCY: O(n*m) where n = blocks, m = avg transactions per block
     * Each isValid() call recalculates hash, which processes all transactions
     * 
     * WHY const:
     * Read-only validation, doesn't modify blockchain state
     * 
     * USAGE:
     * - API endpoint: GET /api/blockchain/validate
     * - System health checks
     * - After receiving blockchain from network
     * - Periodic integrity audits
     */
    bool isChainValid() const {
        // Validate each block starting from index 1 (skip genesis)
        for (size_t i = 1; i < chain.size(); i++) {
            const auto& currentBlock = chain[i];    // Block being validated
            const auto& previousBlock = chain[i - 1];  // Previous block in chain

            // CHECK 1: Verify block's Proof-of-Work and data integrity
            if (!currentBlock->isValid()) {
                std::cout << "Block " << i << " has invalid hash." << std::endl;
                return false;  // Block failed validation
            }

            // CHECK 2: Verify chain link (previousHash matches previous block's hash)
            if (currentBlock->getPreviousHash() != previousBlock->getHash()) {
                std::cout << "Block " << i << " has invalid previous hash." << std::endl;
                return false;  // Chain link broken
            }
        }
        
        // All blocks passed validation
        return true;
    }

    // ========================================================================
    // GETTER METHODS (Public Read Access)
    // ========================================================================
    
    /**
     * @brief Returns const reference to the entire chain
     * @return const std::vector<std::shared_ptr<Block>>& - Chain reference
     * WHY const&: Avoids copying entire chain, prevents external modification
     * USAGE: API serialization, analytics, blockchain exploration
     */
    const std::vector<std::shared_ptr<Block>>& getChain() const {
        return chain;
    }

    /**
     * @brief Returns const reference to pending transactions pool
     * @return const std::vector<Transaction>& - Pending transactions reference
     * WHY const&: Avoids copying, prevents external modification
     * USAGE: API queries, monitoring transaction pool, fee estimation
     */
    const std::vector<Transaction>& getPendingTransactions() const {
        return pendingTransactions;
    }

    /**
     * @brief Returns number of blocks in the chain
     * @return size_t - Block count (unsigned integer)
     * WHY size_t: Matches vector::size() return type, can't be negative
     * USAGE: Blockchain height queries, sync progress
     */
    size_t getChainLength() const {
        return chain.size();
    }

    /**
     * @brief Returns number of pending transactions
     * @return int - Pending transaction count
     * WHY int: Simple integer type (could be size_t for consistency)
     * USAGE: Monitoring transaction pool, deciding when to mine
     */
    int getPendingTransactionCount() const {
        return pendingTransactions.size();
    }

    /**
     * @brief Generates human-readable blockchain summary
     * @return std::string - Formatted blockchain statistics
     * 
     * OUTPUT FORMAT:
     * Blockchain Info:
     *   Blocks: 42
     *   Pending Transactions: 7
     *   Difficulty: 4
     *   Valid: Yes
     * 
     * USAGE: Dashboard display, logging, monitoring
     * VALIDATION: Calls isChainValid() (expensive for large chains)
     */
    std::string getChainInfo() const {
        std::stringstream ss;
        ss << "Blockchain Info:" << std::endl
           << "  Blocks: " << chain.size() << std::endl
           << "  Pending Transactions: " << pendingTransactions.size() << std::endl
           << "  Difficulty: " << difficulty << std::endl
           << "  Valid: " << (isChainValid() ? "Yes" : "No") << std::endl;
        return ss.str();
    }

    /**
     * @brief Prints all blocks in the chain to console
     * @return void
     * 
     * PURPOSE: Debugging and visualization of entire blockchain
     * 
     * IMPLEMENTATION:
     * Iterates through all blocks, calling toString() on each
     * 
     * USAGE:
     * - Development debugging
     * - Command-line blockchain explorer
     * - Demonstrating blockchain concept
     * 
     * PERFORMANCE: O(n) where n = number of blocks
     * Can be slow for large blockchains
     */
    void printChain() const {
        for (const auto& block : chain) {
            std::cout << block->toString() << std::endl;
        }
    }
};

// ============================================================================
// END OF BLOCKCHAIN CLASS
// ============================================================================

#endif // BLOCKCHAIN_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * ARCHITECTURE DECISIONS:
 * - Single-threaded mining: Simplifies implementation, holds lock during mining
 * - Auto-mining: Blocks created automatically when pending pool fills
 * - Fixed difficulty: All blocks use same difficulty (could be adaptive)
 * - No pruning: Chain grows unbounded (production might implement pruning)
 * 
 * MEMORY MANAGEMENT:
 * - shared_ptr for blocks: Enables safe sharing with API handlers
 * - Vectors manage memory automatically
 * - No manual new/delete needed
 * 
 * THREAD SAFETY:
 * - Coarse-grained locking: One mutex for entire blockchain
 * - Simple but may cause contention under high load
 * - Alternative: Reader-writer locks, finer-grained locking
 * 
 * SCALABILITY CONSIDERATIONS:
 * - Validation is O(n*m): Expensive for large chains
 * - Could cache validation results
 * - Consider incremental validation (only new blocks)
 * - Implement checkpointing for long chains
 * 
 * SECURITY CONSIDERATIONS:
 * - Proof-of-Work prevents easy forgery
 * - Chain validation detects tampering
 * - No network layer (single-node implementation)
 * - No consensus mechanism (would need for distributed system)
 * 
 * POTENTIAL IMPROVEMENTS:
 * - Merkle tree for efficient transaction verification
 * - Adaptive difficulty adjustment
 * - Transaction priority queue (fee-based)
 * - Parallel mining (release lock during mining)
 * - Network layer for distributed consensus
 * - Database persistence (current: in-memory only)
 * - Chain pruning/archival for old blocks
 * 
 * COMPARISON TO BITCOIN:
 * - Similar: Proof-of-Work, chain validation, transaction pool
 * - Different: No network, no UTXO model, simpler transaction types
 * - Purpose: Educational/demo vs. production cryptocurrency
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: Block creation, mining, validation
 * - Integration tests: Transaction flow, concurrent access
 * - Performance tests: Mining time vs. difficulty
 * - Security tests: Tampering detection, chain validation
 ******************************************************************************/
