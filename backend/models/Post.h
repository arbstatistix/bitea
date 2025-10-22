/*******************************************************************************
 * POST.H - Social Media Post Data Model
 * 
 * PURPOSE:
 * This header defines the Post class and Comment struct, representing social
 * media content in the Bitea platform. Posts are the core content units that
 * users create, like, and comment on, which are then recorded on the blockchain.
 * 
 * DATA MODEL ARCHITECTURE:
 * Post: Main content object (author, content, likes, comments, blockchain status)
 * Comment: Nested content object (replies to posts)
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - Blockchain: Posts are stored as transactions in blocks
 * - Database (MongoClient): Posts cached/indexed for fast retrieval
 * - HttpServer (main.cpp): Posts created/retrieved via REST API
 * - User.h: Posts authored by users, users can like/comment
 * 
 * LIFECYCLE OF A POST:
 * 1. User creates post via API → Post object instantiated
 * 2. Post stored in database (fast access)
 * 3. Transaction created and added to blockchain (immutable record)
 * 4. Once mined, post.isOnChain = true, blockchainHash set
 * 5. Post can be retrieved from database or reconstructed from blockchain
 * 
 * DUAL STORAGE APPROACH:
 * DATABASE: Fast queries, filtering, pagination
 * BLOCKCHAIN: Immutable proof, censorship resistance, authenticity
 * 
 * WHY BOTH?
 * - Database for performance (O(1) lookups vs. O(n) blockchain scan)
 * - Blockchain for integrity (tamper-proof record)
 * - Database can be regenerated from blockchain if corrupted
 * 
 * SOCIAL FEATURES:
 * - Likes: Users can like/unlike posts (set ensures uniqueness)
 * - Comments: Users can add replies (ordered by time)
 * - Attribution: Each post links to author
 * - Blockchain verification: Posts can prove they existed at specific time
 * 
 * REFERENCES:
 * - Twitter/X data model: Similar post + like + comment structure
 * - Steemit: Blockchain-based social media precedent
 * - Reddit: Comment threading (could be extended)
 ******************************************************************************/

#ifndef POST_H
#define POST_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>      // std::string - text content, IDs, usernames
#include <vector>      // std::vector<Comment> - ordered list of comments
#include <set>         // std::set<std::string> - unique collection of user likes
#include <ctime>       // time_t, std::time() - timestamp generation
#include <sstream>     // std::stringstream - JSON serialization

// ============================================================================
// COMMENT STRUCT DEFINITION
// ============================================================================

/**
 * @struct Comment
 * @brief Represents a user's comment/reply on a post
 * 
 * DESIGN DECISION: struct vs class
 * Using struct (public by default) because Comment is simple data container
 * No complex invariants to protect, straightforward data access
 * 
 * NESTED COMMENTS:
 * Current: Flat comment structure (all comments at same level)
 * Could extend: Tree structure for nested replies (Reddit-style)
 * 
 * RELATIONSHIP TO POST:
 * Each Post contains vector<Comment> storing all its comments
 * Comments are ordered chronologically by timestamp
 * 
 * BLOCKCHAIN STORAGE:
 * Comments stored as part of Post object in database
 * Comment actions can also be stored as separate blockchain transactions
 * Trade-off: Embedded (simpler) vs. Separate (more flexible)
 */
struct Comment {
    // ========================================================================
    // PUBLIC MEMBER VARIABLES (Struct has public access by default)
    // ========================================================================
    
    /**
     * @brief Unique identifier for this comment
     * @type std::string (format: "author-timestamp")
     * 
     * PURPOSE: Enables comment lookup, editing, deletion
     * 
     * GENERATION: Automatically created in constructor
     * FORMAT: "{author}-{timestamp}"
     * Example: "alice-1698765432"
     * 
     * UNIQUENESS: Same as Transaction IDs - collision possible if same user
     * comments twice in same second (rare in practice)
     */
    std::string id;
    
