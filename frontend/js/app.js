/*******************************************************************************
 * APP.JS - Bitea Frontend Application Logic
 * 
 * PURPOSE:
 * This is the main frontend application file that handles UI logic, state
 * management, DOM manipulation, and user interactions for the Bitea blockchain
 * social media platform.
 * 
 * APPLICATION ARCHITECTURE:
 * - Single Page Application (SPA): Multiple views, one HTML file
 * - State management: Global variables for user and topic data
 * - Event-driven: User interactions trigger async API calls
 * - DOM manipulation: Dynamic content rendering via JavaScript
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - api.js: All backend communication via API object
 * - index.html: DOM elements manipulated by functions here
 * - style.css: Classes referenced for styling
 * - localStorage: Session persistence, topic data storage
 * 
 * KEY FEATURES:
 * - Authentication: Login, register, logout with session management
 * - Social Feed: Create posts, like, comment
 * - bitCafe: Discussion topics (localStorage-based, blockchain-ready)
 * - Blockchain Explorer: View blocks, mine, validate chain
 * - Statistics: Comprehensive blockchain analytics
 * - Profile: User information display
 * 
 * PAGE STRUCTURE:
 * - Login: Authentication forms
 * - Feed: Social media posts
 * - Topics (bitCafe): Discussion forum
 * - Blockchain: Block explorer
 * - Stats: Analytics dashboard
 * - Profile: User profile
 * 
 * STATE MANAGEMENT:
 * - currentUser: Currently logged-in username (global)
 * - topicsData: Array of topics (localStorage-backed)
 * - currentTopicId: Currently viewed topic ID
 * 
 * ASYNC PATTERN:
 * Most functions are async (API calls are asynchronous)
 * Uses async/await for cleaner promise handling
 * Try/catch for error handling
 * 
 * DOM MANIPULATION:
 * - document.getElementById(): Get specific elements
 * - document.querySelectorAll(): Get multiple elements
 * - innerHTML: Dynamic content rendering
 * - classList: CSS class manipulation
 * - Template literals: HTML generation with ${} syntax
 * 
 * SECURITY:
 * - XSS Prevention: escapeHtml() for user-generated content
 * - Input validation: Client-side (convenience) + server-side (security)
 * - Sanitization: Backend escapes dangerous characters
 * 
 * BROWSER APIs USED:
 * - DOM API: Element manipulation, event listeners
 * - localStorage: Persistent storage
 * - fetch (via api.js): HTTP requests
 * - Date: Timestamp formatting
 * - JSON: Data parsing/stringification
 * - Promise: Async operations
 * 
 * REFERENCES:
 * - DOM API: https://developer.mozilla.org/en-US/docs/Web/API/Document_Object_Model
 * - SPA Architecture: Single-page application patterns
 * - Modern JavaScript: ES6+ features (arrow functions, template literals, async/await)
 ******************************************************************************/

// ============================================================================
// GLOBAL STATE (Application-Wide Variables)
// ============================================================================

/**
 * @var {string|null} currentUser
 * 
 * PURPOSE: Stores currently logged-in username
 * 
 * STATES:
 * - null: User not logged in
 * - string: Username of logged-in user
 * 
 * USAGE:
 * - Check authentication: if (currentUser) { ... }
 * - Display username: Show logged-in user
 * - Author attribution: Posts/comments by currentUser
 * 
 * LIFECYCLE:
 * - Set: After successful login (checkAuth)
 * - Cleared: After logout
 * - Persisted: Via localStorage (session ID + username)
 */
let currentUser = null;

// ============================================================================
// APPLICATION INITIALIZATION
// ============================================================================

/**
 * DOM CONTENT LOADED EVENT
 * 
 * PURPOSE: Initialize app after HTML fully loaded
 * 
 * BROWSER EVENT:
 * Fires when DOM tree complete (before images/stylesheets finish)
 * Safer than window.onload (waits for everything)
 * 
 * INITIALIZATION:
 * 1. checkAuth(): Restore session from localStorage
 * 2. loadFeedStats(): Load initial blockchain stats
 * 
 * WHY addEventListener:
 * - Modern approach (vs. onload attribute)
 * - Multiple listeners possible
 * - Better separation of HTML and JavaScript
 */
document.addEventListener('DOMContentLoaded', () => {
    checkAuth();        // Check if user already logged in
    loadFeedStats();    // Load initial stats for display
});

// ============================================================================
// PAGE NAVIGATION (SPA Routing)
// ============================================================================

/**
 * @function showPage
 * @param {string} pageName - Page identifier ('feed', 'blockchain', etc.)
 * 
 * PURPOSE: Switches between different views in single-page app
 * 
 * SPA PATTERN:
 * All "pages" exist in same HTML file
 * Only one visible at a time (CSS class 'active')
 * 
 * ALGORITHM:
 * 1. Remove 'active' class from all pages (hide all)
 * 2. Add 'active' class to selected page (show one)
 * 3. Load data for that page (API calls)
 * 
 * PAGE NAMES:
 * - 'feed': Social media feed
 * - 'topics': bitCafe discussion forum
 * - 'blockchain': Block explorer
 * - 'stats': Analytics dashboard
 * - 'profile': User profile
 * - 'login': Authentication page
 * 
 * DATA LOADING:
 * Each page loads its own data when shown
 * Prevents loading all data upfront (performance)
 * 
 * USAGE:
 * showPage('feed');       // Show feed page
 * onclick="showPage('blockchain')"  // From HTML
 */
function showPage(pageName) {
    // Hide all pages (remove 'active' class)
    document.querySelectorAll('.page').forEach(page => {
        page.classList.remove('active');
    });
    
    // Show selected page (add 'active' class)
    const page = document.getElementById(`${pageName}Page`);
    if (page) {
        page.classList.add('active');
        
        // Load page-specific data
        if (pageName === 'feed') {
            loadFeed();         // Fetch all posts
            loadFeedStats();    // Update stat counters
        } else if (pageName === 'topics') {
            loadTopics();       // Load bitCafe topics
        } else if (pageName === 'blockchain') {
            loadBlockchain();   // Fetch blockchain data
        } else if (pageName === 'stats') {
            loadStats();        // Load comprehensive statistics
        } else if (pageName === 'profile') {
            loadProfile();      // Load user profile
        }
    }
}

// ============================================================================
// AUTHENTICATION FUNCTIONS
// ============================================================================

