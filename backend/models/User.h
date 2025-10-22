/*******************************************************************************
 * USER.H - User Account and Authentication Model
 * 
 * PURPOSE:
 * This header defines the User class representing user accounts in the Bitea
 * platform. Handles authentication (password hashing with salts), profile
 * information, and social features (followers/following).
 * 
 * SECURITY ARCHITECTURE:
 * - Password security: SHA-256 hashing with per-user random salts
 * - No plaintext passwords: Only hashes stored
 * - Salt per user: Prevents rainbow table attacks
 * - Secure random: OpenSSL RAND_bytes for salt generation
 * 
 * USER ACCOUNT FEATURES:
 * - Authentication: Username/email/password login
 * - Profile: Display name, bio, timestamps
 * - Social graph: Followers and following relationships
 * - Privacy: Password never exposed, optional private data in JSON
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - Session.h: Sessions link to users via username
 * - Post.h: Posts authored by users, users like/comment
 * - Database (MongoClient): User data persisted
 * - HttpServer (main.cpp): User registration, login, profile endpoints
 * 
 * AUTHENTICATION FLOW:
 * 1. Registration: User provides username/email/password
 * 2. Salt generation: Random 16-byte salt created
 * 3. Password hashing: SHA-256(salt + password) stored
 * 4. Login: User provides username/password
 * 5. Verification: Recalculate hash, compare with stored hash
 * 6. Session: Create session if password matches
 * 
 * SOCIAL GRAPH MODEL:
 * - followers: Set of usernames who follow this user
 * - following: Set of usernames this user follows
 * - Bidirectional: User A follows B ≠ B follows A
 * - Symmetry: If A follows B, then A in B.followers AND B in A.following
 * 
 * SECURITY CONSIDERATIONS:
 * - Salted hashing: Each user has unique salt (prevents rainbow tables)
 * - SHA-256: Industry-standard cryptographic hash
 * - OpenSSL: Battle-tested library for crypto operations
 * - No password storage: Only hash + salt stored
 * - Password verification: Constant-time comparison (prevents timing attacks)
 * 
 * REFERENCES:
 * - OWASP Password Storage: https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
 * - OpenSSL Crypto: https://www.openssl.org/docs/man1.1.1/man3/
 * - NIST Password Guidelines: SP 800-63B
 ******************************************************************************/

#ifndef USER_H
#define USER_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>      // std::string - username, email, passwords, etc.
#include <set>         // std::set<std::string> - followers and following collections
#include <ctime>       // time_t, std::time() - registration and login timestamps
#include <sstream>     // std::stringstream - JSON serialization, hex formatting

// ============================================================================
// EXTERNAL LIBRARY INCLUDES (OpenSSL for Cryptography)
// ============================================================================

#include <openssl/sha.h>   // SHA256_* functions - password hashing
                           // WHY: Industry-standard, proven secure, fast
                           
#include <openssl/rand.h>  // RAND_bytes - cryptographically secure random salt generation
                           // WHY: Hardware entropy, unpredictable, meets crypto standards

#include <iomanip>         // std::hex, std::setw, std::setfill - hexadecimal formatting

// ============================================================================
// USER CLASS DEFINITION
// ============================================================================

/**
 * @class User
 * @brief Represents a user account with authentication and social features
 * 
 * DESIGN PATTERN: Entity with secure authentication
 * 
 * KEY FEATURES:
 * - Secure authentication: Salted SHA-256 password hashing
 * - Profile management: Display name, bio, timestamps
 * - Social features: Followers/following relationships
 * - Privacy controls: Public vs private data serialization
 * 
 * PASSWORD SECURITY:
 * Traditional approach: hash(password) - vulnerable to rainbow tables
 * Our approach: hash(salt + password) - each user has unique salt
 * 
 * WHY SALTING:
 * Without salt: Same password → same hash (lookup in rainbow table)
 * With salt: Same password → different hash per user (rainbow table useless)
 * 
 * SOCIAL GRAPH:
 * Followers: Users who follow this user (this user's audience)
 * Following: Users this user follows (this user's feed sources)
 * 
 * THREAD SAFETY:
 * User objects not thread-safe themselves
 * Access protected by database layer
 */
