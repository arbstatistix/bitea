/*******************************************************************************
 * BLOCK.H - Core Blockchain Block Implementation
 * 
 * PURPOSE:
 * This header file defines the Block class, which is the fundamental building
 * block of the blockchain data structure. Each Block contains a collection of
 * transactions and cryptographic hashes that link it to the previous block,
 * forming an immutable chain.
 * 
 * BLOCKCHAIN ARCHITECTURE CONTEXT:
 * This Block class is used by the Blockchain.h class to create a linked chain
 * of blocks. The chain integrity is maintained through:
 * 1. Each block storing the hash of the previous block
 * 2. Proof-of-Work (PoW) mining mechanism
 * 3. Cryptographic hash validation
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - Transaction.h: Contains transaction data that is stored in blocks
 * - Blockchain.h: Uses Block objects to form the blockchain
 * - HttpServer.h: Serves block data through REST API endpoints
 * - main.cpp: Initializes and manages the blockchain with blocks
 * 
 * SECURITY FEATURES:
 * - SHA-256 cryptographic hashing (collision-resistant, one-way function)
 * - Proof-of-Work mining to prevent tampering
 * - Hash chain verification for immutability
 * 
 * REFERENCES:
 * - Bitcoin Whitepaper: https://bitcoin.org/bitcoin.pdf (Nakamoto, 2008)
 * - OpenSSL SHA-256: https://www.openssl.org/docs/man1.1.1/man3/SHA256.html
 * - Blockchain Fundamentals: "Mastering Bitcoin" by Andreas M. Antonopoulos
 ******************************************************************************/

#ifndef BLOCK_H
#define BLOCK_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>      // For std::string - used for hash values and serialization
#include <vector>      // For std::vector - dynamic array to store variable number of transactions
#include <ctime>       // For time_t and std::time() - timestamp generation (Unix epoch time)
#include <sstream>     // For std::stringstream - efficient string concatenation for hashing
#include <iomanip>     // For std::hex, std::setw, std::setfill - hexadecimal hash formatting

// ============================================================================
// EXTERNAL LIBRARY INCLUDES
// ============================================================================

#include <openssl/sha.h>  // OpenSSL SHA-256 implementation - industry-standard cryptographic hash
                           // WHY OpenSSL: Battle-tested, FIPS 140-2 validated, optimized for performance

// ============================================================================
// PROJECT-SPECIFIC INCLUDES
// ============================================================================

#include "Transaction.h"   // Transaction class - represents individual data entries (posts, likes, etc.)
                           // Each block can contain multiple transactions bundled together

// ============================================================================
// BLOCK CLASS DEFINITION
// ============================================================================

/**
 * @class Block
 * @brief Represents a single block in the blockchain containing transactions and metadata
 * 
 * DESIGN PATTERN: Value Object with Immutability Characteristics
 * 
 * A Block is a container that packages together:
 * 1. Data (transactions)
 * 2. Metadata (index, timestamp, nonce)
 * 3. Cryptographic proofs (hash, previousHash)
 * 
 * RELATIONSHIP TO BLOCKCHAIN:
 * Blocks are linked through the previousHash field, creating a tamper-evident chain.
 * If any block is modified, its hash changes, breaking the chain and making
 * tampering immediately detectable.
 * 
 * LIFECYCLE:
 * 1. Creation: Block is initialized with transactions and previousHash
 * 2. Mining: mineBlock() performs Proof-of-Work to find valid nonce
 * 3. Validation: isValid() verifies hash meets difficulty requirements
 * 4. Storage: Block is added to the blockchain chain (in Blockchain.h)
 */
class Block {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (Block Metadata & Data)
    // ========================================================================
    
    /**
     * @brief Block position in the blockchain (0-indexed)
     * @type int (32-bit signed integer, sufficient for billions of blocks)
     * 
     * PURPOSE: Uniquely identifies the block's position in the chain
     * USAGE: Used in hash calculation to ensure block uniqueness
     * RELATIONSHIP: Increments sequentially; validated by Blockchain.h
     * 
     * WHY int: Sufficient range (-2B to +2B), lightweight, standard integer type
     * Bitcoin has ~800K blocks after 14 years, so int is future-proof
     */
    int index;
    