    /**
     * @brief Username of comment author
     * @type std::string
     * 
     * PURPOSE: Attribution - who wrote this comment
     * USAGE: Display author name, filter comments by user
     */
    std::string author;
    
    /**
     * @brief The text content of the comment
     * @type std::string
     * 
     * PURPOSE: The actual comment text
     * SIZE: Should be limited (e.g., 1000 chars) to prevent abuse
     * SANITIZATION: Escapes special characters in JSON output
     */
    std::string content;
    
    /**
     * @brief Unix epoch timestamp when comment was created
     * @type time_t
     * 
     * PURPOSE: Chronological ordering, display "posted X ago"
     * USAGE: Comments sorted by timestamp in display
     */
    time_t timestamp;

    /**
     * @brief Constructs a new Comment
     * @param author Username of comment author
     * @param content Text content of the comment
     * 
     * PURPOSE: Creates comment with auto-generated ID and timestamp
     * 
     * PARAMETER PASSING:
     * const std::string&: Avoid copying strings (efficient)
     * 
     * AUTO-GENERATED FIELDS:
     * - timestamp: Current time (ensures chronological ordering)
     * - id: Derived from author + timestamp (enables lookup)
     * 
     * USAGE: Called by Post::addComment()
     */
    Comment(const std::string& author, const std::string& content)
        : author(author), content(content) {
        // Capture current time
        timestamp = std::time(nullptr);
        
        // Generate ID: "author-timestamp"
        // std::to_string converts time_t to string
        id = author + "-" + std::to_string(timestamp);
    }

    /**
     * @brief Serializes comment to JSON format
     * @return std::string - JSON representation of comment
     * 
     * PURPOSE: API responses, database storage, frontend display
     * 
     * OUTPUT FORMAT:
     * {"id":"alice-1698765432","author":"alice","content":"Great post!","timestamp":1698765432}
     * 
     * CONTENT ESCAPING:
     * Special characters in content are escaped for valid JSON
     * Prevents JSON injection attacks
     * 
     * WHY const: Read-only operation, doesn't modify comment
     */
    std::string toJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << id << "\",";
        ss << "\"author\":\"" << author << "\",";
        ss << "\"content\":\"" << escapeJson(content) << "\",";  // Escape special chars
        ss << "\"timestamp\":" << timestamp;
        ss << "}";
        return ss.str();
    }

private:
    /**
     * @brief Escapes special characters for valid JSON strings
     * @param str String to escape
     * @return std::string - Escaped string safe for JSON
     * 
     * PURPOSE: Prevents JSON syntax errors and injection attacks
     * 
     * ESCAPES:
     * " → \" (quotes must be escaped in JSON strings)
     * \ → \\ (backslash must be escaped)
     * \n → \\n (newline as escape sequence)
     * \r → \\r (carriage return)
     * \t → \\t (tab)
     * 
     * SECURITY:
     * Without escaping: {"content":"User said "hello""} → Invalid JSON
     * With escaping: {"content":"User said \"hello\""} → Valid JSON
     * 
     * EFFICIENCY: O(n) where n = string length
     * Single pass through string
     */
    std::string escapeJson(const std::string& str) const {
        std::string result;
        // Reserve approximate space (optimization for large strings)
        // result.reserve(str.size() * 1.1);  // Could add this
        
        for (char c : str) {
            // Escape special JSON characters
            if (c == '"') result += "\\\"";       // Quote
            else if (c == '\\') result += "\\\\"; // Backslash
            else if (c == '\n') result += "\\n";  // Newline
            else if (c == '\r') result += "\\r";  // Carriage return
            else if (c == '\t') result += "\\t";  // Tab
            else result += c;                     // Regular character
        }
        return result;
    }
};

// ============================================================================
// POST CLASS DEFINITION
// ============================================================================