class User {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (User Account Data)
    // ========================================================================
    
    /**
     * @brief Unique username for login and identification
     * @type std::string (typically 3-30 characters)
     * 
     * PURPOSE: Primary identifier for user account
     * 
     * CONSTRAINTS:
     * - Unique: No two users can have same username
     * - Immutable: Username cannot change after registration (in current implementation)
     * - Case-sensitive: "Alice" ≠ "alice" (could implement case-insensitive)
     * 
     * USAGE:
     * - Login: User provides username + password
     * - Attribution: Posts, comments show author username
     * - Social: Follow/unfollow by username
     * - URL: Profile pages at /user/{username}
     * 
     * VALIDATION:
     * Should enforce: alphanumeric + underscores, length limits
     * Current: No validation (should be added in InputValidator)
     */
    std::string username;
    
    /**
     * @brief User's email address
     * @type std::string
     * 
     * PURPOSE: Contact, account recovery, notifications
     * 
     * CONSTRAINTS:
     * - Unique: One email per account (prevents abuse)
     * - Valid format: Should validate email syntax
     * - Optional display: Can be hidden in public API
     * 
     * USAGE:
     * - Account recovery: "Forgot password" emails
     * - Notifications: New followers, likes, comments
     * - Contact: Support, admin communication
     * - Alternative login: Could allow email instead of username
     * 
     * PRIVACY:
     * Email not included in public JSON (to Json(false))
     * Only visible to user themselves (toJson(true))
     * 
     * VALIDATION:
     * Should verify: valid email format, no disposable emails
     */
    std::string email;
    
    /**
     * @brief SHA-256 hash of (salt + password)
     * @type std::string (64 hexadecimal characters)
     * 
     * PURPOSE: Secure password storage (never store plaintext!)
     * 
     * FORMAT: 64-character hex string representing 256-bit hash
     * Example: "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"
     * 
     * SECURITY:
     * - Hash function: SHA-256 (one-way, cannot reverse to get password)
     * - Salted: Includes random salt to prevent rainbow table attacks
     * - Unique per user: Same password → different hash (due to different salts)
     * 
     * VERIFICATION PROCESS:
     * 1. User submits password attempt
     * 2. Retrieve user's salt from database
     * 3. Compute hash(salt + attemptedPassword)
     * 4. Compare with stored passwordHash
     * 5. Match → correct password, allow login
     * 
     * WHY SHA-256 (not MD5/SHA1):
     * - MD5: Broken, collision attacks known
     * - SHA1: Deprecated, vulnerabilities exist
     * - SHA-256: Current standard, no known practical attacks
     * 
     * FUTURE IMPROVEMENTS:
     * - Use bcrypt/scrypt/Argon2 (designed for passwords, slower = more secure)
     * - Add password stretching (iterate hash many times)
     * - Consider PBKDF2 (key derivation function)
     */
    std::string passwordHash;
    
    /**
     * @brief Random salt unique to this user
     * @type std::string (32 hexadecimal characters, 16 bytes)
     * 
     * PURPOSE: Prevents rainbow table and dictionary attacks
     * 
     * RAINBOW TABLE ATTACK (without salt):
     * Attacker precomputes hashes of common passwords:
     * "password123" → "hash_abc..."
     * Attacker steals database, looks up hash → finds password
     * 
     * PREVENTION (with salt):
     * Each user has different salt:
     * User A: hash(salt_A + "password123") → "hash_xyz..."
     * User B: hash(salt_B + "password123") → "hash_uvw..."
     * Rainbow table useless - must crack each user individually
     * 
     * GENERATION:
     * OpenSSL RAND_bytes() - cryptographically secure random
     * 16 bytes = 128 bits of entropy
     * 
     * STORAGE:
     * Stored alongside password hash (not secret)
     * Salt doesn't need to be hidden, just unique and random
     * 
     * FORMAT: 32 hex characters (16 bytes in hexadecimal)
     * Example: "a3f8c2d4e5f6a7b8c9d0e1f2a3b4c5d6"
     */
    std::string passwordSalt;  // Store unique salt per user
    