    /**
     * @brief Cryptographic hash of the previous block in the chain
     * @type std::string (64 hexadecimal characters for SHA-256)
     * 
     * PURPOSE: Creates the cryptographic link to the previous block
     * SECURITY: This is what makes blockchain tamper-evident. If any previous
     *           block is modified, its hash changes, and this reference breaks.
     * 
     * RELATIONSHIP TO BLOCKCHAIN INTEGRITY:
     * Block[N].previousHash == Block[N-1].hash
     * This forms the "chain" in blockchain, making it computationally
     * infeasible to alter historical data without detection.
     * 
     * WHY std::string: SHA-256 produces 32 bytes (256 bits), represented as
     *                  64 hexadecimal characters. String provides flexibility
     *                  for serialization and comparison.
     * 
     * GENESIS BLOCK: First block (index 0) has previousHash = "0"
     */
    std::string previousHash;
    
    /**
     * @brief Cryptographic hash of this block's contents
     * @type std::string (64 hexadecimal characters for SHA-256)
     * 
     * PURPOSE: Fingerprint of the entire block (index + previousHash + 
     *          timestamp + transactions + nonce)
     * 
     * COMPUTED BY: calculateHash() method using SHA-256 algorithm
     * VALIDATED BY: isValid() method and Blockchain.h chain validation
     * 
     * MINING CONSTRAINT: Must start with 'difficulty' number of zeros
     * Example with difficulty=4: "0000a8f3c2..." is valid
     *                           "0001a8f3c2..." is invalid
     * 
     * WHY SHA-256: 
     * - Collision-resistant (practically impossible to find two inputs with same hash)
     * - One-way function (cannot reverse hash to get original data)
     * - Avalanche effect (small change in input drastically changes output)
     * - Industry standard used by Bitcoin and major blockchains
     */
    std::string hash;
    
    /**
     * @brief Unix epoch timestamp when the block was created
     * @type time_t (typically 64-bit signed integer representing seconds since 1970-01-01)
     * 
     * PURPOSE: Records when the block was created (temporal ordering)
     * USAGE: Included in hash calculation to ensure chronological uniqueness
     * 
     * WHY time_t: Standard C/C++ time representation, portable, efficient
     * RANGE: Can represent dates from 1970 to beyond year 2100
     * PRECISION: Second-level precision (sufficient for blockchain use)
     * 
     * RELATIONSHIP: Validated by Blockchain.h to prevent future-dated blocks
     * and ensure reasonable chronological ordering
     */
    time_t timestamp;
    
    /**
     * @brief Collection of transactions included in this block
     * @type std::vector<Transaction> (dynamic array of Transaction objects)
     * 
     * PURPOSE: Stores the actual data/content of the block (posts, likes, etc.)
     * 
     * DESIGN DECISION - Why Vector:
     * - Dynamic sizing (unknown number of transactions at compile time)
     * - Efficient memory management (contiguous storage)
     * - Standard container with iterator support
     * - O(1) access by index, O(n) serialization
     * 
     * TRANSACTION TYPES (defined in Transaction.h):
     * - CREATE_POST: User creates a new social media post
     * - LIKE_POST: User likes an existing post
     * - UNLIKE_POST: User removes their like
     * - DELETE_POST: User deletes their post
     * 
     * BLOCK SIZE CONSIDERATION:
     * In production, blocks typically have size limits (e.g., Bitcoin: ~1-4MB)
     * Current implementation: No explicit limit (can be added in Blockchain.h)
     * 
     * SERIALIZATION: transactionsToString() concatenates all transaction data
     * for inclusion in the block hash calculation
     */
    std::vector<Transaction> transactions;
    