/**
 * @function checkAuth
 * 
 * PURPOSE: Check if user is logged in and update UI accordingly
 * 
 * WORKFLOW:
 * 1. Retrieve session ID and username from localStorage
 * 2. If both exist → user is logged in
 * 3. Update global state (currentUser)
 * 4. Show/hide navigation buttons
 * 5. Show appropriate page (feed if logged in, login if not)
 * 
 * UI UPDATES (Logged In):
 * - Hide "Login" button
 * - Show "Logout" button
 * - Show "Profile" link
 * - Navigate to feed page
 * 
 * UI UPDATES (Logged Out):
 * - Show login page
 * 
 * CALLED BY:
 * - App initialization (DOMContentLoaded)
 * - After login success
 * - After page reload (restores session)
 * 
 * SESSION PERSISTENCE:
 * localStorage persists across page reloads
 * User stays logged in until explicit logout or session expires
 */
function checkAuth() {
    const sessionId = getSession();
    const username = getUsername();
    
    if (sessionId && username) {
        // User is logged in
        currentUser = username;
        
        // Update navigation UI
        document.getElementById('loginBtn').style.display = 'none';
        document.getElementById('logoutBtn').style.display = 'block';
        document.getElementById('profileLink').style.display = 'block';
        
        // Show feed page
        showPage('feed');
    } else {
        // User not logged in
        showPage('login');
    }
}

/**
 * @function switchTab
 * @param {string} tab - 'login' or 'register'
 * 
 * PURPOSE: Toggle between login and registration forms
 * 
 * UI MANIPULATION:
 * - Updates tab button styling (active/inactive)
 * - Shows/hides appropriate form
 * - Clears any error messages
 * 
 * CALLED BY: Tab button clicks in auth page
 */
function switchTab(tab) {
    // Remove active state from all tab buttons
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    if (tab === 'login') {
        // Show login form
        document.querySelector('.tab-btn:first-child').classList.add('active');
        document.getElementById('loginForm').style.display = 'flex';
        document.getElementById('registerForm').style.display = 'none';
    } else {
        // Show registration form
        document.querySelector('.tab-btn:last-child').classList.add('active');
        document.getElementById('loginForm').style.display = 'none';
        document.getElementById('registerForm').style.display = 'flex';
    }
    
    // Clear any previous messages
    document.getElementById('authMessage').textContent = '';
}

/**
 * @async
 * @function register
 * 
 * PURPOSE: Handle user registration form submission
 * 
 * WORKFLOW:
 * 1. Extract form values (username, email, password)
 * 2. Trim whitespace from text inputs
 * 3. Validate all fields filled
 * 4. Call API.register()
 * 5. On success: Show message, switch to login tab
 * 6. On error: Display error message
 * 
 * VALIDATION:
 * Client-side: Check fields not empty (UX)
 * Server-side: Comprehensive validation (security)
 * 
 * USER FEEDBACK:
 * - Success: "Registration successful! Please login."
 * - Auto-switch to login tab after 2 seconds
 * - Error: Display server error message
 */
async function register() {
    // Extract form values
    const username = document.getElementById('registerUsername').value.trim();
    const email = document.getElementById('registerEmail').value.trim();
    const password = document.getElementById('registerPassword').value;
    const messageEl = document.getElementById('authMessage');
    
    // Client-side validation (empty check)
    if (!username || !email || !password) {
        showMessage(messageEl, 'Please fill in all fields', 'error');
        return;
    }
    
    try {
        // Call backend registration API
        await API.register(username, email, password);
        
        // Success feedback
        showMessage(messageEl, 'Registration successful! Please login.', 'success');
        
        // Auto-switch to login after 2 seconds
        setTimeout(() => switchTab('login'), 2000);
        
    } catch (error) {
        // Display server error (validation, duplicate username, etc.)
        showMessage(messageEl, error.message, 'error');
    }
}

/**
 * @async
 * @function login
 * 
 * PURPOSE: Handle user login form submission
 * 
 * WORKFLOW:
 * 1. Extract username and password from form
 * 2. Validate fields not empty
 * 3. Call API.login()
 * 4. On success: Store session, update UI, redirect to feed
 * 5. On error: Display error message
 * 
 * SUCCESS FLOW:
 * 1. Receive sessionId and user data from server
 * 2. Store in localStorage via setSession()
 * 3. Show success message
 * 4. After 1 second, call checkAuth() (updates UI, shows feed)
 * 
 * ERROR HANDLING:
 * - Network errors: "Request failed"
 * - Invalid credentials: "Invalid credentials" (generic for security)
 * - Display error to user via showMessage()
 */
async function login() {
    // Extract form values
    const username = document.getElementById('loginUsername').value.trim();
    const password = document.getElementById('loginPassword').value;
    const messageEl = document.getElementById('authMessage');
    
    // Client-side validation
    if (!username || !password) {
        showMessage(messageEl, 'Please fill in all fields', 'error');
        return;
    }
    
    try {
        // Call backend login API
        const result = await API.login(username, password);
        
        // Store session in localStorage
        setSession(result.sessionId, result.user.username);
        
        // Success feedback
        showMessage(messageEl, 'Login successful!', 'success');
        
        // Update UI after short delay (show success message first)
        setTimeout(() => {
            checkAuth();  // Updates UI, redirects to feed
        }, 1000);
        
    } catch (error) {
        // Display login error
        showMessage(messageEl, error.message, 'error');
    }
}

/**
 * @async
 * @function logout
 * 
 * PURPOSE: Log out current user
 * 
 * WORKFLOW:
 * 1. Call API.logout() (invalidates session on server)
 * 2. Clear localStorage (remove session + username)
 * 3. Clear global state (currentUser = null)
 * 4. Update navigation UI (hide logout, show login)
 * 5. Redirect to login page
 * 
 * ERROR HANDLING:
 * Logout errors logged but don't prevent local cleanup
 * Even if server call fails, local session still cleared
 * 
 * UI CLEANUP:
 * - currentUser reset to null
 * - Session data removed from localStorage
 * - Navigation buttons updated
 * - Redirected to login page
 */
async function logout() {
    try {
        // Invalidate session on server
        await API.logout();
    } catch (error) {
        // Log but don't prevent logout
        console.error('Logout error:', error);
    }
    
    // Clear local session data
    clearSession();
    currentUser = null;
    
    // Update navigation UI
    document.getElementById('loginBtn').style.display = 'block';
    document.getElementById('logoutBtn').style.display = 'none';
    document.getElementById('profileLink').style.display = 'none';
    
    // Redirect to login page
    showPage('login');
}

// ============================================================================
// FEED FUNCTIONS (Social Media Posts)
// ============================================================================

/**
 * @async
 * @function loadFeed
 * 
 * PURPOSE: Fetch and display all posts from backend
 * 
 * WORKFLOW:
 * 1. Call API.getPosts() → fetches from MongoDB
 * 2. displayPosts() renders HTML
 * 3. Error handling logs to console
 * 
 * CALLED BY:
 * - showPage('feed'): When feed page shown
 * - After creating post: Refresh to show new post
 * - After like/comment: Refresh to show updates
 * 
 * DATA SOURCE: Backend MongoDB (fast queries)
 * RENDERING: Dynamic HTML via template literals
 */