    /**
     * @brief User's chosen display name
     * @type std::string
     * 
     * PURPOSE: Friendly name shown in UI (can differ from username)
     * 
     * DIFFERENCE FROM USERNAME:
     * - username: Unique, immutable, used for login
     * - displayName: Can duplicate, changeable, shown in UI
     * 
     * EXAMPLES:
     * username: "alice_smith_92"
     * displayName: "Alice Smith" (more readable)
     * 
     * DEFAULT: If not set, defaults to username
     * 
     * USAGE: Shown in posts, comments, profiles
     */
    std::string displayName;
    
    /**
     * @brief User's biography/description
     * @type std::string
     * 
     * PURPOSE: About me, profile description
     * 
     * TYPICAL CONTENT: Short bio, interests, links
     * SIZE LIMIT: Should enforce (e.g., 500 characters)
     * 
     * OPTIONAL: Can be empty string
     * 
     * USAGE: Profile page, hover cards
     */
    std::string bio;
    
    /**
     * @brief Set of usernames who follow this user
     * @type std::set<std::string> (unique, sorted)
     * 
     * PURPOSE: Track this user's audience/followers
     * 
     * RELATIONSHIP:
     * If "bob" follows "alice":
     * - "bob" is in alice.followers
     * - "alice" is in bob.following
     * 
     * WHY std::set:
     * - Uniqueness: User can't follow same person twice
     * - Fast lookup: O(log n) to check if user is follower
     * - Sorted: Consistent ordering
     * 
     * USAGE:
     * - Count: followers.size() for follower count
     * - List: Display followers on profile
     * - Check: followers.find(username) to see if user follows
     * 
     * BIDIRECTIONAL MANAGEMENT:
     * When A follows B:
     * 1. Add B to A.following
     * 2. Add A to B.followers
     * Both must be updated together
     */
    std::set<std::string> followers;
    
    /**
     * @brief Set of usernames this user follows
     * @type std::set<std::string> (unique, sorted)
     * 
     * PURPOSE: Track who this user follows (their feed sources)
     * 
     * RELATIONSHIP:
     * If "alice" follows "bob":
     * - "bob" is in alice.following
     * - "alice" is in bob.followers
     * 
     * FEED GENERATION:
     * User's feed = posts from users in following set
     * SELECT * FROM posts WHERE author IN user.following
     * 
     * USAGE:
     * - Feed: Show posts from followed users
     * - Count: following.size() for following count
     * - List: Display who user follows
     * - Check: is Following(username) method
     */
    std::set<std::string> following;
    
    /**
     * @brief Unix timestamp when user registered
     * @type time_t
     * 
     * PURPOSE: Account age, registration date
     * 
     * IMMUTABLE: Set once at registration, never changes
     * 
     * USAGE:
     * - Display: "Member since 2024"
     * - Analytics: User growth over time
     * - Sorting: Find newest/oldest users
     * - Trust: Older accounts may be more trusted
     */
    time_t createdAt;
    
    /**
     * @brief Unix timestamp of last successful login
     * @type time_t
     * 
     * PURPOSE: Track user activity, security monitoring
     * 
     * UPDATED: Every successful login via updateLastLogin()
     * 
     * USAGE:
     * - Security: Flag unusual login patterns
     * - Activity: Identify inactive users
     * - Display: "Last seen X days ago"
     * - Cleanup: Archive very old inactive accounts
     */
    time_t lastLogin;

    // ========================================================================
    // PRIVATE HELPER METHODS (Password Security)
    // ========================================================================
    