/**
 * @class Post
 * @brief Represents a social media post with likes, comments, and blockchain integration
 * 
 * DESIGN PATTERN: Entity with rich behavior (not just data container)
 * 
 * KEY FEATURES:
 * - Content: Text-based post with author attribution
 * - Social interactions: Likes (set for uniqueness) and comments (ordered)
 * - Blockchain integration: Tracks if post is on blockchain and which block
 * - JSON serialization: For API responses and database storage
 * 
 * DATA INTEGRITY:
 * - Database: Fast retrieval, queries (MongoDB)
 * - Blockchain: Immutable proof of existence and content
 * - Posts can be verified against blockchain to detect tampering
 * 
 * RELATIONSHIP TO OTHER MODELS:
 * - User: Post has author (User), Users can like/comment
 * - Transaction: Post creation stored as blockchain transaction
 * - Block: Post linked to specific block via blockchainHash
 */
class Post {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (Post Data and State)
    // ========================================================================
    
    /**
     * @brief Unique identifier for this post
     * @type std::string
     * 
     * PURPOSE: Primary key for database, URL routing, references
     * 
     * GENERATION: Typically generated externally (e.g., MongoDB ObjectId)
     * or using similar pattern as Comment/Transaction IDs
     * 
     * USAGE:
     * - Database queries: db.posts.find({id: "..."})
     * - API endpoints: GET /api/posts/{id}
     * - References: Comments reference parent post ID
     * - Blockchain: Transaction data includes post ID
     */
    std::string id;
    
    /**
     * @brief Username of post creator
     * @type std::string
     * 
     * PURPOSE: Attribution - who created this content
     * 
     * IMMUTABLE: Author shouldn't change after post creation
     * No setter provided (only set in constructor)
     * 
     * USAGE:
     * - Display: Show author name/profile
     * - Authorization: Check if user can edit/delete post
     * - Filtering: Show all posts by specific user
     * - Analytics: Post count per user
     */
    std::string author;
    
    /**
     * @brief The text content of the post
     * @type std::string
     * 
     * PURPOSE: The actual post message/content
     * 
     * SIZE LIMITS:
     * Should enforce limits (e.g., 280 chars like Twitter, or 5000 for long-form)
     * Current: No limit (should be added in validation)
     * 
     * RICH TEXT:
     * Current: Plain text
     * Could extend: Markdown, HTML (sanitized), @ mentions, # hashtags
     * 
     * SANITIZATION:
     * Escaped in toJson() to prevent JSON injection
     * Should also sanitize for XSS prevention in frontend
     */
    std::string content;
    
    /**
     * @brief Unix epoch timestamp when post was created
     * @type time_t
     * 
     * PURPOSE: Chronological ordering, display "posted X ago"
     * 
     * IMMUTABLE: Creation time shouldn't change
     * No setter provided (only set in constructor)
     * 
     * USAGE:
     * - Feed ordering: Show most recent posts first
     * - Time display: "2 hours ago", "March 15, 2024"
     * - Analytics: Posts per day/week/month
     * - Blockchain verification: Compare with block timestamp
     */
    time_t timestamp;
    
    /**
     * @brief Set of usernames who have liked this post
     * @type std::set<std::string> (automatically sorted, unique)
     * 
     * PURPOSE: Track which users liked the post (no duplicates)
     * 
     * WHY std::set (not vector):
     * - Automatic uniqueness: User can't like same post twice
     * - Fast lookup: O(log n) to check if user liked (hasLiked)
     * - Fast insert/delete: O(log n) for like/unlike operations
     * - Ordered: Users alphabetically sorted (consistent iteration)
     * 
     * ALTERNATIVE: std::unordered_set
     * - O(1) operations but unordered
     * - std::set chosen for consistent ordering (testability, display)
     * 
     * SIZE:
     * Could grow large for viral posts
     * Optimization: Store only count in memory, full list in database
     * 
     * USAGE:
     * - Count: likes.size() for display
     * - Check: likes.find(username) to see if user liked
     * - Add/Remove: likes.insert() / likes.erase()
     */
    std::set<std::string> likes;
    
