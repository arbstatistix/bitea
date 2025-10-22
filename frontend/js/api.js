/*******************************************************************************
 * API.JS - Frontend API Client Layer
 * 
 * PURPOSE:
 * This JavaScript module provides a clean interface for communicating with the
 * Bitea backend API. Handles HTTP requests, authentication, session management,
 * and error handling for all frontend-backend communication.
 * 
 * ARCHITECTURE:
 * - API object: Organized collection of endpoint methods
 * - Session management: localStorage for session persistence
 * - Error handling: Consistent error propagation to UI
 * - Configuration: Flexible API base URL via config.js
 * 
 * INTEGRATION:
 * - app.js: Calls API methods for all user actions
 * - Backend (main.cpp): RESTful API endpoints consumed here
 * - localStorage: Browser storage for session/username
 * - config.js: Optional configuration overrides
 * 
 * AUTHENTICATION FLOW:
 * 1. Login → API.login() → returns sessionId
 * 2. setSession(sessionId, username) → stores in localStorage
 * 3. Future requests include session via Authorization header
 * 4. Logout → API.logout() → clearSession() → removes from localStorage
 * 
 * HTTP METHODS USED:
 * - GET: Retrieve data (posts, users, blockchain)
 * - POST: Create/modify data (register, login, create post, like, etc.)
 * 
 * ERROR HANDLING:
 * - Network errors: fetch() exceptions caught and rethrown
 * - HTTP errors: Non-2xx status codes throw with error message
 * - Server errors: error field extracted from JSON response
 * 
 * BROWSER APIs USED:
 * - fetch(): Modern HTTP client (replaces XMLHttpRequest)
 * - localStorage: Persistent browser storage
 * - JSON: Serialization/deserialization
 * - Optional chaining (?.): Safe property access
 * 
 * REFERENCES:
 * - Fetch API: https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API
 * - localStorage: https://developer.mozilla.org/en-US/docs/Web/API/Window/localStorage
 * - REST API Client Design: Clean separation of concerns
 ******************************************************************************/

// ============================================================================
// CONFIGURATION (API Base URL and Storage Keys)
// ============================================================================

/**
 * API_BASE: Backend server URL
 * 
 * SOURCES (priority order):
 * 1. window.BITEA_CONFIG.API_BASE (from config.js if present)
 * 2. Default: 'http://localhost:3000' (development)
 * 
 * OPTIONAL CHAINING (?.):
 * window.BITEA_CONFIG?.API_BASE
 * - Returns undefined if BITEA_CONFIG doesn't exist
 * - Prevents "Cannot read property of undefined" errors
 * 
 * ENVIRONMENTS:
 * - Development: http://localhost:3000
 * - Production: https://api.bitea.com
 * - Custom: Override via config.js
 * 
 * WHY CONFIGURABLE:
 * Different environments (dev, staging, prod) need different URLs
 * Avoids hardcoding, enables easy deployment
 */
const API_BASE = window.BITEA_CONFIG?.API_BASE || 'http://localhost:3000';

/**
 * SESSION_KEY: localStorage key for session ID
 * USERNAME_KEY: localStorage key for username
 * 
 * PURPOSE: Keys for storing authentication data in browser
 * 
 * DEFAULT VALUES:
 * - SESSION_KEY: 'bitea_session'
 * - USERNAME_KEY: 'bitea_username'
 * 
 * STORAGE:
 * localStorage.setItem(SESSION_KEY, sessionId)
 * 
 * WHY SEPARATE KEYS:
 * Session ID and username stored independently
 * Allows checking username without parsing session data
 * 
 * SECURITY:
 * localStorage accessible to JavaScript (vulnerable to XSS)
 * Session tokens should be HttpOnly cookies in production
 * Current approach: Simple, suitable for educational demo
 */
const SESSION_KEY = window.BITEA_CONFIG?.STORAGE_KEYS.SESSION || 'bitea_session';
const USERNAME_KEY = window.BITEA_CONFIG?.STORAGE_KEYS.USERNAME || 'bitea_username';