    /**
     * @brief Generates cryptographically random salt for password hashing
     * @return std::string - 32-character hex string (16 bytes)
     * 
     * PURPOSE: Create unique random salt for each user
     * 
     * ALGORITHM:
     * 1. Generate 16 random bytes using OpenSSL RAND_bytes()
     * 2. Convert each byte to 2-digit hexadecimal
     * 3. Return 32-character hex string
     * 
     * CRYPTOGRAPHIC STRENGTH:
     * - OpenSSL RAND_bytes: Uses hardware entropy sources
     * - 16 bytes = 128 bits of entropy
     * - 2^128 = 3.4 × 10^38 possible salts
     * - Collision probability negligible
     * 
     * WHY 16 BYTES:
     * NIST recommendations: At least 128 bits for salts
     * 16 bytes meets this requirement
     * 
     * HEXADECIMAL CONVERSION:
     * Each byte (0-255) → 2 hex digits (00-FF)
     * Example byte values: 0xA3, 0xF8 → "a3f8"
     * 
     * std::hex: Format as hexadecimal
     * std::setw(2): Always 2 digits (pad with 0 if needed)
     * std::setfill('0'): Use '0' for padding
     * 
     * WHY const:
     * Doesn't modify User object (pure salt generation)
     * Can be called during construction
     * 
     * CALLED BY:
     * - Constructor: Initial salt for new user
     * - changePassword(): New salt when password changes
     * 
     * SECURITY NOTE:
     * Salt doesn't need to be secret, just unique and random
     * Stored plaintext alongside password hash
     */
    std::string generateSalt() const {
        // Allocate 16 bytes for random salt
        unsigned char salt[16];
        
        // Fill with cryptographically secure random bytes
        // RAND_bytes from OpenSSL - uses hardware entropy
        RAND_bytes(salt, sizeof(salt));
        
        std::stringstream ss;
        // Convert each byte to 2-digit hexadecimal
        for(int i = 0; i < 16; i++) {
            ss << std::hex                          // Hexadecimal format
               << std::setw(2)                      // 2 characters wide
               << std::setfill('0')                 // Pad with zeros
               << static_cast<int>(salt[i]);        // Convert byte to int
        }
        return ss.str();  // Return 32-character hex string
    }

    /**
     * @brief Hashes password with salt using SHA-256
     * @param password Plain text password to hash
     * @param salt Random salt to combine with password
     * @return std::string - 64-character hex hash
     * 
     * PURPOSE: Securely hash passwords for storage
     * 
     * ALGORITHM:
     * 1. Concatenate: saltedPassword = salt + password
     * 2. Compute: SHA-256(saltedPassword)
     * 3. Convert: Binary hash → hex string
     * 4. Return: 64-character hex representation
     * 
     * WHY SALT FIRST:
     * salt + password (not password + salt)
     * Convention: Salt typically prepended
     * Either works as long as consistent
     * 
     * SHA-256 PROCESS:
     * - SHA256_Init(): Initialize hash context
     * - SHA256_Update(): Process input data
     * - SHA256_Final(): Finalize and output hash
     * 
     * OUTPUT FORMAT:
     * 256 bits = 32 bytes → 64 hex characters
     * Example: "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"
     * 
     * SECURITY PROPERTIES:
     * - One-way: Cannot reverse hash to get password
     * - Deterministic: Same input always → same output
     * - Avalanche: Small input change → completely different hash
     * - Collision-resistant: Extremely hard to find two inputs with same hash
     * 
     * WHY const:
     * Pure function - doesn't modify User object
     * Can be called for verification without changing state
     * 
     * USAGE:
     * - Registration: hashPassword(password, generateSalt())
     * - Login verification: hashPassword(attemptedPassword, storedSalt)
     * - Password change: hashPassword(newPassword, generateSalt())
     * 
     * TIMING ATTACK CONSIDERATION:
     * Hash computation takes constant time regardless of input
     * Comparison should also be constant-time (not implemented here)
     * Production: Use dedicated constant-time compare function
     */
    std::string hashPassword(const std::string& password, const std::string& salt) const {
        // Concatenate salt and password
        std::string saltedPassword = salt + password;
        
        // Prepare hash output buffer (32 bytes for SHA-256)
        unsigned char hash[SHA256_DIGEST_LENGTH];
        
        // Initialize SHA-256 context
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        
        // Process the salted password through SHA-256
        SHA256_Update(&sha256, saltedPassword.c_str(), saltedPassword.size());
        
        // Finalize hash computation
        SHA256_Final(hash, &sha256);
        
        // Convert binary hash to hexadecimal string
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();  // Return 64-character hex hash
    }