    /**
     * @brief Ordered list of comments on this post
     * @type std::vector<Comment> (dynamic array, insertion-ordered)
     * 
     * PURPOSE: Store all comments/replies to this post
     * 
     * WHY std::vector:
     * - Insertion order preserved (chronological comments)
     * - Fast append: O(1) amortized for addComment
     * - Sequential access: O(1) for iteration/display
     * - Contiguous memory: Cache-friendly
     * 
     * ORDERING:
     * Comments maintained in chronological order (order of addition)
     * Could add sorting by likes, author, etc.
     * 
     * NESTED REPLIES:
     * Current: Flat structure (all comments at same level)
     * Extension: Tree structure for nested replies
     * 
     * SIZE CONSIDERATIONS:
     * Popular posts could have thousands of comments
     * Optimization: Pagination, lazy loading, database-backed
     * 
     * USAGE:
     * - Display: Show all comments under post
     * - Count: comments.size() for display
     * - Add: comments.emplace_back() or comments.push_back()
     */
    std::vector<Comment> comments;
    
    /**
     * @brief Hash of blockchain block containing this post's transaction
     * @type std::string (64-character hex SHA-256 hash, or empty)
     * 
     * PURPOSE: Links post to specific block in blockchain
     * 
     * LIFECYCLE:
     * - Initially empty: Post created but not yet mined into block
     * - After mining: Set to block's hash via setBlockchainHash()
     * - Immutable: Once set, shouldn't change (block hash is permanent)
     * 
     * VERIFICATION:
     * Can verify post hasn't been tampered with:
     * 1. Look up block by hash
     * 2. Find transaction in block matching this post
     * 3. Verify transaction data matches post content
     * 
     * USAGE:
     * - Display: Show "verified on blockchain" badge
     * - API: Include blockchain reference in response
     * - Verification: Prove post existed at specific time
     * - Audit: Trace post back to immutable blockchain record
     */
    std::string blockchainHash;  // Hash of the block containing this post
    
    /**
     * @brief Flag indicating if post has been recorded on blockchain
     * @type bool
     * 
     * PURPOSE: Quick check for blockchain confirmation status
     * 
     * STATES:
     * false: Post pending (in database, awaiting blockchain confirmation)
     * true: Post confirmed (mined into block, immutable)
     * 
     * RELATIONSHIP TO blockchainHash:
     * isOnChain = true → blockchainHash should be non-empty
     * isOnChain = false → blockchainHash should be empty
     * 
     * USAGE:
     * - Display: Show "pending" vs "confirmed" status
     * - Trust: Confirmed posts have higher authenticity
     * - API filtering: Show only confirmed posts
     * - Analytics: Track confirmation rate, time to confirmation
     * 
     * AUTOMATIC UPDATE:
     * Set to true automatically in setBlockchainHash()
     * Ensures consistency between flag and hash
     */
    bool isOnChain;

    /**
     * @brief Escapes special characters for valid JSON strings (same as Comment)
     * @param str String to escape
     * @return std::string - Escaped string safe for JSON
     * 
     * PURPOSE: Prevents JSON syntax errors in post content
     * SEE: Comment::escapeJson() for detailed documentation
     */
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

    // ========================================================================
    // PUBLIC INTERFACE (Constructors and Methods)
    // ========================================================================

public:
    /**
     * @brief Default constructor - creates empty post
     * PURPOSE: Needed for certain container operations
     * USAGE: Rare - typically use parameterized constructor
     */
    Post() : timestamp(std::time(nullptr)), isOnChain(false) {}

    /**
     * @brief Constructs a new Post with given parameters
     * @param id Unique identifier for the post
     * @param author Username of post creator
     * @param content Text content of the post
     * 
     * PURPOSE: Primary constructor for creating posts
     * 
     * INITIALIZATION:
     * - id, author, content: Copied from parameters
     * - timestamp: Auto-generated (current time)
     * - isOnChain: Initialized to false (pending blockchain confirmation)
     * - likes: Empty set (no likes initially)
     * - comments: Empty vector (no comments initially)
     * - blockchainHash: Empty string (no block yet)
     * 
     * POST-CONSTRUCTION WORKFLOW:
     * 1. Post created with this constructor
     * 2. Stored in database
     * 3. Transaction created for blockchain
     * 4. After mining, setBlockchainHash() called to link to block
     */
    Post(const std::string& id, const std::string& author, const std::string& content)
        : id(id), author(author), content(content), isOnChain(false) {
        timestamp = std::time(nullptr);
    }