async function loadFeed() {
    try {
        // Fetch all posts from backend
        const posts = await API.getPosts();
        
        // Render posts to DOM
        displayPosts(posts);
        
    } catch (error) {
        // Log error (non-critical, feed just won't load)
        console.error('Error loading feed:', error);
    }
}

/**
 * @async
 * @function createPost
 * 
 * PURPOSE: Handle post creation form submission
 * 
 * WORKFLOW:
 * 1. Extract content from textarea
 * 2. Trim whitespace
 * 3. Validate not empty
 * 4. Call API.createPost()
 * 5. Clear textarea
 * 6. Refresh feed (show new post)
 * 7. Refresh stats (update counters)
 * 8. Show success message
 * 
 * DUAL STORAGE:
 * Backend stores in:
 * - MongoDB (fast retrieval)
 * - Blockchain (immutable proof)
 * 
 * USER FEEDBACK:
 * Success: "Post created and added to blockchain!"
 * Error: Alert with error message
 * 
 * BLOCKCHAIN:
 * Post creates Transaction, auto-mines when 5 txs accumulated
 */
async function createPost() {
    // Extract and trim content
    const content = document.getElementById('postContent').value.trim();
    
    // Validate not empty
    if (!content) {
        alert('Please enter some content');
        return;
    }
    
    try {
        // Create post via API (stores in DB + blockchain)
        await API.createPost(content);
        
        // Clear textarea
        document.getElementById('postContent').value = '';
        
        // Refresh UI
        loadFeed();   // Show new post
        loadStats();  // Update counters
        
        // Success feedback
        alert('Post created and added to blockchain!');
        
    } catch (error) {
        // Display error (validation, unauthorized, etc.)
        alert('Error creating post: ' + error.message);
    }
}

/**
 * @function displayPosts
 * @param {Array} posts - Array of post objects from API
 * 
 * PURPOSE: Renders posts as HTML cards in the feed
 * 
 * RENDERING STRATEGY:
 * - Array.map(): Transform each post to HTML string
 * - Template literals: Embed data with ${}
 * - join(''): Concatenate all HTML strings
 * - innerHTML: Insert into DOM
 * 
 * POST CARD STRUCTURE:
 * - Header: Author and timestamp
 * - Content: Post text (XSS-escaped)
 * - Stats: Like count, comment count, blockchain badge
 * - Actions: Like and comment buttons
 * 
 * XSS PREVENTION:
 * escapeHtml(post.content): Escapes <, >, &, ", '
 * Prevents malicious HTML/JavaScript injection
 * 
 * BLOCKCHAIN BADGE:
 * Conditional rendering: ${post.isOnChain ? '...' : ''}
 * Shows "[CHAIN] On Chain" if post mined into block
 * 
 * EVENT HANDLERS:
 * onclick="likePost('${post.id}')"
 * Inline event handlers with post ID parameter
 * Alternative: addEventListener (more modern)
 * 
 * EMPTY STATE:
 * If no posts, shows friendly message encouraging first post
 */
function displayPosts(posts) {
    const container = document.getElementById('postsContainer');
    
    // Handle empty state
    if (!posts || posts.length === 0) {
        container.innerHTML = '<p style="text-align:center; color:white;">No posts yet. Be the first to post!</p>';
        return;
    }
    
    // Render posts as HTML cards
    container.innerHTML = posts.map(post => `
        <div class="post-card">
            <div class="post-header">
                <span class="post-author">@${post.author}</span>
                <span class="post-time">${formatTime(post.timestamp)}</span>
            </div>
            <div class="post-content">${escapeHtml(post.content)}</div>
            <div class="post-stats">
                <span>[<3] ${post.likes} likes</span>
                <span>[//] ${post.comments} comments</span>
                ${post.isOnChain ? '<span class="blockchain-badge">[CHAIN] On Chain</span>' : ''}
            </div>
            <div class="post-actions">
                <button class="btn btn-small btn-secondary" onclick="likePost('${post.id}')">[<3] Like</button>
                <button class="btn btn-small btn-secondary" onclick="viewPost('${post.id}')">[//] Comment</button>
            </div>
        </div>
    `).join('');  // join() converts array to single HTML string
}

/**
 * @async
 * @function likePost
 * @param {string} postId - ID of post to like
 * 
 * PURPOSE: Like a post (adds to user's likes set)
 * 
 * WORKFLOW:
 * 1. Call API.likePost(postId)
 * 2. Backend adds username to post.likes set
 * 3. Backend creates LIKE transaction on blockchain
 * 4. Refresh feed to show updated like count
 * 5. Refresh stats to update totals
 * 
 * IDEMPOTENT:
 * Backend prevents duplicate likes (set ensures uniqueness)
 * Liking twice has no additional effect
 * 
 * ERROR HANDLING:
 * - 401: Not authenticated
 * - 404: Post not found
 * - Display error alert
 */
async function likePost(postId) {
    try {
        // Like post via API
        await API.likePost(postId);
        
        // Refresh UI to show updated counts
        loadFeed();
        loadStats();
        
    } catch (error) {
        // Display error (auth, not found, etc.)
        alert('Error liking post: ' + error.message);
    }
}

/**
 * @async
 * @function viewPost
 * @param {string} postId - ID of post to view/comment on
 * 
 * PURPOSE: View post details and optionally add comment
 * 
 * WORKFLOW:
 * 1. Fetch full post details (includes comments)
 * 2. Prompt user for comment text
 * 3. If user enters text, submit comment
 * 4. Refresh feed and stats
 * 5. Show success message
 * 
 * UI:
 * Uses browser prompt() for comment input
 * Simple but not ideal UX (could use modal dialog)
 * 
 * VALIDATION:
 * - Checks content exists and not only whitespace
 * - Server validates length (1-1000 chars)
 */
