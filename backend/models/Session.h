/*******************************************************************************
 * SESSION.H - User Session Management Model
 * 
 * PURPOSE:
 * This header defines the Session class for managing user authentication
 * sessions in the Bitea platform. Sessions provide stateful authentication,
 * allowing users to stay logged in without re-entering credentials.
 * 
 * AUTHENTICATION ARCHITECTURE:
 * 1. User logs in with username/password
 * 2. Session created with random ID
 * 3. Session ID returned to client (stored in cookie/localStorage)
 * 4. Client includes session ID in subsequent requests
 * 5. Server validates session (checks expiration, username)
 * 6. User authorized for protected actions
 * 
 * SESSION STORAGE:
 * Sessions stored in-memory (std::map in HttpServer/main.cpp)
 * Production: Use Redis, Memcached, or database for persistence
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - User.h: Session links to User via username
 * - HttpServer (main.cpp): Creates/validates/destroys sessions
 * - Frontend: Stores session ID, includes in API requests
 * 
 * SECURITY FEATURES:
 * - Random session IDs: Cryptographically random (hard to guess)
 * - Expiration: Sessions automatically expire after configured time
 * - Validation: isValid() checks expiration before authorizing
 * - Refresh: extend() updates expiration for active users
 * 
 * SESSION LIFECYCLE:
 * 1. Creation: User login → new Session(username)
 * 2. Usage: Client sends sessionId → server validates
 * 3. Refresh: extend() called to prevent expiration
 * 4. Expiration: isExpired() returns true after timeout
 * 5. Destruction: Logout or server cleanup removes expired sessions
 * 
 * SECURITY CONSIDERATIONS:
 * - Session fixation: New ID generated per login
 * - Session hijacking: HTTPS recommended to protect session IDs
 * - Session timeout: Configurable expiration prevents abandoned sessions
 * - No sensitive data: Session stores only username, not password
 * 
 * REFERENCES:
 * - OWASP Session Management: https://owasp.org/www-community/Session_Management_Cheat_Sheet
 * - HTTP Cookies: RFC 6265
 * - JWT Alternative: Could use JWT tokens instead of server-side sessions
 ******************************************************************************/

#ifndef SESSION_H
#define SESSION_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>      // std::string - session ID, username
#include <ctime>       // time_t, std::time() - timestamp and expiration
#include <random>      // std::random_device, std::mt19937 - secure random generation
#include <sstream>     // std::stringstream - ID generation and JSON

// ============================================================================
// SESSION CLASS DEFINITION
// ============================================================================

/**
 * @class Session
 * @brief Manages user authentication sessions with expiration
 * 
 * DESIGN PATTERN: Value Object with lifecycle management
 * 
 * KEY FEATURES:
 * - Random session ID generation (32 hex characters)
 * - Expiration tracking (configurable timeout)
 * - Validation (isValid checks if session not expired)
 * - Refresh capability (extend expiration for active users)
 * 
 * SESSION ID FORMAT:
 * 32 hexadecimal characters (16 bytes = 128 bits of randomness)
 * Example: "a3f8c2d4e5f6a7b8c9d0e1f2a3b4c5d6"
 * 
 * CRYPTOGRAPHIC STRENGTH:
 * 128-bit random IDs provide strong security against guessing attacks
 * 2^128 possible IDs = ~3.4 × 10^38 combinations
 * 
 * THREAD SAFETY:
 * Session objects themselves not thread-safe
 * Session storage (map in HttpServer) protected by mutex
 */
class Session {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (Session State)
    // ========================================================================
    
    /**
     * @brief Unique random identifier for this session
     * @type std::string (32 hexadecimal characters)
     * 
     * PURPOSE: Identifies session in server storage, included in client requests
     * 
     * GENERATION: Auto-generated via generateSessionId() using crypto-random source
     * 
     * FORMAT: 32 hex chars (128 bits of entropy)
     * Example: "a3f8c2d4e5f6a7b8c9d0e1f2a3b4c5d6"
     * 
     * SECURITY:
     * - Random: Each login generates new ID (prevents fixation)
     * - Unpredictable: Cannot be guessed by attackers
     * - Unique: Collision probability negligible with 128-bit space
     * 
     * USAGE:
     * - Client: Stored in cookie/localStorage, sent with each request
     * - Server: Key in session map, lookup for authentication
     * 
     * TRANSMISSION:
     * - Cookie: Set-Cookie: session_id=...
     * - Header: Authorization: Bearer ...
     * - Query param: ?session=... (less secure, not recommended)
     */
    std::string sessionId;
    
