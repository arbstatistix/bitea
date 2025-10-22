/*******************************************************************************
 * TRANSACTION.H - Blockchain Transaction Data Structure
 * 
 * PURPOSE:
 * This header defines the Transaction class, which represents individual
 * data entries stored in blockchain blocks. Transactions record user actions
 * (posts, likes, comments) in the Bitea social media platform.
 * 
 * BLOCKCHAIN CONTEXT:
 * In traditional blockchains (Bitcoin), transactions represent value transfer.
 * In Bitea, transactions represent social media actions:
 * - POST: User creates content
 * - LIKE: User likes content
 * - COMMENT: User comments on content
 * - FOLLOW: User follows another user
 * - etc.
 * 
 * TRANSACTION LIFECYCLE:
 * 1. User performs action via API
 * 2. Transaction object created with action details
 * 3. Added to pending transaction pool (Blockchain.h)
 * 4. Included in block during mining
 * 5. Permanently recorded in blockchain (immutable)
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - Block.h: Blocks contain vectors of transactions
 * - Blockchain.h: Manages pending transaction pool
 * - HttpServer.h (main.cpp): Creates transactions from API calls
 * - Database: Transactions indexed for querying
 * 
 * WHY IMMUTABLE SOCIAL MEDIA:
 * - Censorship resistance: Data cannot be deleted by platform
 * - Authenticity: All actions cryptographically verifiable
 * - Transparency: Complete audit trail of all actions
 * - Decentralization: No single point of control
 * 
 * DATA MODEL:
 * Unlike Bitcoin's UTXO model, we use account-based model
 * Each transaction records:
 * - Who performed the action (sender)
 * - What type of action (type)
 * - Action details (data as JSON)
 * - When it occurred (timestamp)
 * 
 * REFERENCES:
 * - Ethereum Transaction Model: https://ethereum.org/en/developers/docs/transactions/
 * - Blockchain Data Structures: "Mastering Blockchain" by Imran Bashir
 * - Social Media on Blockchain: Steemit, Peepeth architectures
 ******************************************************************************/

#ifndef TRANSACTION_H
#define TRANSACTION_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>      // std::string - for text data (sender, data, id)
#include <ctime>       // time_t, std::time() - Unix timestamp generation
#include <sstream>     // std::stringstream - string building and serialization

// ============================================================================
// TRANSACTION TYPE ENUMERATION
// ============================================================================

/**
 * @enum TransactionType
 * @brief Defines types of actions that can be recorded on the blockchain
 * 
 * WHY enum class:
 * - Type-safe (cannot implicitly convert to int)
 * - Scoped names (TransactionType::POST, not just POST)
 * - Better than #define or plain enum (prevents naming conflicts)
 * 
 * TRANSACTION TYPES EXPLAINED:
 * 
 * BASIC SOCIAL ACTIONS:
 * - POST: User creates a new post/content
 *   Data: {title, content, media_urls, tags}
 * 
 * - LIKE: User likes a post
 *   Data: {post_id}
 * 
 * - COMMENT: User comments on a post
 *   Data: {post_id, comment_text}
 * 
 * - FOLLOW: User follows another user
 *   Data: {target_user_id}
 * 
 * SYSTEM ACTIONS:
 * - USER_REGISTRATION: New user joins platform
 *   Data: {username, public_key, profile_info}
 * 
 * TOPIC-BASED ACTIONS (Forum/Discussion Features):
 * - TOPIC_CREATE: User creates a discussion topic
 *   Data: {title, description, category}
 * 
 * - TOPIC_COMMENT: User comments on a topic
 *   Data: {topic_id, comment_text}
 * 
 * - TOPIC_LIKE: User likes a topic
 *   Data: {topic_id}
 * 
 * - TOPIC_RESHARE: User shares/retweets a topic
 *   Data: {topic_id, reshare_comment}
 * 
 * EXTENSIBILITY:
 * New transaction types can be added as platform evolves:
 * - EDIT_POST, DELETE_POST (soft delete, kept in blockchain)
 * - TIP/DONATION (cryptocurrency integration)
 * - NFT_MINT (digital collectibles)
 * - PRIVATE_MESSAGE (encrypted messaging)
 * 
 * STORAGE:
 * Enum stored as integer internally (typically 4 bytes)
 * Efficient for serialization and comparison
 */