    /**
     * @brief Number used in Proof-of-Work mining (nonce = "number used once")
     * @type int (32-bit signed integer)
     * 
     * PURPOSE: Variable that miners increment to find a valid block hash
     * 
     * PROOF-OF-WORK MECHANISM:
     * The nonce is repeatedly incremented (0, 1, 2, 3, ...) until the
     * calculateHash() produces a hash that meets the difficulty requirement
     * (starts with N zeros, where N = difficulty)
     * 
     * WHY THIS WORKS:
     * Due to SHA-256's properties, there's no way to predict which nonce
     * will produce a valid hash. Miners must try many values (brute force).
     * This computational work proves the miner invested resources, making
     * attacks expensive.
     * 
     * EXAMPLE:
     * difficulty = 4
     * nonce = 0 → hash = "a3f8c2..." (invalid, doesn't start with "0000")
     * nonce = 1 → hash = "7bc3d1..." (invalid)
     * ...
     * nonce = 45823 → hash = "0000f3a..." (VALID! Mining complete)
     * 
     * RANGE: ~2 billion attempts possible with 32-bit int
     * For very high difficulty, may need to adjust other block parameters
     */
    int nonce;
    
    /**
     * @brief Number of leading zeros required in the block hash (Proof-of-Work difficulty)
     * @type int (typically 1-10 for testing, 15-25 for production)
     * 
     * PURPOSE: Controls how hard it is to mine a block (computational cost)
     * 
     * DIFFICULTY EXPLAINED:
     * Higher difficulty = more leading zeros = harder to find valid hash
     * difficulty=4: hash must match "0000****************" (1 in 65,536 chance)
     * difficulty=5: hash must match "00000***************" (1 in ~1 million chance)
     * 
     * MINING TIME RELATIONSHIP:
     * Each additional difficulty level multiplies mining time by ~16
     * (because hex has 16 possible values: 0-9, a-f)
     * 
     * WHY ADJUSTABLE DIFFICULTY:
     * Bitcoin adjusts difficulty every 2016 blocks to maintain ~10 min block time
     * Our system: Fixed per block (can be made dynamic in Blockchain.h)
     * 
     * SECURITY TRADE-OFF:
     * Higher difficulty = more secure (harder to attack)
     * Lower difficulty = faster blocks (better user experience)
     * 
     * DEFAULT: 4 (reasonable for development/testing)
     */
    int difficulty;

    // ========================================================================
    // PRIVATE HELPER METHODS (Internal Cryptographic Operations)
    // ========================================================================
    
    /**
     * @brief Calculates the SHA-256 hash of this block's contents
     * @return std::string - 64-character hexadecimal hash
     * 
     * PURPOSE: Creates a unique cryptographic fingerprint of the block
     * 
     * ALGORITHM:
     * 1. Concatenate: index + previousHash + timestamp + transactions + nonce
     * 2. Pass concatenated string through SHA-256 hash function
     * 3. Return resulting hash as hexadecimal string
     * 
     * HASH INPUT COMPONENTS:
     * - index: Ensures each block position produces unique hash
     * - previousHash: Links this block to previous block cryptographically
     * - timestamp: Adds temporal uniqueness
     * - transactions: The actual data being secured
     * - nonce: Variable used in Proof-of-Work mining
     * 
     * WHY const: This method doesn't modify the block, allowing it to be
     *            called on const Block objects (functional programming style)
     * 
     * CALLED BY:
     * - Constructor: Initial hash calculation
     * - mineBlock(): Repeated hash recalculation during mining
     * - isValid(): Hash verification
     * 
     * SECURITY NOTE:
     * Any change to ANY component (even 1 bit) completely changes the hash
     * due to SHA-256's avalanche effect. This makes tampering detectable.
     * 
     * TIME COMPLEXITY: O(n) where n = total size of all transactions
     */
    std::string calculateHash() const {
        std::stringstream ss;
        // Concatenate all block components into a single string
        // Order matters! Different order = different hash
        ss << index << previousHash << timestamp << transactionsToString() << nonce;
        return sha256(ss.str());
    }