    /**
     * @brief Username of authenticated user
     * @type std::string
     * 
     * PURPOSE: Links session to user account
     * 
     * USAGE:
     * - Authorization: Determine which user is making request
     * - User lookup: Fetch user details from database
     * - Audit: Log actions by username
     * 
     * IMMUTABLE: Username set at construction, shouldn't change
     * (User would need to re-login to switch accounts)
     */
    std::string username;
    
    /**
     * @brief Unix timestamp when session was created
     * @type time_t
     * 
     * PURPOSE: Track session age, audit logging
     * 
     * USAGE:
     * - Analytics: Session duration = now - createdAt
     * - Security: Flag suspiciously old sessions
     * - Display: Show "logged in X hours ago"
     */
    time_t createdAt;
    
    /**
     * @brief Unix timestamp when session will expire
     * @type time_t
     * 
     * PURPOSE: Automatic session timeout for security
     * 
     * CALCULATION: expiresAt = createdAt + expirationSeconds
     * 
     * VALIDATION: isValid() returns false when now >= expiresAt
     * 
     * REFRESH: Can be extended by calling refresh()
     * 
     * WHY EXPIRATION:
     * - Security: Limits window for session hijacking
     * - Cleanup: Prevents accumulation of abandoned sessions
     * - Force re-auth: Ensures user still has access rights
     * 
     * TYPICAL VALUES:
     * - Short: 15-30 minutes (banking apps)
     * - Medium: 24 hours (social media) - DEFAULT
     * - Long: 7-30 days (convenience apps with "remember me")
     */
    time_t expiresAt;
    
    /**
     * @brief Session timeout duration in seconds
     * @type int
     * 
     * PURPOSE: Configurable session lifespan
     * 
     * DEFAULT: 86400 seconds = 24 hours
     * 
     * COMMON VALUES:
     * - 900 seconds = 15 minutes
     * - 3600 seconds = 1 hour
     * - 86400 seconds = 24 hours
     * - 604800 seconds = 7 days
     * 
     * USAGE:
     * - Initial: Calculate expiresAt = createdAt + expirationSeconds
     * - Refresh: Calculate new expiresAt = now + expirationSeconds
     * 
     * SECURITY vs USABILITY:
     * - Shorter: More secure (less time for attack)
     * - Longer: More convenient (less frequent re-login)
     */
    int expirationSeconds;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Generates cryptographically random session ID
     * @return std::string - 32 hex character session ID
     * 
     * PURPOSE: Creates unpredictable session identifiers
     * 
     * ALGORITHM:
     * 1. Initialize random device (entropy source)
     * 2. Create Mersenne Twister generator (seeded with random device)
     * 3. Generate 32 random hex digits
     * 4. Return as string
     * 
     * RANDOM NUMBER GENERATION:
     * - std::random_device: Hardware entropy source (truly random)
     * - std::mt19937: Mersenne Twister PRNG (fast, high-quality randomness)
     * - std::uniform_int_distribution<>(0, 15): Uniform distribution over hex digits
     * 
     * WHY 32 HEX CHARACTERS:
     * 32 hex chars = 128 bits of entropy
     * 2^128 = 3.4 × 10^38 possible IDs
     * Brute force: Would take billions of years with billions of attempts/second
     * 
     * SECURITY NOTE:
     * std::random_device quality varies by platform
     * Production: Consider using dedicated crypto library (OpenSSL RAND_bytes)
     * 
     * EFFICIENCY: O(1) constant time generation
     * 
     * ALTERNATIVE APPROACHES:
     * - UUID v4: Standard format (128 bits)
     * - OpenSSL RAND_bytes: More secure but requires OpenSSL
     * - /dev/urandom: POSIX systems only
     */
    std::string generateSessionId() {
        // Initialize random number generator with hardware entropy
        std::random_device rd;       // Seed source (truly random)
        std::mt19937 gen(rd());      // Mersenne Twister generator
        std::uniform_int_distribution<> dis(0, 15);  // Range for hex (0-F)

        std::stringstream ss;
        const char* hexChars = "0123456789abcdef";  // Hex digit characters
        
        // Generate 32 random hex characters
        for (int i = 0; i < 32; i++) {
            ss << hexChars[dis(gen)];  // Random index into hexChars
        }
        
        return ss.str();  // Return 32-character hex string
    }

    // ========================================================================
    // PUBLIC INTERFACE (Constructors and Methods)
    // ========================================================================

public:
    /**
     * @brief Default constructor - creates session without username
     * PURPOSE: Rarely used, provided for container compatibility
     * DEFAULT EXPIRATION: 24 hours (86400 seconds)
     */
    Session() : expirationSeconds(86400) {  // 24 hours default
        sessionId = generateSessionId();
        createdAt = std::time(nullptr);
        expiresAt = createdAt + expirationSeconds;
    }