enum class TransactionType {
    POST,                // Create new post/content (type 0)
    LIKE,                // Like a post (type 1)
    COMMENT,             // Comment on post (type 2)
    FOLLOW,              // Follow a user (type 3)
    USER_REGISTRATION,   // Register new user (type 4)
    TOPIC_CREATE,        // Create discussion topic (type 5)
    TOPIC_COMMENT,       // Comment on topic (type 6)
    TOPIC_LIKE,          // Like a topic (type 7)
    TOPIC_RESHARE        // Reshare/retweet topic (type 8)
};

// ============================================================================
// TRANSACTION CLASS DEFINITION
// ============================================================================

/**
 * @class Transaction
 * @brief Represents a single action/event recorded on the blockchain
 * 
 * DESIGN PATTERN: Value Object (immutable after creation)
 * 
 * COMPONENTS:
 * - id: Unique identifier for transaction lookup
 * - sender: User who initiated the transaction
 * - type: What kind of action (POST, LIKE, etc.)
 * - data: JSON string with action-specific details
 * - timestamp: When transaction was created
 * 
 * IMMUTABILITY:
 * Once created, transaction data shouldn't change
 * No setters provided (only constructor and getters)
 * Enforces blockchain immutability principle
 * 
 * SIZE CONSIDERATIONS:
 * Typical transaction: ~100-1000 bytes
 * - id: ~50 bytes
 * - sender: ~20 bytes (username)
 * - type: 4 bytes (enum)
 * - data: ~50-900 bytes (JSON, varies by type)
 * - timestamp: 8 bytes (time_t)
 * Total: ~132-982 bytes per transaction
 * 
 * RELATIONSHIP TO BLOCK:
 * Block contains vector of Transactions
 * Block hash computed from all transaction data
 * Changing any transaction changes block hash (tamper-evident)
 */
class Transaction {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (Transaction Data)
    // ========================================================================
    
    /**
     * @brief Unique identifier for this transaction
     * @type std::string (typically ~30-50 characters)
     * 
     * PURPOSE: Enables transaction lookup and referencing
     * 
     * FORMAT: "{sender}-{type}-{timestamp}"
     * Example: "alice-0-1698765432" (alice creates post at timestamp)
     * 
     * GENERATION: Automatically generated by generateId() in constructor
     * 
     * UNIQUENESS:
     * - Sender: Different users create different IDs
     * - Type: Same user doing different actions creates different IDs
     * - Timestamp: Same user, same action at different times = different IDs
     * 
     * COLLISION PROBABILITY:
     * If same user performs same action type in same second → collision
     * Could be improved by adding nonce or random component
     * 
     * USAGE:
     * - Database indexing (primary key)
     * - API queries: "GET /api/transaction/{id}"
     * - Referencing: Like transactions reference post transaction ID
     * 
     * ALTERNATIVE APPROACHES:
     * - UUID: Cryptographically random (more robust)
     * - Hash: SHA-256 of transaction contents (content-addressed)
     * - Sequential: Auto-increment number (simpler but less distributed)
     * 
     * WHY std::string:
     * Flexible format, human-readable, easy to serialize
     */
    std::string id;
    