    /**
     * @brief Computes SHA-256 cryptographic hash of input data
     * @param data Input string to hash
     * @return std::string - 64-character hexadecimal representation of hash
     * 
     * PURPOSE: Wrapper around OpenSSL's SHA-256 implementation
     * 
     * SHA-256 (Secure Hash Algorithm 256-bit):
     * - Input: Any length string (0 to ~2^64 bits)
     * - Output: Fixed 256-bit (32-byte) hash
     * - Properties: One-way, collision-resistant, avalanche effect
     * 
     * OPENSSL API WORKFLOW:
     * 1. SHA256_Init(): Initialize SHA256_CTX context structure
     * 2. SHA256_Update(): Process input data (can be called multiple times)
     * 3. SHA256_Final(): Finalize and output hash
     * 
     * DATA TYPES:
     * - unsigned char hash[32]: Array to store raw 32-byte hash output
     * - SHA256_CTX: OpenSSL context structure (holds internal state)
     * - SHA256_DIGEST_LENGTH: Constant = 32 bytes = 256 bits
     * 
     * HEXADECIMAL CONVERSION:
     * Raw bytes are converted to hex for human readability:
     * [0xAB, 0xCD] → "abcd"
     * 
     * std::hex: Formats integers as hexadecimal
     * std::setw(2): Ensures 2 characters per byte (e.g., "0f" not "f")
     * std::setfill('0'): Pads with zeros (e.g., "0f" not " f")
     * 
     * WHY const: Pure function - doesn't modify object state
     * 
     * REFERENCE: OpenSSL EVP API (modern) vs legacy SHA API (used here)
     * Legacy chosen for simplicity; production might use EVP for better security
     */
    std::string sha256(const std::string& data) const {
        // Allocate 32-byte array for raw hash output
        unsigned char hash[SHA256_DIGEST_LENGTH];
        
        // Initialize SHA-256 context (prepares hashing engine)
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        
        // Process input data through SHA-256 algorithm
        // data.c_str(): Converts std::string to C-style char*
        // data.size(): Number of bytes to hash
        SHA256_Update(&sha256, data.c_str(), data.size());
        
        // Finalize hash computation and store result in 'hash' array
        SHA256_Final(hash, &sha256);
        
        // Convert raw bytes to hexadecimal string representation
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            // Format each byte as 2-digit hex (00-ff)
            // static_cast<int>: Treats unsigned char as integer for formatting
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();  // Returns 64-character hex string
    }

    /**
     * @brief Serializes all transactions into a single concatenated string
     * @return std::string - Concatenated serialization of all transactions
     * 
     * PURPOSE: Converts transaction vector into hashable string format
     * 
     * USAGE: Called by calculateHash() to include transaction data in block hash
     * 
     * ALGORITHM:
     * Iterates through transactions vector and concatenates each serialized form
     * Transaction.serialize() returns: sender + type + timestamp + data
     * 
     * WHY const: Read-only operation, doesn't modify block state
     * 
     * DATA FLOW:
     * transactions (vector) → serialize each → concatenate → single string
     * 
     * RANGE-BASED FOR LOOP EXPLAINED:
     * for(const auto& tx : transactions)
     * - const: Don't modify transactions
     * - auto: Compiler deduces type (Transaction)
     * - &: Reference (avoid copying entire Transaction object)
     * - tx: Variable name for each transaction
     * 
     * EFFICIENCY: O(n*m) where n=num transactions, m=avg transaction size
     * Could be optimized with reserve(), but current implementation is clear
     * 
     * RELATIONSHIP: Each transaction's serialize() defined in Transaction.h
     */
    std::string transactionsToString() const {
        std::stringstream ss;
        // Iterate through all transactions and concatenate their serialized forms
        for(const auto& tx : transactions) {
            ss << tx.serialize();  // Calls Transaction::serialize()
        }
        return ss.str();
    }