    // ========================================================================
    // GETTER METHODS (Public Read-Only Access)
    // ========================================================================
    
    /** @brief Returns post ID */
    std::string getId() const { return id; }
    
    /** @brief Returns author username */
    std::string getAuthor() const { return author; }
    
    /** @brief Returns post content text */
    std::string getContent() const { return content; }
    
    /** @brief Returns creation timestamp */
    time_t getTimestamp() const { return timestamp; }
    
    /** @brief Returns const reference to likes set (avoids copy) */
    const std::set<std::string>& getLikes() const { return likes; }
    
    /** @brief Returns const reference to comments vector (avoids copy) */
    const std::vector<Comment>& getComments() const { return comments; }
    
    /** @brief Returns number of likes */
    int getLikeCount() const { return likes.size(); }
    
    /** @brief Returns number of comments */
    int getCommentCount() const { return comments.size(); }
    
    /** @brief Returns blockchain block hash (empty if not yet mined) */
    std::string getBlockchainHash() const { return blockchainHash; }
    
    /** @brief Returns true if post is on blockchain */
    bool getIsOnChain() const { return isOnChain; }

    // ========================================================================
    // SETTER METHODS (Limited - most fields immutable)
    // ========================================================================
    
    /**
     * @brief Sets blockchain hash and marks post as on-chain
     * @param hash SHA-256 hash of the block containing this post
     * 
     * PURPOSE: Links post to blockchain block after mining
     * 
     * CALLED BY: Mining process after block is successfully mined
     * 
     * SIDE EFFECTS:
     * - Sets blockchainHash to provided hash
     * - Automatically sets isOnChain to true (ensures consistency)
     * 
     * IMMUTABILITY:
     * Should only be called once (when post first mined)
     * Calling again would indicate error (post can't move between blocks)
     */
    void setBlockchainHash(const std::string& hash) {
        blockchainHash = hash;
        isOnChain = true;  // Automatically mark as on-chain
    }

    // ========================================================================
    // SOCIAL INTERACTION METHODS (Likes)
    // ========================================================================
    
    /**
     * @brief Adds a like from a user
     * @param username Username of user liking the post
     * @return bool - true if like added, false if user already liked
     * 
     * PURPOSE: Records user's like, prevents duplicate likes
     * 
     * IMPLEMENTATION:
     * Uses std::set::insert() which returns pair<iterator, bool>
     * - second element (bool): true if inserted, false if already present
     * 
     * IDEMPOTENT: Calling multiple times with same username has no effect
     * 
     * USAGE:
     * if (post.addLike("alice")) {
     *     // Like added successfully
     * } else {
     *     // User already liked this post
     * }
     */
    bool addLike(const std::string& username) {
        auto result = likes.insert(username);
        return result.second;  // true if inserted, false if already existed
    }

    /**
     * @brief Removes a like from a user (unlike)
     * @param username Username of user unliking the post
     * @return bool - true if like removed, false if user hadn't liked
     * 
     * PURPOSE: Allows users to remove their like
     * 
     * IMPLEMENTATION:
     * Uses std::set::erase() which returns count of elements removed
     * - Returns > 0 if element was removed
     * - Returns 0 if element not found
     * 
     * IDEMPOTENT: Calling multiple times with same username safe
     */
    bool removeLike(const std::string& username) {
        return likes.erase(username) > 0;
    }

    /**
     * @brief Checks if a user has liked this post
     * @param username Username to check
     * @return bool - true if user liked, false otherwise
     * 
     * PURPOSE: Determine like button state (liked vs not liked)
     * 
     * IMPLEMENTATION:
     * Uses std::set::find() which returns iterator
     * - If found: iterator != end()
     * - If not found: iterator == end()
     * 
     * TIME COMPLEXITY: O(log n) where n = number of likes
     * 
     * USAGE: UI shows filled heart if hasLiked returns true
     */
    bool hasLiked(const std::string& username) const {
        return likes.find(username) != likes.end();
    }