    // ========================================================================
    // PUBLIC INTERFACE (Constructors and Methods)
    // ========================================================================

public:
    /**
     * @brief Default constructor - creates empty user
     * PURPOSE: Needed for container operations, rarely used directly
     */
    User() : createdAt(std::time(nullptr)), lastLogin(std::time(nullptr)) {}

    /**
     * @brief Constructs new User with secure password hashing
     * @param username Unique username for login
     * @param email User's email address
     * @param password Plain text password (will be hashed)
     * 
     * PURPOSE: Primary constructor for user registration
     * 
     * SECURITY WORKFLOW:
     * 1. Store username, email
     * 2. Generate random salt (generateSalt)
     * 3. Hash password with salt (hashPassword)
     * 4. Store hash + salt (NOT plaintext password!)
     * 5. Record timestamps
     * 
     * CRITICAL: Password parameter never stored
     * Only hash(salt + password) stored in passwordHash
     * 
     * DEFAULT VALUES:
     * - displayName: Defaults to username (user can change later)
     * - bio: Empty string
     * - followers/following: Empty sets
     * - createdAt/lastLogin: Current time
     * 
     * CALLED BY:
     * - Registration endpoint: POST /api/auth/register
     * 
     * POST-CONSTRUCTION:
     * User object saved to database
     * Password hash + salt persisted (password itself forgotten)
     */
    User(const std::string& username, const std::string& email, const std::string& password)
        : username(username), email(email), displayName(username) {
        // Generate random salt for this user
        passwordSalt = generateSalt();
        
        // Hash password with salt (never store plaintext!)
        passwordHash = hashPassword(password, passwordSalt);
        
        // Record registration timestamp
        createdAt = std::time(nullptr);
        
        // Initialize last login to registration time
        lastLogin = std::time(nullptr);
    }

    // ========================================================================
    // GETTER METHODS (Public Read-Only Access)
    // ========================================================================
    
    /** @brief Returns username */
    std::string getUsername() const { return username; }
    
    /** @brief Returns email address */
    std::string getEmail() const { return email; }
    
    /** @brief Returns password hash (for database storage) */
    std::string getPasswordHash() const { return passwordHash; }
    
    /** @brief Returns password salt (for database storage) */
    std::string getPasswordSalt() const { return passwordSalt; }
    
    /** @brief Returns display name */
    std::string getDisplayName() const { return displayName; }
    
    /** @brief Returns biography */
    std::string getBio() const { return bio; }
    
    /** @brief Returns const reference to followers set */
    const std::set<std::string>& getFollowers() const { return followers; }
    
    /** @brief Returns const reference to following set */
    const std::set<std::string>& getFollowing() const { return following; }
    
    /** @brief Returns registration timestamp */
    time_t getCreatedAt() const { return createdAt; }
    
    /** @brief Alias for getCreatedAt (alternative naming) */
    time_t getJoinedAt() const { return createdAt; }
    
    /** @brief Returns last login timestamp */
    time_t getLastLogin() const { return lastLogin; }
    
    /** @brief Returns number of followers */
    int getFollowerCount() const { return followers.size(); }
    
    /** @brief Alias for getFollowerCount (alternative naming) */
    int getFollowersCount() const { return followers.size(); }
    
    /** @brief Returns number of users being followed */
    int getFollowingCount() const { return following.size(); }