    /**
     * @brief Identifier of user who created this transaction
     * @type std::string (username, typically 3-30 characters)
     * 
     * PURPOSE: Records who performed the action
     * 
     * AUTHENTICATION:
     * In production blockchain, this would be:
     * - Public key address (e.g., "0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb")
     * - Signed by private key (cryptographic proof of ownership)
     * 
     * CURRENT IMPLEMENTATION:
     * Simple username string (trust-based)
     * Relies on API authentication (HttpServer validates user)
     * 
     * SECURITY CONSIDERATION:
     * Without cryptographic signatures, sender field can be spoofed
     * Production would use public-key cryptography (like Bitcoin addresses)
     * 
     * SPECIAL VALUES:
     * "SYSTEM": Reserved for protocol-generated transactions
     *           (e.g., genesis block, system events)
     * 
     * USAGE:
     * - Attribution: Who created this post/comment?
     * - Filtering: Show all transactions from user "alice"
     * - Authorization: Can user edit/delete this content?
     * 
     * WHY std::string:
     * Flexible to support usernames or hex addresses
     */
    std::string sender;
    
    /**
     * @brief Type of action this transaction represents
     * @type TransactionType enum (4 bytes)
     * 
     * PURPOSE: Categorizes what kind of action occurred
     * 
     * VALUES: See TransactionType enum (POST, LIKE, COMMENT, etc.)
     * 
     * WHY ENUM:
     * - Type safety: Cannot pass invalid type
     * - Efficient: Stored as integer (4 bytes vs. string)
     * - Clear intent: TransactionType::POST vs. magic number 0
     * - IDE support: Auto-completion, type checking
     * 
     * USAGE:
     * - Parsing: How to interpret 'data' field?
     * - Filtering: Show only POST transactions
     * - Validation: Check if action is allowed
     * - Processing: Different logic for different types
     * 
     * SERIALIZATION:
     * Cast to int for hashing: static_cast<int>(type)
     * Example: TransactionType::POST → 0
     * 
     * EXTENSIBILITY:
     * New types can be added to enum without breaking existing code
     */
    TransactionType type;
    
    /**
     * @brief JSON string containing transaction-specific data
     * @type std::string (JSON-formatted, varies by transaction type)
     * 
     * PURPOSE: Stores action details in flexible format
     * 
     * WHY JSON:
     * - Flexible: Different transaction types need different fields
     * - Standard: Easy to parse in any language
     * - Human-readable: Can inspect contents easily
     * - Extensible: Add new fields without breaking old code
     * 
     * DATA EXAMPLES BY TYPE:
     * 
     * POST:
     * {"title":"Hello World","content":"My first post!","tags":["intro"]}
     * 
     * LIKE:
     * {"post_id":"alice-0-1698765432"}
     * 
     * COMMENT:
     * {"post_id":"alice-0-1698765432","text":"Great post!"}
     * 
     * FOLLOW:
     * {"target_user":"bob"}
     * 
     * USER_REGISTRATION:
     * {"username":"alice","email":"alice@example.com","bio":"Blockchain enthusiast"}
     * 
     * VALIDATION:
     * Should validate JSON structure matches expected schema for type
     * Current: No validation (accepts any string)
     * Production: Use JSON schema validation
     * 
     * SIZE CONSIDERATIONS:
     * Large posts (with images/videos) should store URLs, not raw data
     * Typical size: 50-1000 bytes
     * Max size: Should be limited (e.g., 10KB) to prevent bloat
     * 
     * PARSING:
     * Application layer parses JSON to extract specific fields
     * Blockchain layer treats it as opaque string
     * 
     * WHY std::string:
     * JSON is text-based format
     * Could use JSON library (nlohmann/json, RapidJSON) for type safety
     */
    std::string data;  // JSON string containing transaction data
    
    /**
     * @brief Unix epoch timestamp when transaction was created
     * @type time_t (typically 64-bit signed integer)
     * 
     * PURPOSE: Records when action occurred (chronological ordering)
     * 
     * GENERATION: std::time(nullptr) - seconds since 1970-01-01 00:00:00 UTC
     * 
     * PRECISION: Second-level (no milliseconds)
     * For higher precision, could use std::chrono
     * 
     * USAGE:
     * - Ordering: Sort transactions chronologically
     * - ID generation: Part of transaction ID
     * - Display: Convert to human-readable date/time
     * - Analytics: Transaction volume over time
     * 
     * RANGE:
     * 64-bit time_t: Valid until year 292 billion (practically unlimited)
     * 32-bit time_t: Y2038 problem (Jan 19, 2038 overflow)
     * Modern systems use 64-bit
     * 
     * TIMEZONE:
     * Stored as UTC (universal time)
     * Display layer converts to user's local timezone
     * 
     * RELATIONSHIP TO BLOCK TIMESTAMP:
     * Transaction timestamp: When user created transaction
     * Block timestamp: When block was mined
     * Block timestamp >= all its transaction timestamps (usually)
     * 
     * WHY time_t:
     * Standard C/C++ time representation
     * Compatible with database time types
     * Easy conversion to/from human-readable formats
     */
    time_t timestamp;