    // ========================================================================
    // SOCIAL INTERACTION METHODS (Comments)
    // ========================================================================
    
    /**
     * @brief Adds a comment to this post
     * @param author Username of commenter
     * @param content Text content of comment
     * 
     * PURPOSE: Allows users to reply to posts
     * 
     * IMPLEMENTATION:
     * Uses emplace_back() to construct Comment in-place
     * - More efficient than creating Comment then pushing
     * - Directly constructs using Comment constructor
     * 
     * ORDERING:
     * Comments appended to end (chronological order)
     * 
     * NO RETURN VALUE:
     * Unlike addLike, always succeeds (no duplicate prevention)
     * Users can comment multiple times
     * 
     * EXTENSIONS:
     * Could add: Comment editing, deletion, likes on comments
     */
    void addComment(const std::string& author, const std::string& content) {
        comments.emplace_back(author, content);
    }

    // ========================================================================
    // JSON SERIALIZATION METHODS
    // ========================================================================
    
    /**
     * @brief Serializes post to compact JSON format (summary)
     * @return std::string - JSON representation without full comment details
     * 
     * PURPOSE: Lightweight JSON for lists/feeds (reduces bandwidth)
     * 
     * OUTPUT FORMAT:
     * {
     *   "id": "post123",
     *   "author": "alice",
     *   "content": "Hello world!",
     *   "timestamp": 1698765432,
     *   "likes": 5,
     *   "comments": 3,
     *   "isOnChain": true,
     *   "blockchainHash": "0000a8f3c2..."  // Optional, only if on chain
     * }
     * 
     * USAGE:
     * - Feed/list views: GET /api/posts (shows many posts)
     * - Reduced payload: Counts instead of full comment/like details
     * - Fast: No iteration through comments
     * 
     * BLOCKCHAIN HASH:
     * Only included if post is on blockchain (conditional field)
     * Avoids empty string in JSON
     * 
     * ALTERNATIVE: toDetailedJson() includes full comment array
     */
    std::string toJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << id << "\",";
        ss << "\"author\":\"" << author << "\",";
        ss << "\"content\":\"" << escapeJson(content) << "\",";  // Escape for safety
        ss << "\"timestamp\":" << timestamp << ",";
        ss << "\"likes\":" << likes.size() << ",";              // Count only
        ss << "\"comments\":" << comments.size() << ",";        // Count only
        ss << "\"isOnChain\":" << (isOnChain ? "true" : "false");
        
        // Conditionally include blockchain hash (only if on chain)
        if (!blockchainHash.empty()) {
            ss << ",\"blockchainHash\":\"" << blockchainHash << "\"";
        }
        
        ss << "}";
        return ss.str();
    }

    /**
     * @brief Serializes post to detailed JSON format with full comments
     * @return std::string - JSON representation including comment array
     * 
     * PURPOSE: Complete post data for detail view
     * 
     * OUTPUT FORMAT:
     * {
     *   "id": "post123",
     *   "author": "alice",
     *   "content": "Hello world!",
     *   "timestamp": 1698765432,
     *   "likes": 5,
     *   "isOnChain": true,
     *   "blockchainHash": "0000a8f3...",
     *   "comments": [
     *     {"id":"bob-1698765440","author":"bob","content":"Nice!","timestamp":1698765440},
     *     {"id":"charlie-1698765450","author":"charlie","content":"Agree!","timestamp":1698765450}
     *   ]
     * }
     * 
     * USAGE:
     * - Detail view: GET /api/posts/{id} (single post)
     * - Full data: Includes all comments with details
     * - Larger payload: Not suitable for list views with many posts
     * 
     * COMMENT SERIALIZATION:
     * Calls Comment::toJson() for each comment
     * Comma-separated array (careful with last element)
     * 
     * SIZE CONSIDERATIONS:
     * Posts with hundreds of comments produce large JSON
     * Production: Implement comment pagination
     * 
     * ALTERNATIVE: toJson() provides lightweight summary
     */
    std::string toDetailedJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << id << "\",";
        ss << "\"author\":\"" << author << "\",";
        ss << "\"content\":\"" << escapeJson(content) << "\",";
        ss << "\"timestamp\":" << timestamp << ",";
        ss << "\"likes\":" << likes.size() << ",";
        ss << "\"isOnChain\":" << (isOnChain ? "true" : "false") << ",";
        
        // Conditionally include blockchain hash
        if (!blockchainHash.empty()) {
            ss << "\"blockchainHash\":\"" << blockchainHash << "\",";
        }
        
        // Add full comments array (detailed information)
        ss << "\"comments\":[";
        for (size_t i = 0; i < comments.size(); i++) {
            ss << comments[i].toJson();  // Serialize each comment
            if (i < comments.size() - 1) ss << ",";  // Comma between elements
        }
        ss << "]";
        
        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// END OF POST CLASS