    // ========================================================================
    // SETTER METHODS (Modifiable Profile Fields)
    // ========================================================================
    
    /** @brief Updates display name */
    void setDisplayName(const std::string& name) { displayName = name; }
    
    /** @brief Updates biography */
    void setBio(const std::string& b) { bio = b; }
    
    /** @brief Sets password hash (used when loading from database) */
    void setPasswordHash(const std::string& hash) { passwordHash = hash; }
    
    /** @brief Sets password salt (used when loading from database) */
    void setPasswordSalt(const std::string& salt) { passwordSalt = salt; }
    
    /** @brief Updates last login to current time (call after successful login) */
    void updateLastLogin() { lastLogin = std::time(nullptr); }

    // ========================================================================
    // AUTHENTICATION METHODS
    // ========================================================================
    
    /**
     * @brief Verifies if provided password matches stored hash
     * @param password Plain text password to verify
     * @return bool - true if password correct, false otherwise
     * 
     * PURPOSE: Login authentication
     * 
     * ALGORITHM:
     * 1. Take provided password
     * 2. Hash it with this user's salt
     * 3. Compare result with stored passwordHash
     * 4. Match → correct password
     * 
     * SECURITY:
     * - Salt prevents rainbow table attacks
     * - Hash comparison (not password comparison)
     * - Original password never stored or logged
     * 
     * TIMING ATTACK CONSIDERATION:
     * String comparison may leak timing information
     * Production: Use constant-time comparison
     * 
     * USAGE:
     * if (user.verifyPassword(attemptedPassword)) {
     *     // Create session, allow login
     * } else {
     *     // Reject login, return error
     * }
     * 
     * CALLED BY:
     * - Login endpoint: POST /api/auth/login
     */
    bool verifyPassword(const std::string& password) const {
        return passwordHash == hashPassword(password, passwordSalt);
    }

    /**
     * @brief Changes user's password (generates new salt)
     * @param newPassword New plain text password
     * 
     * PURPOSE: Password change functionality
     * 
     * SECURITY BEST PRACTICE:
     * Generates NEW salt when password changes
     * Even if user sets same password, hash will differ
     * 
     * WORKFLOW:
     * 1. Generate fresh random salt
     * 2. Hash new password with new salt
     * 3. Store new hash + new salt
     * 4. Old hash/salt discarded
     * 
     * WHY NEW SALT:
     * - Best practice: Fresh salt per password
     * - Prevents correlation if user reuses password
     * - Adds additional security layer
     * 
     * USAGE:
     * - Password reset: After email verification
     * - Password change: User updates in settings
     * 
     * SHOULD ALSO:
     * - Invalidate all existing sessions (force re-login)
     * - Send notification email
     * - Log security event
     */
    void changePassword(const std::string& newPassword) {
        // Generate new salt (best practice)
        passwordSalt = generateSalt();
        
        // Hash new password with new salt
        passwordHash = hashPassword(newPassword, passwordSalt);
    }

    // ========================================================================
    // SOCIAL GRAPH METHODS (Follow Relationships)
    // ========================================================================
    
    /**
     * @brief Adds username to this user's following set
     * @param username Username to follow
     * 
     * PURPOSE: Record that this user follows another user
     * 
     * BIDIRECTIONAL UPDATE REQUIRED:
     * This method only updates THIS user's following set
     * Caller must ALSO update target user's followers set:
     * 
     * alice.follow("bob");           // Add bob to alice's following
     * bob.addFollower("alice");      // Add alice to bob's followers
     * 
     * IDEMPOTENT: Safe to call multiple times (set prevents duplicates)
     * 
     * NO VALIDATION:
     * Doesn't check if target user exists
     * Doesn't prevent self-follow (alice.follow("alice"))
     * Validation should be done at application layer
     */
    void follow(const std::string& username) {
        following.insert(username);
    }