    // ========================================================================
    // PUBLIC INTERFACE (Constructor and Methods)
    // ========================================================================

public:
    /**
     * @brief Constructs a new Block with given parameters
     * @param index Block position in the blockchain (0 = genesis)
     * @param previousHash Hash of the previous block (creates chain link)
     * @param transactions Vector of Transaction objects to include in block
     * @param difficulty Number of leading zeros required in hash (default: 4)
     * 
     * PURPOSE: Initializes a new block ready for mining
     * 
     * INITIALIZATION ORDER (member initializer list):
     * 1. index: Set from parameter
     * 2. previousHash: Set from parameter (creates link to prev block)
     * 3. transactions: Copied from parameter vector
     * 4. difficulty: Set from parameter (default 4 for testing)
     * 5. timestamp: Generated via std::time(nullptr) - current Unix time
     * 6. nonce: Initialized to 0 (will be incremented during mining)
     * 7. hash: Calculated via calculateHash() with nonce=0
     * 
     * WHY MEMBER INITIALIZER LIST:
     * More efficient than assignment in constructor body
     * Required for const members and references
     * Initializes in declaration order, not list order
     * 
     * PARAMETER PASSING STRATEGIES:
     * - int index: Pass by value (int is small, 4 bytes)
     * - const std::string&: Pass by const reference (avoid copying large string)
     * - const std::vector<Transaction>&: Pass by const ref (avoid copying vector)
     * - int difficulty: Pass by value with default parameter
     * 
     * DEFAULT PARAMETER: difficulty = 4
     * Allows: Block(0, "0", txs) → difficulty defaults to 4
     * Or: Block(0, "0", txs, 6) → difficulty explicitly set to 6
     * 
     * POST-CONSTRUCTION STATE:
     * - Block is initialized but NOT mined (hash doesn't meet difficulty)
     * - Caller must call mineBlock() to perform Proof-of-Work
     * - This separation allows flexibility in mining strategy
     * 
     * CALLED BY: 
     * - Blockchain::createGenesisBlock() - Creates first block
     * - Blockchain::minePendingTransactions() - Creates regular blocks
     */
    Block(int index, const std::string& previousHash, const std::vector<Transaction>& transactions, int difficulty = 4)
        : index(index), previousHash(previousHash), transactions(transactions), difficulty(difficulty) {
        // Set timestamp to current time (seconds since Unix epoch: 1970-01-01)
        timestamp = std::time(nullptr);
        
        // Initialize nonce to 0 (will be incremented during mining)
        nonce = 0;
        
        // Calculate initial hash (likely won't meet difficulty requirement)
        hash = calculateHash();
    }