// ============================================================================

#endif // POST_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * DESIGN PHILOSOPHY:
 * - Encapsulation: Private data with public getter/setter interface
 * - Social features: Likes (set), comments (vector) for engagement
 * - Blockchain integration: Links to immutable blockchain record
 * - Dual storage: Database (performance) + Blockchain (integrity)
 * 
 * DATA STRUCTURES CHOICE:
 * - std::set for likes: Uniqueness, fast lookup/insert/erase
 * - std::vector for comments: Chronological ordering, fast append
 * - std::string for IDs: Flexible format, human-readable
 * 
 * SECURITY CONSIDERATIONS:
 * - JSON escaping: Prevents injection attacks
 * - Const methods: Prevents accidental modification
 * - Limited setters: Most fields immutable after creation
 * - Blockchain verification: Posts can be verified against chain
 * 
 * PERFORMANCE CONSIDERATIONS:
 * - toJson() lightweight: For list views (counts only)
 * - toDetailedJson() comprehensive: For detail views (full data)
 * - Const references in getters: Avoids copying containers
 * - emplace_back for comments: In-place construction
 * 
 * SCALABILITY:
 * - Viral posts: Likes/comments could grow very large
 * - Optimization: Store counts only, paginate full lists
 * - Database backed: Don't keep all data in memory
 * - Caching: Cache popular posts for fast access
 * 
 * BLOCKCHAIN INTEGRATION:
 * - Pending state: isOnChain = false initially
 * - Confirmation: isOnChain = true after mining
 * - Verification: blockchainHash links to immutable record
 * - Audit trail: Can reconstruct post history from blockchain
 * 
 * EXTENSIBILITY:
 * - Rich content: Add media attachments, links, embeds
 * - Nested comments: Tree structure for threaded replies
 * - Reactions: Beyond binary like (emoji reactions)
 * - Edit history: Track post modifications
 * - Privacy: Public/private/followers-only posts
 * - Tags/categories: Organize posts by topic
 * - Search: Full-text search on content
 * 
 * COMPARISON TO OTHER PLATFORMS:
 * 
 * TWITTER/X:
 * - Similar: Posts (tweets), likes, replies
 * - Different: No retweets (could add), character limits
 * 
 * FACEBOOK:
 * - Similar: Posts, likes, comments
 * - Different: No reactions, shares, privacy settings
 * 
 * REDDIT:
 * - Similar: Posts, upvotes (likes), comments
 * - Different: No downvotes, nested comment trees
 * 
 * STEEMIT/HIVE:
 * - Similar: Blockchain-based social media
 * - Different: They have cryptocurrency rewards, voting
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: Like/unlike, add comments, JSON serialization
 * - Edge cases: Empty posts, very long content, many likes/comments
 * - Security: JSON injection attempts, XSS prevention
 * - Performance: Large like sets, comment arrays
 * - Blockchain: Verify hash linking, state transitions
 ******************************************************************************/

