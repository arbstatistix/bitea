// Application State
let currentUser = null;

// Initialize app
document.addEventListener('DOMContentLoaded', () => {
    checkAuth();
    loadStats();
});

// Page Navigation
function showPage(pageName) {
    document.querySelectorAll('.page').forEach(page => {
        page.classList.remove('active');
    });
    
    const page = document.getElementById(`${pageName}Page`);
    if (page) {
        page.classList.add('active');
        
        // Load page data
        if (pageName === 'feed') {
            loadFeed();
            loadStats();
        } else if (pageName === 'blockchain') {
            loadBlockchain();
        } else if (pageName === 'profile') {
            loadProfile();
        }
    }
}

// Authentication
function checkAuth() {
    const sessionId = getSession();
    const username = getUsername();
    
    if (sessionId && username) {
        currentUser = username;
        document.getElementById('loginBtn').style.display = 'none';
        document.getElementById('logoutBtn').style.display = 'block';
        document.getElementById('profileLink').style.display = 'block';
        showPage('feed');
    } else {
        showPage('login');
    }
}

function switchTab(tab) {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    if (tab === 'login') {
        document.querySelector('.tab-btn:first-child').classList.add('active');
        document.getElementById('loginForm').style.display = 'flex';
        document.getElementById('registerForm').style.display = 'none';
    } else {
        document.querySelector('.tab-btn:last-child').classList.add('active');
        document.getElementById('loginForm').style.display = 'none';
        document.getElementById('registerForm').style.display = 'flex';
    }
    
    document.getElementById('authMessage').textContent = '';
}

async function register() {
    const username = document.getElementById('registerUsername').value.trim();
    const email = document.getElementById('registerEmail').value.trim();
    const password = document.getElementById('registerPassword').value;
    const messageEl = document.getElementById('authMessage');
    
    if (!username || !email || !password) {
        showMessage(messageEl, 'Please fill in all fields', 'error');
        return;
    }
    
    try {
        await API.register(username, email, password);
        showMessage(messageEl, 'Registration successful! Please login.', 'success');
        setTimeout(() => switchTab('login'), 2000);
    } catch (error) {
        showMessage(messageEl, error.message, 'error');
    }
}

async function login() {
    const username = document.getElementById('loginUsername').value.trim();
    const password = document.getElementById('loginPassword').value;
    const messageEl = document.getElementById('authMessage');
    
    if (!username || !password) {
        showMessage(messageEl, 'Please fill in all fields', 'error');
        return;
    }
    
    try {
        const result = await API.login(username, password);
        setSession(result.sessionId, result.user.username);
        showMessage(messageEl, 'Login successful!', 'success');
        setTimeout(() => {
            checkAuth();
        }, 1000);
    } catch (error) {
        showMessage(messageEl, error.message, 'error');
    }
}

async function logout() {
    try {
        await API.logout();
    } catch (error) {
        console.error('Logout error:', error);
    }
    
    clearSession();
    currentUser = null;
    document.getElementById('loginBtn').style.display = 'block';
    document.getElementById('logoutBtn').style.display = 'none';
    document.getElementById('profileLink').style.display = 'none';
    showPage('login');
}

// Feed Functions
async function loadFeed() {
    try {
        const posts = await API.getPosts();
        displayPosts(posts);
    } catch (error) {
        console.error('Error loading feed:', error);
    }
}

async function createPost() {
    const content = document.getElementById('postContent').value.trim();
    
    if (!content) {
        alert('Please enter some content');
        return;
    }
    
    try {
        await API.createPost(content);
        document.getElementById('postContent').value = '';
        loadFeed();
        loadStats();
        alert('Post created and added to blockchain!');
    } catch (error) {
        alert('Error creating post: ' + error.message);
    }
}

function displayPosts(posts) {
    const container = document.getElementById('postsContainer');
    
    if (!posts || posts.length === 0) {
        container.innerHTML = '<p style="text-align:center; color:white;">No posts yet. Be the first to post!</p>';
        return;
    }
    
    container.innerHTML = posts.map(post => `
        <div class="post-card">
            <div class="post-header">
                <span class="post-author">@${post.author}</span>
                <span class="post-time">${formatTime(post.timestamp)}</span>
            </div>
            <div class="post-content">${escapeHtml(post.content)}</div>
            <div class="post-stats">
                <span>❤️ ${post.likes} likes</span>
                <span>💬 ${post.comments} comments</span>
                ${post.isOnChain ? '<span class="blockchain-badge">⛓️ On Chain</span>' : ''}
            </div>
            <div class="post-actions">
                <button class="btn btn-small btn-secondary" onclick="likePost('${post.id}')">❤️ Like</button>
                <button class="btn btn-small btn-secondary" onclick="viewPost('${post.id}')">💬 Comment</button>
            </div>
        </div>
    `).join('');
}

async function likePost(postId) {
    try {
        await API.likePost(postId);
        loadFeed();
        loadStats();
    } catch (error) {
        alert('Error liking post: ' + error.message);
    }
}

async function viewPost(postId) {
    try {
        const post = await API.getPost(postId);
        const content = prompt('Add a comment:');
        
        if (content && content.trim()) {
            await API.commentPost(postId, content.trim());
            loadFeed();
            loadStats();
            alert('Comment added to blockchain!');
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Blockchain Functions
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
                    ✅ Blockchain is VALID! All blocks are correctly linked and verified.
                </div>
            `;
        } else {
            statusEl.innerHTML = `
                <div class="message error">
                    ❌ Blockchain validation failed! Chain may have been tampered with.
                </div>
            `;
        }
    } catch (error) {
        document.getElementById('blockchainStatus').innerHTML = `
            <div class="message error">Error: ${error.message}</div>
        `;
    }
}

// Stats
async function loadStats() {
    try {
        const info = await API.getApiInfo();
        
        document.getElementById('totalPosts').textContent = info.database.posts;
        document.getElementById('totalBlocks').textContent = info.blockchain.blocks;
        document.getElementById('pendingTxs').textContent = info.blockchain.pending;
    } catch (error) {
        console.error('Error loading stats:', error);
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

// Utility Functions
function showMessage(element, message, type) {
    element.textContent = message;
    element.className = `message ${type}`;
    element.style.display = 'block';
}

function formatTime(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleString();
}

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