    /**
     * @brief Performs Proof-of-Work mining to find a valid block hash
     * @return void - Modifies the block's nonce and hash in place
     * 
     * PURPOSE: Implements the Proof-of-Work consensus mechanism
     * 
     * PROOF-OF-WORK EXPLAINED:
     * Mining is the process of finding a nonce value that produces a hash
     * meeting the difficulty requirement (starts with N zeros).
     * 
     * ALGORITHM:
     * 1. Create target string of zeros (e.g., "0000" for difficulty=4)
     * 2. Increment nonce starting from 1
     * 3. Recalculate hash with new nonce
     * 4. Check if hash starts with required zeros
     * 5. Repeat until valid hash found
     * 
     * WHY THIS PROVIDES SECURITY:
     * - Computational cost: Must try many nonces (avg. 16^difficulty attempts)
     * - Asymmetric verification: Mining is hard, verification is easy
     * - Cannot be predicted: Must brute-force due to SHA-256 properties
     * - Economic cost: Attacking blockchain requires mining many blocks
     * 
     * PERFORMANCE:
     * difficulty=1: ~16 attempts (instant)
     * difficulty=4: ~65,536 attempts (~milliseconds)
     * difficulty=6: ~16 million attempts (~seconds)
     * difficulty=8: ~4 billion attempts (~minutes)
     * 
     * STRING CONSTRUCTION:
     * std::string target(difficulty, '0')
     * Creates string with 'difficulty' number of '0' characters
     * Example: difficulty=4 → target = "0000"
     * 
     * SUBSTRING COMPARISON:
     * hash.substr(0, difficulty): Extracts first 'difficulty' characters
     * Compared against target to check validity
     * 
     * DO-WHILE LOOP:
     * Guarantees at least one iteration (nonce starts at 1)
     * Continues until hash meets difficulty requirement
     * 
     * SIDE EFFECTS:
     * - Modifies nonce (increments until valid)
     * - Modifies hash (updates with each calculation)
     * - CPU-intensive (intentionally expensive)
     * 
     * CALLED BY:
     * - Blockchain::createGenesisBlock() - Mines genesis block
     * - Blockchain::minePendingTransactions() - Mines regular blocks
     * 
     * ALTERNATIVE IMPLEMENTATIONS:
     * Bitcoin uses double SHA-256, we use single SHA-256 (simpler)
     * Production systems might add timeout or max attempts limit
     */
    void mineBlock() {
        // Create target pattern: string of zeros matching difficulty
        // Example: difficulty=4 → target="0000"
        std::string target(difficulty, '0');
        
        // Proof-of-Work loop: increment nonce until valid hash found
        do {
            nonce++;  // Try next nonce value
            hash = calculateHash();  // Recalculate hash with new nonce
            
            // Loop continues while hash doesn't start with required zeros
        } while (hash.substr(0, difficulty) != target);
        
        // Mining complete! Hash now meets difficulty requirement
        // Uncomment for debugging: std::cout << "Block mined: " << hash << std::endl;
    }

    /**
     * @brief Validates that this block meets Proof-of-Work requirements
     * @return bool - true if valid, false otherwise
     * 
     * PURPOSE: Verifies block integrity and mining correctness
     * 
     * TWO-PART VALIDATION:
     * 1. Hash starts with required number of zeros (Proof-of-Work)
     * 2. Hash matches recalculated value (Data integrity)
     * 
     * WHY BOTH CHECKS:
     * - First check: Ensures mining was performed correctly
     * - Second check: Detects any tampering with block data
     * 
     * ATTACK SCENARIOS PREVENTED:
     * - Fake PoW: Someone claims block is mined but hash doesn't have zeros
     * - Data tampering: Someone modifies transactions after mining
     * - Hash mismatch: Stored hash doesn't match actual block contents
     * 
     * SHORT-CIRCUIT EVALUATION:
     * && operator: If first condition false, second isn't evaluated
     * Optimization: Skip expensive calculateHash() if PoW check fails
     * 
     * WHY const:
     * Read-only validation, doesn't modify block state
     * Can be called on const Block objects
     * 
     * USAGE:
     * - Blockchain::isChainValid() - Validates entire chain
     * - Network nodes: Verify received blocks
     * - Testing: Ensure mining produced valid blocks
     * 
     * EFFICIENCY: O(n) where n = transaction data size (for hash calculation)
     */
    bool isValid() const {
        // Create target pattern for Proof-of-Work check
        std::string target(difficulty, '0');
        
        // Validate both PoW (leading zeros) and data integrity (hash matches)
        return hash.substr(0, difficulty) == target  // PoW: Has required zeros?
            && hash == calculateHash();              // Integrity: Hash still matches data?
    }

    // ========================================================================
    // GETTER METHODS (Public Read-Only Access to Private Members)
    // ========================================================================
    //
    // DESIGN PATTERN: Encapsulation
    // Private data with public const getters provides controlled access
    // Benefits:
    // - Read access without modification ability
    // - Can add validation or logging in future
    // - Maintains class invariants
    // ========================================================================
    
    /**
     * @brief Returns the block's index in the blockchain
     * @return int - Zero-based position (0 = genesis block)
     * WHY const: Read-only operation, allows use with const Block objects
     */
    int getIndex() const { return index; }
    