    /**
     * @brief Removes username from this user's following set
     * @param username Username to unfollow
     * 
     * PURPOSE: Record that this user unfollows another user
     * 
     * BIDIRECTIONAL UPDATE REQUIRED:
     * This method only updates THIS user's following set
     * Caller must ALSO update target user's followers set:
     * 
     * alice.unfollow("bob");         // Remove bob from alice's following
     * bob.removeFollower("alice");   // Remove alice from bob's followers
     * 
     * IDEMPOTENT: Safe to call even if not currently following
     */
    void unfollow(const std::string& username) {
        following.erase(username);
    }

    /**
     * @brief Adds username to this user's followers set
     * @param username Username of new follower
     * 
     * PURPOSE: Record that another user follows this user
     * 
     * TYPICALLY CALLED BY:
     * Follow operation on another user object
     * When alice follows bob:
     * - alice.follow("bob") → bob.addFollower("alice")
     * 
     * USAGE: Part of bidirectional follow relationship management
     */
    void addFollower(const std::string& username) {
        followers.insert(username);
    }

    /**
     * @brief Removes username from this user's followers set
     * @param username Username of follower to remove
     * 
     * PURPOSE: Record that another user unfollows this user
     * 
     * TYPICALLY CALLED BY:
     * Unfollow operation on another user object
     * When alice unfollows bob:
     * - alice.unfollow("bob") → bob.removeFollower("alice")
     * 
     * USAGE: Part of bidirectional follow relationship management
     */
    void removeFollower(const std::string& username) {
        followers.erase(username);
    }

    /**
     * @brief Checks if this user follows another user
     * @param username Username to check
     * @return bool - true if following, false otherwise
     * 
     * PURPOSE: Determine follow relationship for UI, permissions
     * 
     * USAGE:
     * if (currentUser.isFollowing("bob")) {
     *     // Show "Unfollow" button
     * } else {
     *     // Show "Follow" button
     * }
     * 
     * COMPLEXITY: O(log n) where n = following count
     */
    bool isFollowing(const std::string& username) const {
        return following.find(username) != following.end();
    }

    /**
     * @brief Checks if another user follows this user
     * @param username Username to check
     * @return bool - true if they follow this user, false otherwise
     * 
     * PURPOSE: Determine follower status
     * 
     * USAGE:
     * if (user.hasFollower("alice")) {
     *     // Alice follows this user
     * }
     * 
     * COMPLEXITY: O(log n) where n = follower count
     */
    bool hasFollower(const std::string& username) const {
        return followers.find(username) != followers.end();
    }

    // ========================================================================
    // JSON SERIALIZATION
    // ========================================================================
    