    // ========================================================================
    // PUBLIC INTERFACE (Constructor and Methods)
    // ========================================================================

public:
    /**
     * @brief Constructs a new Transaction with given parameters
     * @param sender Username or ID of transaction creator
     * @param type Type of action (POST, LIKE, etc.)
     * @param data JSON string with action-specific details
     * 
     * PURPOSE: Creates transaction object ready for blockchain inclusion
     * 
     * INITIALIZATION ORDER (member initializer list):
     * 1. sender: Copied from parameter
     * 2. type: Copied from parameter
     * 3. data: Copied from parameter
     * 4. timestamp: Generated via std::time(nullptr)
     * 5. id: Generated via generateId() (uses sender, type, timestamp)
     * 
     * PARAMETER PASSING:
     * const std::string&: Pass by const reference
     * - Avoids copying large strings (efficient)
     * - const: Guarantees we won't modify caller's data
     * TransactionType: Pass by value (enum is small, 4 bytes)
     * 
     * AUTOMATIC FIELDS:
     * Caller doesn't provide:
     * - id: Auto-generated to ensure uniqueness
     * - timestamp: Auto-captured at creation time
     * This ensures consistency and prevents manipulation
     * 
     * VALIDATION (MISSING):
     * Production implementation should validate:
     * - sender is not empty
     * - data is valid JSON
     * - data matches schema for given type
     * - sender has permission for this action
     * 
     * USAGE EXAMPLE:
     * Transaction tx("alice", TransactionType::POST, 
     *               "{\"title\":\"Hello\",\"content\":\"World\"}");
     * 
     * CALLED BY:
     * - main.cpp: API endpoint handlers create transactions
     * - Test code: Generate transactions for testing
     * 
     * EXCEPTION SAFETY:
     * May throw std::bad_alloc if string allocation fails (rare)
     * std::time() typically doesn't fail
     */
    Transaction(const std::string& sender, TransactionType type, const std::string& data)
        : sender(sender), type(type), data(data) {
        // Capture current time (UTC, seconds since Unix epoch)
        timestamp = std::time(nullptr);
        
        // Generate unique ID from sender, type, and timestamp
        generateId();
    }

    /**
     * @brief Generates unique ID for this transaction
     * @return void (modifies 'id' member variable)
     * 
     * PURPOSE: Creates identifier from transaction properties
     * 
     * ID FORMAT: "{sender}-{type_as_int}-{timestamp}"
     * Example: "alice-0-1698765432"
     * - alice: Username
     * - 0: TransactionType::POST (enum value)
     * - 1698765432: Unix timestamp
     * 
     * WHY THIS FORMAT:
     * - Human-readable: Can understand ID without lookup
     * - Sortable: Lexicographic sort ≈ chronological (for same sender)
     * - Debuggable: ID reveals transaction properties
     * 
     * UNIQUENESS GUARANTEE:
     * Unique if: Same sender doesn't create same type in same second
     * Collision possible but rare in practice
     * 
     * ENUM TO INT CONVERSION:
     * static_cast<int>(type): Converts enum to underlying integer
     * TransactionType::POST → 0
     * TransactionType::LIKE → 1
     * etc.
     * 
     * STRINGSTREAM USAGE:
     * std::stringstream: Efficient string building
     * Concatenates multiple types into single string
     * Alternative: std::string concatenation (less efficient)
     * 
     * CALLED BY:
     * Constructor (automatically during transaction creation)
     * 
     * POTENTIAL IMPROVEMENTS:
     * - Add random nonce to prevent collisions
     * - Use UUID for guaranteed uniqueness
     * - Hash transaction contents for content-addressing
     */
    void generateId() {
        std::stringstream ss;
        // Concatenate: sender, type (as int), timestamp
        // Separated by hyphens for readability
        ss << sender << "-" << static_cast<int>(type) << "-" << timestamp;
        id = ss.str();  // Convert stringstream to string
    }