    /**
     * @brief Returns the block's cryptographic hash
     * @return std::string - 64-character hexadecimal SHA-256 hash
     * USAGE: Used by next block as previousHash to create chain link
     */
    std::string getHash() const { return hash; }
    
    /**
     * @brief Returns the hash of the previous block
     * @return std::string - Hash that links this block to previous block
     * CHAIN RELATIONSHIP: Blockchain validates previousHash == chain[i-1].getHash()
     */
    std::string getPreviousHash() const { return previousHash; }
    
    /**
     * @brief Returns the block creation timestamp
     * @return time_t - Unix epoch time (seconds since 1970-01-01)
     * USAGE: For chronological ordering, UI display, analytics
     */
    time_t getTimestamp() const { return timestamp; }
    
    /**
     * @brief Returns reference to transaction vector
     * @return const std::vector<Transaction>& - Reference to internal transactions
     * WHY const&: Avoids copying entire vector, prevents modification
     * USAGE: Iterate transactions, display in UI, search for specific transaction
     */
    const std::vector<Transaction>& getTransactions() const { return transactions; }
    
    /**
     * @brief Returns the nonce value found during mining
     * @return int - The "magic number" that made the hash valid
     * USAGE: Debugging, analytics, research on mining difficulty
     */
    int getNonce() const { return nonce; }
    
    /**
     * @brief Returns the difficulty level used for mining
     * @return int - Number of leading zeros required in hash
     * USAGE: Analytics, adjusting difficulty for future blocks
     */
    int getDifficulty() const { return difficulty; }

    /**
     * @brief Generates human-readable string representation of the block
     * @return std::string - Formatted block information
     * 
     * PURPOSE: Creates readable output for logging, debugging, and display
     * 
     * OUTPUT FORMAT:
     * Block #2 [
     *   Hash: 0000a8f3c2d...
     *   Previous Hash: 0000b7e2f1...
     *   Timestamp: 1698765432
     *   Nonce: 45823
     *   Transactions: 5
     * ]
     * 
     * USAGE:
     * - Blockchain::printChain() - Displays entire blockchain
     * - Debugging: Quick inspection of block contents
     * - Logging: Record block creation events
     * - Testing: Verify block properties
     * 
     * WHY const: Read-only operation, doesn't modify block
     * 
     * DESIGN DECISION:
     * Shows summary (transaction count) rather than full transaction details
     * to keep output concise. Use getTransactions() for detailed access.
     * 
     * ALTERNATIVE: Could return JSON for API compatibility
     */
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

// ============================================================================
// END OF BLOCK CLASS
// ============================================================================

#endif // BLOCK_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * MEMORY MANAGEMENT:
 * - Block objects typically stored in std::shared_ptr by Blockchain class
 * - Automatic memory management (no manual new/delete needed)
 * - Vector handles transaction memory automatically
 * 
 * THREAD SAFETY:
 * - Block class itself is not thread-safe
 * - Blockchain class uses std::mutex to protect concurrent access
 * - Once mined, blocks should be treated as immutable
 * 
 * PERFORMANCE CONSIDERATIONS:
 * - Mining is intentionally CPU-intensive (security feature)
 * - Hash calculations are relatively fast (SHA-256 is optimized)
 * - Vector copy in constructor could be optimized with move semantics
 * 
 * SECURITY CONSIDERATIONS:
 * - SHA-256 provides 2^256 possible hashes (astronomically large)
 * - Difficulty prevents easy block creation
 * - Hash chain makes historical tampering computationally infeasible
 * 
 * POTENTIAL IMPROVEMENTS:
 * - Add Merkle tree for efficient transaction verification
 * - Implement block size limits
 * - Add timestamp validation
 * - Support for different hash algorithms
 * - Difficulty adjustment algorithm
 * 
 * TESTING:
 * - Unit tests should verify hash calculation correctness
 * - Test mining with various difficulties
 * - Validate chain integrity after tampering attempts
 * - Performance benchmarks for different difficulty levels
 ******************************************************************************/