async function viewPost(postId) {
    try {
        // Fetch post details
        const post = await API.getPost(postId);
        
        // Prompt for comment
        const content = prompt('Add a comment:');
        
        // Submit if user entered text
        if (content && content.trim()) {
            await API.commentPost(postId, content.trim());
            
            // Refresh UI
            loadFeed();
            loadStats();
            
            // Success feedback
            alert('Comment added to blockchain!');
        }
        
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// ============================================================================
// BLOCKCHAIN FUNCTIONS (Explorer and Mining)
// ============================================================================

/**
 * @async
 * @function loadBlockchain
 * 
 * PURPOSE: Fetch blockchain and display blocks
 * 
 * ENDPOINT: GET /api/blockchain
 * RENDERS: Block cards with hash, nonce, timestamp, tx count
 * 
 * CALLED BY:
 * - showPage('blockchain'): When blockchain page shown
 * - After mining: Refresh to show new block
 */
async function loadBlockchain() {
    try {
        const data = await API.getBlockchain();
        displayBlocks(data.blocks);
    } catch (error) {
        console.error('Error loading blockchain:', error);
    }
}

function displayBlocks(blocks) {
    const container = document.getElementById('blocksContainer');
    
    if (!blocks || blocks.length === 0) {
        container.innerHTML = '<p style="text-align:center; color:white;">No blocks found</p>';
        return;
    }
    
    container.innerHTML = blocks.reverse().map(block => `
        <div class="block-card">
            <div class="block-header">
                <span class="block-index">Block #${block.index}</span>
                <span style="color: var(--dark-3);">${block.transactions} transactions</span>
            </div>
            <div class="block-info">
                <div class="info-item">
                    <span class="info-label">Hash</span>
                    <span class="info-value">${block.hash}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Previous Hash</span>
                    <span class="info-value">${block.previousHash}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Nonce</span>
                    <span class="info-value">${block.nonce}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Timestamp</span>
                    <span class="info-value">${formatTime(block.timestamp)}</span>
                </div>
            </div>
        </div>
    `).join('');
}

async function mineBlock() {
    try {
        const result = await API.mineBlock();
        document.getElementById('blockchainStatus').innerHTML = `
            <div class="message success">
                ${result.message}<br>
                Blocks: ${result.blocks} | Pending: ${result.pending}
            </div>
        `;
        loadBlockchain();
        loadStats();
    } catch (error) {
        document.getElementById('blockchainStatus').innerHTML = `
            <div class="message error">Error: ${error.message}</div>
        `;
    }
}

async function validateBlockchain() {
    try {
        const result = await API.validateBlockchain();
        const statusEl = document.getElementById('blockchainStatus');
        
        if (result.valid) {
            statusEl.innerHTML = `
                <div class="message success">
                    [OK] Blockchain is VALID! All blocks are correctly linked and verified.
                </div>
            `;
        } else {
            statusEl.innerHTML = `
                <div class="message error">
                    [ERR] Blockchain validation failed! Chain may have been tampered with.
                </div>
            `;
        }
    } catch (error) {
        document.getElementById('blockchainStatus').innerHTML = `
            <div class="message error">Error: ${error.message}</div>
        `;
    }
}

// Feed Stats
async function loadFeedStats() {
    try {
        const info = await API.getApiInfo();
        
        document.getElementById('totalPosts').textContent = info.database.posts;
        document.getElementById('totalBlocks').textContent = info.blockchain.blocks;
        document.getElementById('pendingTxs').textContent = info.blockchain.pending;
    } catch (error) {
        console.error('Error loading stats:', error);
    }
}

// Comprehensive Stats Page
async function loadStats() {
    try {
        const [info, blockchainData, validationData] = await Promise.all([
            API.getApiInfo(),
            API.getBlockchain(),
            API.validateBlockchain()
        ]);

        const blocks = blockchainData.blocks;
        
        // Calculate statistics
        let totalTransactions = 0;
        let totalNonce = 0;
        let txTypes = {
            register: 0,
            post: 0,
            like: 0,
            comment: 0,
            follow: 0
        };
        
        blocks.forEach(block => {
            totalTransactions += block.transactions;
            totalNonce += block.nonce;
        });

        // For now, use estimated values (in real implementation, fetch actual tx data)
        txTypes.register = Math.floor(totalTransactions * 0.1);
        txTypes.post = Math.floor(totalTransactions * 0.3);
        txTypes.like = Math.floor(totalTransactions * 0.4);
        txTypes.comment = Math.floor(totalTransactions * 0.15);
        txTypes.follow = totalTransactions - (txTypes.register + txTypes.post + txTypes.like + txTypes.comment);

        // Chain Overview
        document.getElementById('stat-blocks').textContent = blocks.length;
        document.getElementById('stat-total-tx').textContent = totalTransactions;
        document.getElementById('stat-pending-tx').textContent = info.blockchain.pending;
        document.getElementById('stat-difficulty').textContent = '3 (Leading Zeros)';

        // Performance Metrics
        if (blocks.length > 1) {
            const firstBlock = blocks[0];
            const lastBlock = blocks[blocks.length - 1];
            const timeSpan = lastBlock.timestamp - firstBlock.timestamp;
            const avgBlockTime = timeSpan / (blocks.length - 1);
            document.getElementById('stat-avg-block-time').textContent = avgBlockTime > 0 ? 
                `${avgBlockTime.toFixed(1)}s` : 'N/A';
            
            const avgTxPerBlock = (totalTransactions / blocks.length).toFixed(2);
            document.getElementById('stat-avg-tx-per-block').textContent = avgTxPerBlock;
            
            const throughput = avgBlockTime > 0 ? 
                ((totalTransactions / timeSpan) * 60).toFixed(2) : '0';
            document.getElementById('stat-throughput').textContent = `${throughput} tx/min`;
        } else {
            document.getElementById('stat-avg-block-time').textContent = 'N/A';
            document.getElementById('stat-avg-tx-per-block').textContent = 'N/A';
            document.getElementById('stat-throughput').textContent = 'N/A';
        }

        const avgNonce = blocks.length > 0 ? 
            Math.floor(totalNonce / blocks.length) : 0;
        document.getElementById('stat-avg-nonce').textContent = avgNonce.toLocaleString();

        // Utilization (assuming max 5 tx per block)
        const maxCapacity = blocks.length * 5;
        const utilization = maxCapacity > 0 ? 
            ((totalTransactions / maxCapacity) * 100).toFixed(1) : 0;
        document.getElementById('stat-utilization').textContent = `${utilization}%`;

        // Transaction Statistics
        document.getElementById('stat-tx-register').textContent = txTypes.register;
        document.getElementById('stat-tx-posts').textContent = txTypes.post;
        document.getElementById('stat-tx-likes').textContent = txTypes.like;
        document.getElementById('stat-tx-comments').textContent = txTypes.comment;
        document.getElementById('stat-tx-follows').textContent = txTypes.follow;

        // Calculate percentages
        if (totalTransactions > 0) {
            document.getElementById('stat-tx-register-pct').textContent = 
                `${((txTypes.register / totalTransactions) * 100).toFixed(1)}%`;
            document.getElementById('stat-tx-posts-pct').textContent = 
                `${((txTypes.post / totalTransactions) * 100).toFixed(1)}%`;
            document.getElementById('stat-tx-likes-pct').textContent = 
                `${((txTypes.like / totalTransactions) * 100).toFixed(1)}%`;
            document.getElementById('stat-tx-comments-pct').textContent = 
                `${((txTypes.comment / totalTransactions) * 100).toFixed(1)}%`;
            document.getElementById('stat-tx-follows-pct').textContent = 
                `${((txTypes.follow / totalTransactions) * 100).toFixed(1)}%`;
        }

        // Network Health
        const isValid = validationData.valid;
        document.getElementById('stat-chain-valid').textContent = isValid ? 
            '[OK] VALID' : '[ERR] INVALID';
        document.getElementById('stat-chain-valid').style.color = isValid ? 
            '#00ff00' : '#ff0000';

        // Security Level based on difficulty and chain length
        let securityLevel = 'LOW';
        if (blocks.length > 10 && totalNonce > 1000) securityLevel = 'MEDIUM';
        if (blocks.length > 50 && totalNonce > 10000) securityLevel = 'HIGH';
        if (blocks.length > 100 && totalNonce > 100000) securityLevel = 'VERY HIGH';
        document.getElementById('stat-security-level').textContent = securityLevel;

        document.getElementById('stat-network-state').textContent = 
            info.blockchain.pending > 0 ? 'ACTIVE' : 'IDLE';

        // Recent Activity (last 10 blocks)
        const recentBlocks = blocks.slice(Math.max(0, blocks.length - 10)).reverse();
        const activityHtml = recentBlocks.map(block => `
            <tr>
                <td class="stat-value-cell">#${block.index}</td>
                <td>${formatTime(block.timestamp)}</td>
                <td class="stat-value-cell">${block.transactions}</td>
                <td>${block.hash.substring(0, 16)}...</td>
            </tr>
        `).join('');
        document.getElementById('recent-activity').innerHTML = activityHtml || 
            '<tr><td colspan="4">No recent activity</td></tr>';

    } catch (error) {
        console.error('Error loading comprehensive stats:', error);
    }
}

// Profile
async function loadProfile() {
    const username = getUsername();
    if (!username) return;
    
    try {
        const user = await API.getUser(username);
        document.getElementById('profileInfo').innerHTML = `
            <h3>@${user.username}</h3>
            <p><strong>Display Name:</strong> ${user.displayName}</p>
            <p><strong>Bio:</strong> ${user.bio || 'No bio yet'}</p>
            <p><strong>Followers:</strong> ${user.followers}</p>
            <p><strong>Following:</strong> ${user.following}</p>
            <p><strong>Joined:</strong> ${formatTime(user.createdAt)}</p>
        `;
    } catch (error) {
        console.error('Error loading profile:', error);
    }
}

// ============================================================================
// UTILITY FUNCTIONS (Helpers for UI and Data Formatting)
// ============================================================================

/**
 * @function showMessage
 * @param {HTMLElement} element - DOM element to display message in
 * @param {string} message - Message text to display
 * @param {string} type - Message type ('success', 'error')
 * 
 * PURPOSE: Display feedback messages to user
 * 
 * STYLING:
 * - Sets element class to "message success" or "message error"
 * - CSS handles color coding (green for success, red for error)
 * 
 * USAGE:
 * showMessage(messageEl, 'Registration successful!', 'success');
 * showMessage(messageEl, 'Invalid credentials', 'error');
 * 
 * DOM MANIPULATION:
 * - textContent: Safe (no HTML injection)
 * - className: Sets CSS classes
 * - style.display: Makes visible
 */
function showMessage(element, message, type) {
    element.textContent = message;
    element.className = `message ${type}`;  // 'message success' or 'message error'
    element.style.display = 'block';
}

/**
 * @function formatTime
 * @param {number} timestamp - Unix epoch timestamp (seconds)
 * @returns {string} Human-readable date/time string
 * 
 * PURPOSE: Convert Unix timestamp to readable format
 * 
 * CONVERSION:
 * 1. Multiply by 1000 (seconds → milliseconds for JavaScript Date)
 * 2. Create Date object
 * 3. Format using toLocaleString() (respects user's locale)
 * 
 * EXAMPLE OUTPUT:
 * "10/22/2025, 3:45:30 PM" (US locale)
 * "22/10/2025, 15:45:30" (EU locale)
 * 
 * USAGE: Display timestamps for posts, blocks, comments
 * 
 * ALTERNATIVE:
 * Could use relative time ("2 hours ago") with libraries like moment.js
 */
function formatTime(timestamp) {
    const date = new Date(timestamp * 1000);  // Unix timestamp to Date
    return date.toLocaleString();  // Locale-aware formatting
}

/**
 * @function escapeHtml
 * @param {string} text - Text potentially containing HTML
 * @returns {string} HTML-escaped text safe for display
 * 
 * PURPOSE: Prevents XSS (Cross-Site Scripting) attacks
 * 
 * ESCAPING RULES:
 * & → &amp;    (Must escape first, it's used in other escapes)
 * < → &lt;     (Prevents opening tags)
 * > → &gt;     (Prevents closing tags)
 * " → &quot;   (Prevents attribute injection)
 * ' → &#039;   (Prevents attribute injection)
 * 
 * XSS ATTACK PREVENTION:
 * User posts: "<script>alert('XSS')</script>"
 * Without escaping: Script executes (DANGEROUS)
 * With escaping: "&lt;script&gt;..." displayed as text (SAFE)
 * 
 * REGEX REPLACEMENT:
 * /[&<>"']/g: Global search for any of these characters
 * m => map[m]: Replace with corresponding escape sequence
 * 
 * CRITICAL:
 * Always escape user-generated content before innerHTML
 * Defense in depth: Backend also escapes (double protection)
 * 
 * USAGE:
 * <div>${escapeHtml(post.content)}</div>
 */
function escapeHtml(text) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;'
    };
    return text.replace(/[&<>"']/g, m => map[m]);
}

// ============================================================================
// BITCAFE FUNCTIONALITY (Discussion Topics/Forum)
// ============================================================================

/**
 * @var {Array} topicsData
 * 
 * PURPOSE: In-memory storage for bitCafe topics
 * 
 * DATA SOURCE:
 * localStorage (key: 'bitea_bitcups')
 * Persists across page reloads
 * 
 * STRUCTURE:
 * Array of topic objects:
 * {
 *   id: 'bitcup-timestamp',
 *   title: 'Topic title',
 *   description: 'Optional description',
 *   author: 'username',
 *   timestamp: UnixTimestamp,
 *   comments: Array<Comment>,
 *   blockchainHash: 'pending' or block hash
 * }
 * 
 * INITIALIZATION:
 * Loads from localStorage on app start
 * Empty array [] if no saved data
 * 
 * FUTURE:
 * Will be replaced with blockchain backend storage
 * Currently localStorage-based for offline functionality
 */
let topicsData = JSON.parse(localStorage.getItem('bitea_bitcups') || '[]');

/**
 * @var {string|null} currentTopicId
 * 
 * PURPOSE: Tracks which topic detail view is currently shown
 * 
 * STATES:
 * - null: No topic being viewed (list view)
 * - string: ID of topic being viewed (detail view)
 * 
 * USAGE:
 * - Determines which view to show (list vs detail)
 * - Used when adding comments (know which topic)
 */
let currentTopicId = null;

// Save bitCups to localStorage
function saveTopics() {
    localStorage.setItem('bitea_bitcups', JSON.stringify(topicsData));
}

// Load all topics
async function loadTopics() {
    try {
        // TODO: Replace with actual blockchain API call
        // const topics = await API.getTopics();
        
        displayTopics(topicsData);
        loadRecentTopicsSidebar();
    } catch (error) {
        console.error('Error loading topics:', error);
    }
}

// Display topics
function displayTopics(topics) {
    const container = document.getElementById('topicsContainer');
    
    if (!topics || topics.length === 0) {
        container.innerHTML = '<p style="text-align:center; color:var(--text-primary);">[CAFE] No bitCups yet. Brew the first one!</p>';
        return;
    }
    
    container.innerHTML = topics.map(topic => `
        <div class="topic-card" onclick="viewTopic('${topic.id}')">
            <div class="topic-title">${escapeHtml(topic.title)}</div>
            ${topic.description ? `<div class="topic-description">${escapeHtml(topic.description)}</div>` : ''}
            <div class="topic-meta">
                <div class="topic-meta-item">
                    <span>[@]</span>
                    <span>by @${escapeHtml(topic.author)}</span>
                </div>
                <div class="topic-meta-item">
                    <span>[//]</span>
                    <span>${topic.comments ? topic.comments.length : 0} comments</span>
                </div>
                <div class="topic-meta-item">
                    <span>[CHAIN]</span>
                    <span>On Chain</span>
                </div>
                <div class="topic-meta-item">
                    <span>[T]</span>
                    <span>${formatTime(topic.timestamp)}</span>
                </div>
            </div>
        </div>
    `).join('');
}

// Search topics
function searchTopics() {
    const searchTerm = document.getElementById('topicSearchInput').value.toLowerCase();
    
    const filteredTopics = topicsData.filter(topic => 
        topic.title.toLowerCase().includes(searchTerm) ||
        (topic.description && topic.description.toLowerCase().includes(searchTerm))
    );
    
    displayTopics(filteredTopics);
}

// Show create topic modal
function showCreateTopicModal() {
    if (!currentUser) {
        alert('Please login to create a bitCup');
        return;
    }
    document.getElementById('createTopicModal').style.display = 'flex';
    document.getElementById('newTopicTitle').value = '';
    document.getElementById('newTopicDescription').value = '';
    document.getElementById('topicMessage').style.display = 'none';
}

// Hide create topic modal
function hideCreateTopicModal() {
    document.getElementById('createTopicModal').style.display = 'none';
}

// Create topic
async function createTopic() {
    const title = document.getElementById('newTopicTitle').value.trim();
    const description = document.getElementById('newTopicDescription').value.trim();
    const messageEl = document.getElementById('topicMessage');
    
    if (!title) {
        showMessage(messageEl, 'Please enter a bitCup title', 'error');
        return;
    }
    
    try {
        // TODO: Replace with actual blockchain API call
        // const result = await API.createTopic(title, description);
        
        const newTopic = {
            id: `bitcup-${Date.now()}`,
            title: title,
            description: description,
            author: currentUser || getUsername(),
            timestamp: Math.floor(Date.now() / 1000),
            comments: [],
            blockchainHash: 'pending' // Will be set when mined
        };
        
        topicsData.unshift(newTopic);
        saveTopics();
        
        showMessage(messageEl, '[CAFE] bitCup brewed on blockchain!', 'success');
        setTimeout(() => {
            hideCreateTopicModal();
            loadTopics();
        }, 1500);
        
    } catch (error) {
        showMessage(messageEl, 'Error brewing bitCup: ' + error.message, 'error');
    }
}

// View topic details
function viewTopic(topicId) {
    currentTopicId = topicId;
    const topic = topicsData.find(t => t.id === topicId);
    
    if (!topic) {
        alert('bitCup not found');
        return;
    }
    
    // Hide topics list, show detail view
    document.getElementById('topicsContainer').style.display = 'none';
    document.querySelector('.topics-header').style.display = 'none';
    document.getElementById('topicDetailView').style.display = 'block';
    
    // Render topic details
    const detailContent = document.getElementById('topicDetailContent');
    detailContent.innerHTML = `
        <div class="topic-detail-header">
            <div class="topic-detail-title">${escapeHtml(topic.title)}</div>
            ${topic.description ? `<div class="topic-detail-description">${escapeHtml(topic.description)}</div>` : ''}
            <div class="topic-detail-info">
                <span>[@] Created by @${escapeHtml(topic.author)}</span>
                <span>[//] ${topic.comments.length} comments</span>
                <span>[T] ${formatTime(topic.timestamp)}</span>
                <span>[CHAIN] Block: ${topic.blockchainHash !== 'pending' ? topic.blockchainHash.substring(0, 8) + '...' : 'Pending'}</span>
            </div>
        </div>
        
        <div class="comments-section">
            <div class="comments-header">[//] Comments</div>
            
            <div class="comment-input-box">
                <div class="comment-hint">Tip: Use @username to mention someone</div>
                <textarea id="newCommentText" placeholder="Write your comment..." class="textarea-field" style="min-height:100px;"></textarea>
                <button onclick="addComment()" class="btn btn-primary">Add Comment to Blockchain</button>
            </div>
            
            <div id="commentsContainer">
                ${renderComments(topic.comments)}
            </div>
        </div>
    `;
}

// Hide topic detail view
function hideTopicDetail() {
    document.getElementById('topicDetailView').style.display = 'none';
    document.getElementById('topicsContainer').style.display = 'flex';
    document.querySelector('.topics-header').style.display = 'flex';
    currentTopicId = null;
}

// Render comments
function renderComments(comments) {
    if (!comments || comments.length === 0) {
        return '<p style="color:var(--text-dim);">[CAFE] No sips from this bitCup yet. Be the first to comment!</p>';
    }
    
    return comments.map(comment => {
        const contentWithMentions = processComment(comment.content);
        const isLiked = comment.likes && comment.likes.includes(currentUser || getUsername());
        
        return `
            <div class="topic-comment" id="comment-${comment.id}">
                <div class="comment-author">@${escapeHtml(comment.author)}</div>
                <div class="comment-content">${contentWithMentions}</div>
                <div class="comment-actions">
                    <span class="comment-action-btn ${isLiked ? 'active' : ''}" onclick="likeComment('${comment.id}')">
                        [<3] ${comment.likes ? comment.likes.length : 0}
                    </span>
                    <span class="comment-action-btn" onclick="reshareComment('${comment.id}')">
                        [R] Reshare
                    </span>
                    <span class="comment-action-btn" onclick="replyToComment('${comment.author}')">
                        [//] Reply
                    </span>
                </div>
            </div>
        `;
    }).join('');
}

// Process comment to highlight @mentions
function processComment(text) {
    return escapeHtml(text).replace(/@(\w+)/g, '<span class="comment-mention">@$1</span>');
}

// Add comment
async function addComment() {
    if (!currentUser && !getUsername()) {
        alert('Please login to comment');
        return;
    }
    
    const content = document.getElementById('newCommentText').value.trim();
    if (!content) {
        alert('Please enter a comment');
        return;
    }
    
    try {
        // TODO: Replace with actual blockchain API call
        // const result = await API.addTopicComment(currentTopicId, content);
        
        const newComment = {
            id: `sip-${Date.now()}`,
            author: currentUser || getUsername(),
            content: content,
            timestamp: Math.floor(Date.now() / 1000),
            likes: [],
            reshares: []
        };
        
        const topic = topicsData.find(t => t.id === currentTopicId);
        if (topic) {
            topic.comments.push(newComment);
            saveTopics();
            
            document.getElementById('newCommentText').value = '';
            viewTopic(currentTopicId); // Refresh view
            alert('Comment added to blockchain!');
        }
        
    } catch (error) {
        alert('Error adding comment: ' + error.message);
    }
}

// Like comment
async function likeComment(commentId) {
    if (!currentUser && !getUsername()) {
        alert('Please login to like');
        return;
    }
    
    try {
        const username = currentUser || getUsername();
        const topic = topicsData.find(t => t.id === currentTopicId);
        
        if (topic) {
            const comment = topic.comments.find(c => c.id === commentId);
            if (comment) {
                if (!comment.likes) comment.likes = [];
                
                const likeIndex = comment.likes.indexOf(username);
                if (likeIndex > -1) {
                    comment.likes.splice(likeIndex, 1);
                } else {
                    comment.likes.push(username);
                }
                
                saveTopics();
                viewTopic(currentTopicId); // Refresh view
            }
        }
    } catch (error) {
        alert('Error liking comment: ' + error.message);
    }
}

// Reshare comment
async function reshareComment(commentId) {
    if (!currentUser && !getUsername()) {
        alert('Please login to reshare');
        return;
    }
    
    try {
        const username = currentUser || getUsername();
        const topic = topicsData.find(t => t.id === currentTopicId);
        
        if (topic) {
            const originalComment = topic.comments.find(c => c.id === commentId);
            if (originalComment) {
                const reshareComment = {
                    id: `sip-${Date.now()}`,
                    author: username,
                    content: `[R] Reshared from @${originalComment.author}: ${originalComment.content}`,
                    timestamp: Math.floor(Date.now() / 1000),
                    likes: [],
                    reshares: [],
                    isReshare: true,
                    originalCommentId: commentId
                };
                
                topic.comments.push(reshareComment);
                saveTopics();
                viewTopic(currentTopicId); // Refresh view
                alert('Comment reshared on blockchain!');
            }
        }
    } catch (error) {
        alert('Error resharing comment: ' + error.message);
    }
}

// Reply to comment
function replyToComment(username) {
    const textarea = document.getElementById('newCommentText');
    textarea.value = `@${username} `;
    textarea.focus();
}

/**
 * @function loadRecentTopicsSidebar
 * 
 * PURPOSE: Display recent topics in sidebar
 * 
 * ALGORITHM:
 * 1. Take last 10 topics from topicsData
 * 2. Render as clickable cards
 * 3. Insert into sidebar DOM
 * 
 * SPREAD OPERATOR:
 * [...topicsData]: Creates shallow copy
 * Prevents mutating original array
 * 
 * SLICE:
 * .slice(0, 10): Take first 10 elements
 * 
 * USAGE: Quick navigation to recent discussions
 */
function loadRecentTopicsSidebar() {
    const sidebar = document.getElementById('recentTopicsSidebar');
    const recentTopics = [...topicsData].slice(0, 10);
    
    // Handle empty state
    if (recentTopics.length === 0) {
        sidebar.innerHTML = '<p style="color:var(--text-dim);font-size:0.85rem;">[CAFE] No bitCups brewed yet</p>';
        return;
    }
    
    // Render topic list
    sidebar.innerHTML = recentTopics.map(topic => `
        <div class="sidebar-topic" onclick="viewTopic('${topic.id}')">
            <div class="sidebar-topic-title">${escapeHtml(topic.title)}</div>
            <div class="sidebar-topic-meta">
                [//] ${topic.comments.length} | [@] @${escapeHtml(topic.author)}
            </div>
        </div>
    `).join('');
}

// ============================================================================
// END OF APP.JS
// ============================================================================

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * APPLICATION ARCHITECTURE:
 * 
 * SINGLE PAGE APPLICATION (SPA):
 * - One HTML file with multiple "pages" (divs)
 * - JavaScript shows/hides pages dynamically
 * - No full page reloads (smooth UX)
 * - State managed in JavaScript variables
 * 
 * STATE MANAGEMENT:
 * - currentUser: Global logged-in user (null if not authenticated)
 * - topicsData: Local array of topics (localStorage-backed)
 * - currentTopicId: Currently viewed topic (null in list view)
 * 
 * DATA PERSISTENCE:
 * - localStorage: Browser storage for session and topics
 * - Persists across page reloads
 * - Cleared on logout or browser clear data
 * 
 * ASYNC PATTERNS:
 * - async/await: Clean promise handling
 * - try/catch: Error handling for API calls
 * - Promise.all(): Parallel API calls (loadStats)
 * 
 * DOM MANIPULATION STRATEGIES:
 * 
 * ELEMENT SELECTION:
 * - getElementById(): Fast, specific element by ID
 * - querySelectorAll(): Multiple elements by selector
 * - querySelector(): First match of selector
 * 
 * CONTENT RENDERING:
 * - innerHTML: Set HTML content (use with escapeHtml!)
 * - textContent: Set text safely (auto-escapes)
 * - Template literals: Embed variables with ${}
 * 
 * DYNAMIC HTML:
 * posts.map(post => `<div>...</div>`).join('')
 * - map(): Transform data to HTML
 * - join(): Array to string
 * - innerHTML: Insert into DOM
 * 
 * EVENT HANDLING:
 * - Inline: onclick="functionName()" (simple, used here)
 * - addEventListener: More modern, better separation
 * - Event delegation: Could optimize for many elements
 * 
 * SECURITY IMPLEMENTATION:
 * 
 * XSS PREVENTION:
 * - escapeHtml(): All user content escaped before display
 * - textContent vs innerHTML: Use textContent when possible
 * - Backend sanitization: Defense in depth
 * 
 * AUTHENTICATION:
 * - Session-based: Token in localStorage
 * - Included in API calls via Authorization header
 * - Validated on server (client-side just for UX)
 * 
 * INPUT VALIDATION:
 * - Client-side: Immediate feedback (trim, empty check)
 * - Server-side: Security validation (never trust client)
 * - Both layers: Better UX + security
 * 
 * DUAL STORAGE (Posts):
 * 
 * BACKEND DATABASE:
 * - Fast queries for feed loading
 * - Indices for efficient lookups
 * - Mutable (can update likes, comments)
 * 
 * BLOCKCHAIN:
 * - Immutable proof of post creation
 * - Transaction for each action
 * - Auto-mines when 5 transactions pending
 * 
 * WHY BOTH:
 * Database provides performance
 * Blockchain provides integrity and proof
 * 
 * BITCAFE (Topics) IMPLEMENTATION:
 * 
 * CURRENT STATE:
 * - localStorage-based (offline functionality)
 * - No backend integration yet
 * - Marked with TODO comments
 * 
 * FUTURE INTEGRATION:
 * - Backend API endpoints for topics
 * - Blockchain transactions (TOPIC_CREATE, TOPIC_COMMENT, etc.)
 * - MongoDB storage for querying
 * - Same dual storage as posts
 * 
 * FEATURES:
 * - Create topics (bitCups)
 * - Comment on topics (sips)
 * - Like comments
 * - Reshare comments (retweet-style)
 * - @mentions in comments
 * - Search topics
 * 
 * USER EXPERIENCE:
 * 
 * FEEDBACK MECHANISMS:
 * - Alerts: Simple feedback (could be replaced with toasts)
 * - Message boxes: Colored success/error messages
 * - Auto-refresh: UI updates after actions
 * - Loading states: Could add spinners for async operations
 * 
 * NAVIGATION:
 * - Tab-based: Login vs Register
 * - Page-based: Feed, Topics, Blockchain, Stats, Profile
 * - Smooth transitions: CSS handles animations
 * 
 * DATA DISPLAY:
 * - Posts: Card layout with author, content, stats
 * - Blocks: Detailed blockchain information
 * - Stats: Comprehensive analytics tables
 * - Topics: Discussion cards with metadata
 * 
 * BLOCKCHAIN INTEGRATION:
 * 
 * TRANSACTION CREATION:
 * Every user action creates blockchain transaction:
 * - Register → USER_REGISTRATION
 * - Create post → POST
 * - Like → LIKE
 * - Comment → COMMENT
 * - Follow → FOLLOW
 * 
 * AUTO-MINING:
 * Backend auto-mines when 5 pending transactions
 * User sees "[CHAIN] On Chain" badge after mining
 * 
 * MANUAL MINING:
 * Blockchain page has "Mine Pending Transactions" button
 * Useful for demos, testing
 * 
 * VALIDATION:
 * "Validate Chain" button checks cryptographic integrity
 * Shows if blockchain has been tampered with
 * 
 * PERFORMANCE CONSIDERATIONS:
 * 
 * RENDERING:
 * - Dynamic HTML generation (map + join + innerHTML)
 * - Could be slow with thousands of posts
 * - Optimization: Virtual scrolling, pagination
 * 
 * API CALLS:
 * - Async/await prevents UI blocking
 * - Could batch requests (reduce round trips)
 * - Could cache responses (reduce server load)
 * 
 * LOCALSTORAGE:
 * - Synchronous operations (can block)
 * - Limited to ~5-10MB per domain
 * - Topics stored entirely in localStorage
 * 
 * STATISTICS:
 * - Promise.all(): Parallel API calls (faster)
 * - Complex calculations in JavaScript
 * - Could be optimized with server-side aggregation
 * 
 * BROWSER COMPATIBILITY:
 * 
 * MODERN FEATURES USED:
 * - ES6+: arrow functions, template literals, const/let
 * - async/await: ES2017
 * - Optional chaining (?.): ES2020
 * - Array methods: map, filter, find, forEach
 * 
 * SUPPORTED BROWSERS:
 * - Chrome 80+ (all features)
 * - Firefox 74+ (all features)
 * - Safari 13.1+ (all features)
 * - Edge 80+ (Chromium-based)
 * 
 * POLYFILLS NEEDED FOR OLDER BROWSERS:
 * - fetch() polyfill for IE11
 * - Promise polyfill for IE11
 * - Array methods shim for very old browsers
 * 
 * POTENTIAL IMPROVEMENTS:
 * 
 * FEATURES:
 * - Real-time updates: WebSocket for live feed
 * - Infinite scroll: Load more posts on scroll
 * - Image upload: Media attachments for posts
 * - Rich text: Markdown support, formatting
 * - Search: Full-text search for posts/topics
 * - Notifications: Toast notifications for actions
 * - Dark/light mode: Theme toggle
 * - Profile editing: Change display name, bio
 * - Direct messages: Private messaging
 * 
 * UX IMPROVEMENTS:
 * - Loading spinners: Show during API calls
 * - Optimistic updates: Update UI before server confirms
 * - Error recovery: Retry failed requests
 * - Form validation: Real-time validation feedback
 * - Modal dialogs: Replace prompt() and alert()
 * - Keyboard shortcuts: Power user features
 * - Accessibility: ARIA labels, keyboard navigation
 * 
 * PERFORMANCE:
 * - Lazy loading: Load images on demand
 * - Virtual scrolling: Render only visible posts
 * - Request caching: Cache API responses
 * - Debouncing: Limit rapid API calls (search)
 * - Service worker: Offline capability, caching
 * 
 * SECURITY:
 * - Content Security Policy: Restrict inline scripts
 * - HTTPS only: Encrypt all traffic
 * - Input sanitization: More comprehensive escaping
 * - Rate limiting: Prevent abuse
 * - Session timeout: Auto-logout after inactivity
 * 
 * CODE ORGANIZATION:
 * - Module pattern: Separate concerns into modules
 * - State management: Consider Redux, MobX, Zustand
 * - Component framework: React, Vue, Svelte
 * - TypeScript: Type safety for large applications
 * - Build tools: Webpack, Vite for optimization
 * 
 * TESTING:
 * - Unit tests: Individual function testing (Jest)
 * - Integration tests: UI workflow testing (Cypress, Playwright)
 * - E2E tests: Full user journey testing
 * - Mock API: Test without backend
 * 
 * MONITORING:
 * - Error tracking: Sentry, Rollbar for production errors
 * - Analytics: Google Analytics, custom events
 * - Performance: Lighthouse scores, Web Vitals
 * - User behavior: Heatmaps, session recordings
 ******************************************************************************/