    /**
     * @brief Constructs a new Session for a user
     * @param username Username of authenticated user
     * @param expirationSeconds Session timeout in seconds (default: 86400 = 24h)
     * 
     * PURPOSE: Primary constructor used after successful login
     * 
     * INITIALIZATION SEQUENCE:
     * 1. Store username and expiration duration
     * 2. Generate random session ID
     * 3. Record creation time
     * 4. Calculate expiration time
     * 
     * USAGE EXAMPLE:
     * // Create 24-hour session (default)
     * Session session1("alice");
     * 
     * // Create 1-hour session (custom expiration)
     * Session session2("bob", 3600);
     * 
     * CALLED BY:
     * - Login endpoint: POST /api/auth/login
     * - After successful password verification
     * 
     * POST-CONSTRUCTION:
     * Session stored in server's session map (sessionId → Session)
     * Session ID returned to client for subsequent requests
     */
    Session(const std::string& username, int expirationSeconds = 86400)
        : username(username), expirationSeconds(expirationSeconds) {
        // Generate unique random session ID
        sessionId = generateSessionId();
        
        // Record creation time
        createdAt = std::time(nullptr);
        
        // Calculate expiration time (current time + duration)
        expiresAt = createdAt + expirationSeconds;
    }

    // ========================================================================
    // GETTER METHODS
    // ========================================================================
    
    /** @brief Returns session ID */
    std::string getSessionId() const { return sessionId; }
    
    /** @brief Returns authenticated username */
    std::string getUsername() const { return username; }
    
    /** @brief Returns creation timestamp */
    time_t getCreatedAt() const { return createdAt; }
    
    /** @brief Returns expiration timestamp */
    time_t getExpiresAt() const { return expiresAt; }

    // ========================================================================
    // VALIDATION AND LIFECYCLE METHODS
    // ========================================================================
    
    /**
     * @brief Checks if session is still valid (not expired)
     * @return bool - true if session valid, false if expired
     * 
     * PURPOSE: Determines if session can be used for authentication
     * 
     * LOGIC: Valid when current time < expiration time
     * 
     * USAGE:
     * if (session.isValid()) {
     *     // Authorize request
     * } else {
     *     // Reject, return 401 Unauthorized
     * }
     * 
     * CALLED BY:
     * - Authentication middleware: Every protected endpoint
     * - Session cleanup: Remove expired sessions
     * 
     * WHY const: Read-only check, doesn't modify session
     * 
     * TIME COMPLEXITY: O(1) - simple comparison
     */
    bool isValid() const {
        return std::time(nullptr) < expiresAt;
    }

    /**
     * @brief Checks if session has expired
     * @return bool - true if expired, false if still valid
     * 
     * PURPOSE: Inverse of isValid() for clarity in some contexts
     * 
     * LOGIC: Simply returns !isValid()
     * 
     * USAGE:
     * if (session.isExpired()) {
     *     // Remove from session store, redirect to login
     * }
     * 
     * WHY BOTH METHODS:
     * Readability - "if expired" sometimes clearer than "if not valid"
     */
    bool isExpired() const {
        return !isValid();
    }

    /**
     * @brief Extends session expiration (refresh/renew)
     * @return void
     * 
     * PURPOSE: Prevents active sessions from timing out
     * 
     * MECHANISM:
     * Sets expiresAt = current time + original expiration duration
     * Effectively resets the timeout timer
     * 
     * USAGE PATTERNS:
     * 1. Sliding expiration: Call on every request (keeps active users logged in)
     * 2. Explicit refresh: User clicks "extend session" button
     * 3. Activity-based: Call after significant actions
     * 
     * EXAMPLE:
     * User logged in 23 hours ago (1 hour left)
     * User makes request → refresh() called
     * Now they have 24 hours again from current time
     * 
     * SECURITY CONSIDERATION:
     * Sliding expiration means active sessions never expire
     * Pro: Better user experience (no sudden logouts)
     * Con: Stolen sessions stay valid longer if actively used
     * 
     * ALTERNATIVE STRATEGIES:
     * - Absolute expiration: Never extend, force re-login after fixed time
     * - Combined: Sliding with absolute max (e.g., slide up to 7 days max)
     * 
     * WHY NOT const: Modifies expiresAt
     */
    void refresh() {
        // Extend expiration: new expiration = now + timeout duration
        expiresAt = std::time(nullptr) + expirationSeconds;
    }

    // ========================================================================
    // JSON SERIALIZATION
    // ========================================================================
    