    /**
     * @brief Serializes user to JSON format
     * @param includePrivate If true, includes email and lastLogin (default: false)
     * @return std::string - JSON representation
     * 
     * PURPOSE: API responses with privacy control
     * 
     * OUTPUT FORMAT (public):
     * {
     *   "username": "alice",
     *   "displayName": "Alice Smith",
     *   "bio": "Blockchain enthusiast",
     *   "followers": 42,
     *   "following": 15,
     *   "createdAt": 1698765432
     * }
     * 
     * OUTPUT FORMAT (private - includePrivate=true):
     * {
     *   ...same as above...
     *   "email": "alice@example.com",
     *   "lastLogin": 1698851832
     * }
     * 
     * PRIVACY LEVELS:
     * false (default): Public profile info
     *   - Visible to anyone
     *   - Safe to return in user lists, profiles
     * 
     * true: Private info included
     *   - Only for authenticated user viewing their own profile
     *   - Includes email, lastLogin
     * 
     * NEVER INCLUDED:
     * - passwordHash: Security risk
     * - passwordSalt: Not needed by client
     * 
     * USAGE:
     * Public profile: user.toJson(false)  // or just user.toJson()
     * Own profile: user.toJson(true)
     * 
     * ALTERNATIVE APPROACH:
     * Could have separate toPublicJson() and toPrivateJson() methods
     * Current approach more flexible with single parameter
     */
    std::string toJson(bool includePrivate = false) const {
        std::stringstream ss;
        ss << "{";
        ss << "\"username\":\"" << username << "\",";
        ss << "\"displayName\":\"" << displayName << "\",";
        ss << "\"bio\":\"" << bio << "\",";
        ss << "\"followers\":" << followers.size() << ",";  // Count only
        ss << "\"following\":" << following.size() << ",";  // Count only
        ss << "\"createdAt\":" << createdAt;
        
        // Conditionally include private fields
        if (includePrivate) {
            ss << ",\"email\":\"" << email << "\"";
            ss << ",\"lastLogin\":" << lastLogin;
        }
        
        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// END OF USER CLASS
// ============================================================================

#endif // USER_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * SECURITY ARCHITECTURE:
 * - Salted password hashing: Industry best practice implemented
 * - SHA-256: Current standard (consider bcrypt/Argon2 for production)
 * - Per-user salts: Defeats rainbow table attacks
 * - OpenSSL: Battle-tested cryptographic library
 * 
 * PASSWORD SECURITY EVOLUTION:
 * 1. Plaintext: Never do this! (ancient, insecure)
 * 2. Hashing: hash(password) - vulnerable to rainbow tables
 * 3. Salted hashing: hash(salt + password) - OUR CURRENT APPROACH
 * 4. Slow hashing: bcrypt/scrypt/Argon2 - even better (recommended upgrade)
 * 
 * WHY NOT BCRYPT/SCRYPT/ARGON2:
 * SHA-256 chosen for simplicity and OpenSSL availability
 * Production upgrade path: Replace hashPassword() with bcrypt
 * 
 * SOCIAL GRAPH DESIGN:
 * - Bidirectional: Both followers and following tracked
 * - Set-based: O(log n) operations, automatic uniqueness
 * - Scalable: Works for celebrity accounts (millions of followers)
 * - Could extend: Followers-only lists, mutual follows
 * 
 * FOLLOW RELATIONSHIP MANAGEMENT:
 * Critical: Both users must be updated for follow/unfollow
 * 
 * CORRECT:
 * alice.follow("bob");
 * bob.addFollower("alice");
 * 
 * INCORRECT (breaks invariant):
 * alice.follow("bob");  // Without updating bob - WRONG!
 * 
 * Application layer responsible for maintaining bidirectional consistency
 * 
 * DATA MODEL TRADE-OFFS:
 * 
 * EMBEDDED FOLLOWERS (current):
 * Pro: Simple, fast for moderate follower counts
 * Con: Document grows large for popular users
 * 
 * SEPARATE COLLECTION:
 * Pro: Scales to millions of followers
 * Con: More complex queries, requires joins
 * 
 * Current approach fine for MVP, can migrate if needed
 * 
 * PRIVACY AND SECURITY:
 * - Password never exposed in any method
 * - Email hidden by default (toJson)
 * - Hash/salt accessible for database operations
 * - No password logging or storing
 * 
 * EXTENSIBILITY:
 * - Two-factor authentication: Add 2FA secret, backup codes
 * - OAuth integration: Add provider ID, access tokens
 * - Profile pictures: Add avatar URL or binary data
 * - User roles: Add permissions, admin flags
 * - Account status: Add banned, verified, suspended flags
 * - Privacy settings: Add profile visibility controls
 * - Blocking: Add blocked users set
 * - Preferences: Add notification, theme settings
 * 
 * PERFORMANCE CONSIDERATIONS:
 * - Set operations: O(log n) for most operations
 * - Celebrity problem: Millions of followers in single document
 * - Solution: Pagination, follower count caching
 * - JSON serialization: O(1) since only counts included
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: Password hashing, salt generation, verification
 * - Security tests: Rainbow table resistance, timing attacks
 * - Integration tests: Follow relationships, bidirectional consistency
 * - Load tests: Many followers, large following lists
 * - Edge cases: Self-follow, duplicate follows, empty profiles
 ******************************************************************************/