    // ========================================================================
    // GETTER METHODS (Public Read-Only Access)
    // ========================================================================
    //
    // DESIGN PATTERN: Encapsulation
    // Private data with public const getters provides controlled access
    // ========================================================================
    
    /**
     * @brief Returns the transaction's unique identifier
     * @return std::string - Transaction ID
     * WHY const: Read-only operation, immutable after creation
     * USAGE: Database lookups, API responses, referencing
     */
    std::string getId() const { return id; }
    
    /**
     * @brief Returns the sender's identifier
     * @return std::string - Username or address of transaction creator
     * USAGE: Attribution, filtering transactions by user, authorization
     */
    std::string getSender() const { return sender; }
    
    /**
     * @brief Returns the transaction type
     * @return TransactionType - Enum indicating action type
     * USAGE: Parsing data field, filtering by type, routing logic
     */
    TransactionType getType() const { return type; }
    
    /**
     * @brief Returns the transaction data payload
     * @return std::string - JSON string with transaction details
     * USAGE: Parse to extract post content, comment text, etc.
     */
    std::string getData() const { return data; }
    
    /**
     * @brief Returns the creation timestamp
     * @return time_t - Unix epoch timestamp
     * USAGE: Chronological sorting, display, analytics
     */
    time_t getTimestamp() const { return timestamp; }

    /**
     * @brief Generates human-readable string representation
     * @return std::string - Formatted transaction information
     * 
     * PURPOSE: Debugging, logging, display
     * 
     * OUTPUT FORMAT:
     * Transaction{id=alice-0-1698765432, sender=alice, type=0, 
     *            timestamp=1698765432, data={"title":"Hello"}}
     * 
     * USAGE:
     * - Debugging: Quick inspection of transaction
     * - Logging: Record transaction details
     * - Console output: Display transaction info
     * 
     * DESIGN DECISION:
     * Includes all fields for complete information
     * Alternative: Could format as JSON for API compatibility
     * 
     * WHY const:
     * Read-only operation, doesn't modify transaction
     */
    std::string toString() const {
        std::stringstream ss;
        ss << "Transaction{id=" << id 
           << ", sender=" << sender 
           << ", type=" << static_cast<int>(type)  // Convert enum to int
           << ", timestamp=" << timestamp
           << ", data=" << data << "}";
        return ss.str();
    }

    /**
     * @brief Serializes transaction for inclusion in block hash
     * @return std::string - Concatenated transaction data
     * 
     * PURPOSE: Creates deterministic string for cryptographic hashing
     * 
     * INCLUDED FIELDS:
     * sender + type + timestamp + data
     * 
     * NOTABLY EXCLUDED: id
     * Why? ID is derived from other fields, including it would be redundant
     * 
     * DETERMINISTIC:
     * Same transaction always produces same serialization
     * Critical for blockchain integrity - hash must be reproducible
     * 
     * ORDER MATTERS:
     * Fields concatenated in specific order
     * Changing order would change hash (breaking verification)
     * 
     * NO SEPARATORS:
     * Fields concatenated directly without delimiters
     * Example: "alice" + "0" + "1698765432" + "{...}" → "alice01698765432{...}"
     * This is safe because we hash the result (not parsing it back)
     * 
     * USED BY:
     * - Block::transactionsToString(): Concatenates all transaction serializations
     * - Block::calculateHash(): Includes transaction data in block hash
     * 
     * WHY const:
     * Read-only serialization, doesn't modify transaction
     * 
     * SECURITY:
     * This serialization is cryptographically hashed
     * Any tampering changes the hash, making it detectable
     * 
     * ALTERNATIVE FORMATS:
     * - JSON: More structured but larger
     * - Binary: More compact but less readable
     * - Current: Simple concatenation (sufficient for our use case)
     * 
     * EFFICIENCY: O(n) where n = total data size
     * Stringstream efficiently builds result string
     */
    std::string serialize() const {
        std::stringstream ss;
        // Concatenate fields in fixed order (no separators needed for hashing)
        ss << sender                        // Who created it
           << static_cast<int>(type)        // What kind of action (as integer)
           << timestamp                     // When it was created
           << data;                         // Action details
        return ss.str();
    }
};