// ============================================================================
// SESSION MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * @function getSession
 * @returns {string|null} Session ID or null if not logged in
 * 
 * PURPOSE: Retrieve stored session ID from browser
 * 
 * USAGE:
 * const sessionId = getSession();
 * if (sessionId) { // User is logged in }
 * 
 * STORAGE: Browser localStorage (persists across page reloads)
 */
function getSession() {
    return localStorage.getItem(SESSION_KEY);
}

/**
 * @function setSession
 * @param {string} sessionId - Session token from server
 * @param {string} username - Authenticated username
 * 
 * PURPOSE: Store authentication data after successful login
 * 
 * CALLED BY: login() function after API.login() succeeds
 * 
 * STORAGE:
 * - sessionId → localStorage[SESSION_KEY]
 * - username → localStorage[USERNAME_KEY]
 * 
 * PERSISTENCE:
 * Data persists across:
 * - Page reloads
 * - Browser restarts
 * - Tab closures
 * 
 * UNTIL: clearSession() called or user clears browser data
 */
function setSession(sessionId, username) {
    localStorage.setItem(SESSION_KEY, sessionId);
    localStorage.setItem(USERNAME_KEY, username);
}

/**
 * @function clearSession
 * 
 * PURPOSE: Remove authentication data (logout)
 * 
 * CALLED BY: logout() function
 * 
 * EFFECT:
 * - Removes session ID from localStorage
 * - Removes username from localStorage
 * - User appears logged out on next page load
 */
function clearSession() {
    localStorage.removeItem(SESSION_KEY);
    localStorage.removeItem(USERNAME_KEY);
}

/**
 * @function getUsername
 * @returns {string|null} Stored username or null
 * 
 * PURPOSE: Retrieve current user's username
 * 
 * USAGE: Display username, check if logged in, identify user
 */
function getUsername() {
    return localStorage.getItem(USERNAME_KEY);
}

// ============================================================================
// HTTP CLIENT (Fetch API Wrapper)
// ============================================================================

/**
 * @async
 * @function apiCall
 * @param {string} endpoint - API path (e.g., '/api/posts')
 * @param {string} method - HTTP method ('GET', 'POST', etc.) - default: 'GET'
 * @param {Object|null} body - Request payload object - default: null
 * @returns {Promise<Object>} Parsed JSON response from server
 * @throws {Error} On network error or HTTP error status
 * 
 * PURPOSE: Centralized HTTP request handler for all API calls
 * 
 * WORKFLOW:
 * 1. Build request options (method, headers, body)
 * 2. Add Authorization header if session exists
 * 3. Make HTTP request via fetch()
 * 4. Parse JSON response
 * 5. Check for errors (non-2xx status)
 * 6. Return data or throw error
 * 
 * HEADERS:
 * - Content-Type: application/json (all requests send/receive JSON)
 * - Authorization: Bearer <sessionId> (if logged in)
 * 
 * ERROR HANDLING:
 * - Network errors: fetch() rejects → caught and rethrown
 * - HTTP errors: response.ok = false → extract error message, throw
 * - Server errors: JSON {error: "message"} → thrown with message
 * 
 * ASYNC/AWAIT:
 * Function is async → returns Promise
 * Uses await for fetch (cleaner than .then() chains)
 * 
 * FETCH API:
 * Modern browser API for HTTP requests
 * Replaces XMLHttpRequest (older, more complex)
 * Returns Response object with status, headers, body
 * 
 * JSON SERIALIZATION:
 * - Request: body object → JSON.stringify() → sent to server
 * - Response: JSON text → response.json() → parsed object
 * 
 * CORS:
 * Backend configured with CORS headers
 * Allows cross-origin requests (frontend on different port/domain)
 * 
 * USAGE:
 * const posts = await apiCall('/api/posts', 'GET');
 * const newPost = await apiCall('/api/posts', 'POST', {content: "..."});
 */
async function apiCall(endpoint, method = 'GET', body = null) {
    // Build request options
    const options = {
        method,
        headers: {
            'Content-Type': 'application/json'  // Always JSON for API
        }
    };

    // Add authentication if session exists
    const sessionId = getSession();
    if (sessionId) {
        options.headers['Authorization'] = `Bearer ${sessionId}`;
    }

    // Add request body if provided
    if (body) {
        options.body = JSON.stringify(body);  // Convert object to JSON string
    }

    try {
        // Make HTTP request
        const response = await fetch(`${API_BASE}${endpoint}`, options);
        
        // Parse JSON response
        const data = await response.json();
        
        // Check for HTTP errors (4xx, 5xx)
        if (!response.ok) {
            // Extract error message from response or use default
            throw new Error(data.error || 'Request failed');
        }
        
        // Success - return parsed data
        return data;
        
    } catch (error) {
        // Log error for debugging
        console.error('API Error:', error);
        
        // Rethrow for caller to handle (show to user)
        throw error;
    }
}

// ============================================================================
// API ENDPOINT METHODS (Organized by Category)
// ============================================================================

/**
 * @const API
 * @type {Object}
 * 
 * PURPOSE: Centralized collection of all API endpoint methods
 * 
 * ORGANIZATION:
 * - Auth: register, login, logout
 * - Posts: CRUD operations and social interactions
 * - Users: Profiles and follow functionality
 * - Blockchain: Inspection and mining
 * - Info: System metadata
 * 
 * DESIGN PATTERN:
 * Each method is thin wrapper around apiCall()
 * Provides clean, descriptive interface for app.js
 * 
 * USAGE:
 * const posts = await API.getPosts();
 * await API.likePost(postId);
 * const user = await API.getUser(username);
 * 
 * BENEFITS:
 * - Self-documenting: Method names describe action
 * - Type hints: Parameters show what's needed
 * - Centralized: All endpoints in one place
 * - Easy to mock: For testing without backend
 */
const API = {
    // ========================================================================
    // AUTHENTICATION ENDPOINTS
    // ========================================================================
    
    /**
     * @method register
     * @param {string} username - Desired username (3-20 chars)
     * @param {string} email - Valid email address
     * @param {string} password - Password (min 8 chars, letter+digit)
     * @returns {Promise<Object>} Created user object
     * 
     * ENDPOINT: POST /api/register
     * RESPONSE: {username, email, displayName, followers, following, createdAt}
     */
    register: (username, email, password) => 
        apiCall('/api/register', 'POST', { username, email, password }),
    
    /**
     * @method login
     * @param {string} username - Username
     * @param {string} password - Password
     * @returns {Promise<Object>} {sessionId, user} on success
     * 
     * ENDPOINT: POST /api/login
     * RESPONSE: Session ID and user profile
     */
    login: (username, password) => 
        apiCall('/api/login', 'POST', { username, password }),
    
    /**
     * @method logout
     * @returns {Promise<Object>} Success message
     * 
     * ENDPOINT: POST /api/logout
     * EFFECT: Invalidates current session on server
     */
    logout: () => 
        apiCall('/api/logout', 'POST'),
    
    // ========================================================================
    // POST ENDPOINTS (Social Media Content)
    // ========================================================================
    
    /**
     * @method getPosts
     * @returns {Promise<Array>} Array of all posts
     * 
     * ENDPOINT: GET /api/posts
     * RETURNS: Lightweight post summaries (counts, not full comments)
     */
    getPosts: () => 
        apiCall('/api/posts'),
    
    /**
     * @method getPost
     * @param {string} id - Post ID
     * @returns {Promise<Object>} Single post with full details
     * 
     * ENDPOINT: GET /api/posts/:id
     * RETURNS: Detailed post (includes full comment array)
     */
    getPost: (id) => 
        apiCall(`/api/posts/${id}`),
    
    /**
     * @method createPost
     * @param {string} content - Post content (1-5000 chars)
     * @returns {Promise<Object>} Created post object
     * 
     * ENDPOINT: POST /api/posts
     * AUTH: Required
     * EFFECT: Creates post in DB + blockchain transaction
     */
    createPost: (content) => 
        apiCall('/api/posts', 'POST', { content }),
    
    /**
     * @method likePost
     * @param {string} id - Post ID to like
     * @returns {Promise<Object>} Updated post object
     * 
     * ENDPOINT: POST /api/posts/:id/like
     * AUTH: Required
     * IDEMPOTENT: Liking same post twice has no additional effect
     */
    likePost: (id) => 
        apiCall(`/api/posts/${id}/like`, 'POST'),
    
    /**
     * @method commentPost
     * @param {string} id - Post ID to comment on
     * @param {string} content - Comment text (1-1000 chars)
     * @returns {Promise<Object>} Updated post with new comment
     * 
     * ENDPOINT: POST /api/posts/:id/comment
     * AUTH: Required
     */
    commentPost: (id, content) => 
        apiCall(`/api/posts/${id}/comment`, 'POST', { content }),
    
    // ========================================================================
    // USER ENDPOINTS (Profiles and Social Graph)
    // ========================================================================
    
    /**
     * @method getUser
     * @param {string} username - Username to fetch
     * @returns {Promise<Object>} User profile (public data)
     * 
     * ENDPOINT: GET /api/users/:username
     * RETURNS: Public profile (no email, no private data)
     */
    getUser: (username) => 
        apiCall(`/api/users/${username}`),
    
    /**
     * @method followUser
     * @param {string} username - Username to follow
     * @returns {Promise<Object>} Success message
     * 
     * ENDPOINT: POST /api/users/:username/follow
     * AUTH: Required
     * EFFECT: Bidirectional update (both users' followers/following updated)
     */
    followUser: (username) => 
        apiCall(`/api/users/${username}/follow`, 'POST'),
    
    // ========================================================================
    // BLOCKCHAIN ENDPOINTS (Inspection and Mining)
    // ========================================================================
    
    /**
     * @method getBlockchain
     * @returns {Promise<Object>} {blocks: Array<Block>}
     * 
     * ENDPOINT: GET /api/blockchain
     * RETURNS: Entire blockchain with block summaries
     */
    getBlockchain: () => 
        apiCall('/api/blockchain'),
    
    /**
     * @method validateBlockchain
     * @returns {Promise<Object>} {valid: boolean}
     * 
     * ENDPOINT: GET /api/blockchain/validate
     * VALIDATES: Chain integrity, Proof-of-Work, hash links
     */
    validateBlockchain: () => 
        apiCall('/api/blockchain/validate'),
    
    /**
     * @method mineBlock
     * @returns {Promise<Object>} {message, blocks, pending}
     * 
     * ENDPOINT: GET /api/mine
     * EFFECT: Manually triggers block mining
     * DURATION: May take seconds depending on difficulty
     */
    mineBlock: () => 
        apiCall('/api/mine'),
    
    // ========================================================================
    // INFO ENDPOINTS (System Metadata)
    // ========================================================================
    
    /**
     * @method getApiInfo
     * @returns {Promise<Object>} API version, blockchain stats, database stats
     * 
     * ENDPOINT: GET /api
     * USAGE: Dashboard, monitoring, system health
     */
    getApiInfo: () => 
        apiCall('/api')
};

// ============================================================================
// END OF API MODULE
// ============================================================================

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * DESIGN PATTERNS:
 * - Facade Pattern: API object simplifies backend communication
 * - Async/Await: Modern promise handling (cleaner than callbacks)
 * - Error propagation: Throw errors up to UI layer for handling
 * - Configuration: Flexible URL/key configuration
 * 
 * FETCH API BENEFITS:
 * - Promise-based: Works naturally with async/await
 * - Standard: Built into modern browsers (IE support via polyfill)
 * - Flexible: Easy to add headers, change methods
 * - Stream-based: Can handle large responses (not used here)
 * 
 * AUTHENTICATION STRATEGY:
 * - Session-based: Server maintains session state
 * - localStorage: Client stores session ID (persists across reloads)
 * - Bearer token: Industry-standard Authorization header format
 * 
 * SECURITY CONSIDERATIONS:
 * - HTTPS: Should use HTTPS in production (encrypts session tokens)
 * - XSS: localStorage vulnerable to XSS (backend escapes content)
 * - CORS: Backend configured to allow cross-origin (development mode)
 * - Tokens: Short-lived sessions reduce exposure window
 * 
 * ERROR HANDLING STRATEGY:
 * - Network errors: Caught in apiCall, rethrown with context
 * - HTTP errors: Checked via response.ok, error message extracted
 * - UI responsibility: Caller displays error to user (alert, message box)
 * 
 * BROWSER COMPATIBILITY:
 * - fetch(): Modern browsers (Chrome 42+, Firefox 39+, Safari 10.1+)
 * - async/await: ES2017 (Chrome 55+, Firefox 52+, Safari 10.1+)
 * - Optional chaining (?.): ES2020 (Chrome 80+, Firefox 74+, Safari 13.1+)
 * - localStorage: All modern browsers
 * 
 * Polyfills available for older browsers if needed
 * 
 * ALTERNATIVE APPROACHES:
 * - Axios: Popular HTTP library (more features, larger bundle)
 * - jQuery.ajax: Legacy approach (not needed with fetch)
 * - XMLHttpRequest: Old standard (more complex than fetch)
 * - GraphQL: Alternative to REST (different paradigm)
 * 
 * API VERSIONING:
 * Current: /api/posts (no version in URL)
 * Future: /api/v1/posts (version in path)
 * Or: Accept: application/vnd.bitea.v1+json (content negotiation)
 * 
 * OPTIMIZATION OPPORTUNITIES:
 * - Request caching: Cache GET requests in memory/localStorage
 * - Request deduplication: Prevent duplicate in-flight requests
 * - Retry logic: Auto-retry failed requests
 * - Timeout handling: Abort slow requests
 * - Progress callbacks: For long operations (file uploads)
 * - Batch requests: Combine multiple calls (GraphQL-style)
 * 
 * TESTING:
 * - Mock API: Replace apiCall for offline testing
 * - Service workers: Intercept requests for offline mode
 * - Unit tests: Test each API method independently
 * - Integration tests: Test with actual backend
 * 
 * MONITORING:
 * - Log all API calls (endpoint, method, duration)
 * - Track error rates (network vs HTTP vs app errors)
 * - Performance metrics (request latency by endpoint)
 * - Analytics: Which endpoints used most frequently
 ******************************************************************************/