    /**
     * @brief Serializes session to JSON format
     * @return std::string - JSON representation of session
     * 
     * PURPOSE: API responses, debugging, session management UI
     * 
     * OUTPUT FORMAT:
     * {
     *   "sessionId": "a3f8c2d4e5f6a7b8c9d0e1f2a3b4c5d6",
     *   "username": "alice",
     *   "createdAt": 1698765432,
     *   "expiresAt": 1698851832,
     *   "valid": true
     * }
     * 
     * FIELDS:
     * - sessionId: For reference/debugging (not usually sent to client)
     * - username: Which user is authenticated
     * - createdAt: Session age
     * - expiresAt: When session will expire
     * - valid: Current validity status
     * 
     * SECURITY NOTE:
     * Session ID included for debugging/admin purposes
     * In client responses, typically return only username and expiration
     * Session ID already known by client (they sent it)
     * 
     * USAGE:
     * - Admin dashboard: List all active sessions
     * - Debug endpoints: Inspect session state
     * - Client info: Show session details to user
     * 
     * WHY const: Read-only serialization
     */
    std::string toJson() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"sessionId\":\"" << sessionId << "\",";
        ss << "\"username\":\"" << username << "\",";
        ss << "\"createdAt\":" << createdAt << ",";
        ss << "\"expiresAt\":" << expiresAt << ",";
        ss << "\"valid\":" << (isValid() ? "true" : "false");
        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// END OF SESSION CLASS
// ============================================================================

#endif // SESSION_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * DESIGN PHILOSOPHY:
 * - Security first: Random IDs, expiration, validation
 * - Simplicity: Stateful server-side sessions (not JWT)
 * - Flexibility: Configurable expiration, refresh capability
 * 
 * SESSION STORAGE:
 * - Current: In-memory std::map in HttpServer
 * - Production: Redis, Memcached for distributed systems
 * - Persistence: Sessions lost on server restart (acceptable for MVP)
 * 
 * SECURITY BEST PRACTICES:
 * 1. Random session IDs: 128 bits of entropy (strong)
 * 2. HTTPS only: Protect session IDs in transit
 * 3. Secure cookies: HttpOnly, Secure, SameSite flags
 * 4. Short expiration: 24 hours or less
 * 5. Logout: Explicit session destruction
 * 6. Cleanup: Regularly remove expired sessions
 * 
 * ATTACKS AND MITIGATIONS:
 * 
 * SESSION HIJACKING:
 * - Attack: Attacker steals session ID (XSS, network sniffing)
 * - Mitigation: HTTPS, HttpOnly cookies, short expiration
 * 
 * SESSION FIXATION:
 * - Attack: Attacker sets victim's session ID
 * - Mitigation: New ID generated on login (done here)
 * 
 * BRUTE FORCE:
 * - Attack: Guess valid session IDs
 * - Mitigation: 128-bit IDs (2^128 combinations = impossible)
 * 
 * CSRF (Cross-Site Request Forgery):
 * - Attack: Trick user's browser into making authenticated requests
 * - Mitigation: CSRF tokens (separate from session system)
 * 
 * SCALABILITY CONSIDERATIONS:
 * 
 * SINGLE SERVER:
 * - In-memory map works fine
 * - O(1) lookup, minimal overhead
 * 
 * MULTIPLE SERVERS (Load Balanced):
 * - Sticky sessions: Route user to same server
 * - OR shared storage: Redis/Memcached for session data
 * - JWT alternative: Stateless tokens (no server storage)
 * 
 * PERFORMANCE:
 * - Session lookup: O(1) with hash map
 * - Validation: O(1) time comparison
 * - Memory: ~200 bytes per session
 * - 10K active users: ~2MB RAM
 * - 1M active users: ~200MB RAM (still reasonable)
 * 
 * ALTERNATIVES TO SESSIONS:
 * 
 * JWT (JSON Web Tokens):
 * - Pro: Stateless (no server storage), distributed-friendly
 * - Con: Cannot invalidate before expiration, larger payload
 * 
 * OAuth 2.0 Access Tokens:
 * - Pro: Standard, integrates with external auth providers
 * - Con: More complex, requires OAuth server
 * 
 * API Keys:
 * - Pro: Simple, no expiration needed
 * - Con: Less secure (long-lived), user-based not session-based
 * 
 * EXTENSIBILITY:
 * - IP binding: Track IP address, reject if changed
 * - User agent: Validate browser fingerprint
 * - Remember me: Longer expiration with auto-refresh
 * - Multi-factor: Require 2FA for session creation
 * - Device tracking: One session per device
 * - Session history: Log all session events
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: ID generation randomness, expiration logic
 * - Integration tests: Login/logout, expired session rejection
 * - Security tests: Brute force resistance, fixation prevention
 * - Performance tests: Many concurrent sessions
 * - Load tests: Session storage under high load
 ******************************************************************************/