// ============================================================================
// END OF TRANSACTION CLASS
// ============================================================================

#endif // TRANSACTION_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * DESIGN PHILOSOPHY:
 * - Immutability: No setters, transaction cannot be modified after creation
 * - Simplicity: Straightforward data structure, easy to understand
 * - Flexibility: JSON data field supports diverse transaction types
 * - Efficiency: Small memory footprint (~100-1000 bytes)
 * 
 * SECURITY CONSIDERATIONS:
 * - No cryptographic signatures: Sender field is trust-based
 * - Production: Should use public-key cryptography
 * - No validation: Data field accepts any string
 * - Production: Should validate JSON schema
 * - Timestamp manipulation: Client-controlled timestamp
 * - Production: Could validate against block timestamp
 * 
 * EXTENSIBILITY:
 * - New transaction types: Add to TransactionType enum
 * - New fields: Add to data JSON (backward compatible)
 * - Schema evolution: Old transactions remain valid
 * 
 * COMPARISON TO OTHER BLOCKCHAINS:
 * 
 * BITCOIN:
 * - Transactions: Value transfer (inputs/outputs, UTXO model)
 * - Bitea: Action recording (account-based model)
 * 
 * ETHEREUM:
 * - Transactions: Value transfer + smart contract calls
 * - Bitea: Similar flexibility with different use case
 * 
 * SOCIAL BLOCKCHAINS (Steemit, Hive):
 * - Similar: Content posting, voting, following
 * - Different: Bitea is educational/demo, they are production
 * 
 * POTENTIAL IMPROVEMENTS:
 * 
 * CRYPTOGRAPHIC SIGNATURES:
 * Add signature field with ECDSA/Ed25519 signature
 * Verify sender has private key for claimed public key
 * 
 * SCHEMA VALIDATION:
 * Validate data JSON matches expected schema for transaction type
 * Prevent malformed or malicious data
 * 
 * FEE MECHANISM:
 * Add fee field to prioritize transactions
 * Prevent spam, incentivize miners
 * 
 * NONCE FOR ORDERING:
 * Add per-user nonce to ensure transaction ordering
 * Prevent replay attacks, ensure sequential execution
 * 
 * COMPRESSION:
 * Compress large data fields
 * Reduce blockchain bloat for text-heavy posts
 * 
 * ATTACHMENTS:
 * Add support for binary attachments (images, videos)
 * Store as IPFS hashes or similar
 * 
 * ENCRYPTION:
 * Add optional encryption for private content
 * Public metadata, encrypted payload
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: Constructor, ID generation, serialization
 * - Integration tests: Transaction flow through blockchain
 * - Security tests: Attempt to create invalid transactions
 * - Performance tests: Transaction creation and serialization speed
 * - Edge cases: Empty fields, very large data, special characters
 * 
 * MEMORY MANAGEMENT:
 * - Automatic: std::string handles memory
 * - Copy semantics: Transactions copied when added to blocks
 * - Move semantics: Could optimize with std::move
 * - No manual new/delete needed
 * 
 * THREAD SAFETY:
 * - Transaction class: Not thread-safe (doesn't need to be)
 * - Immutable after creation: Safe to share between threads
 * - Vector of transactions: Protected by Blockchain mutex
 ******************************************************************************/

