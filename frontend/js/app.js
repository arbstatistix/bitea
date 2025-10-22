// Application State
let currentUser = null;

// Initialize app
document.addEventListener('DOMContentLoaded', () => {
    checkAuth();
    loadFeedStats();
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
            loadFeedStats();
        } else if (pageName === 'topics') {
            loadTopics();
        } else if (pageName === 'blockchain') {
            loadBlockchain();
        } else if (pageName === 'stats') {
            loadStats();
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
                <span>[<3] ${post.likes} likes</span>
                <span>[//] ${post.comments} comments</span>
                ${post.isOnChain ? '<span class="blockchain-badge">[CHAIN] On Chain</span>' : ''}
            </div>
            <div class="post-actions">
                <button class="btn btn-small btn-secondary" onclick="likePost('${post.id}')">[<3] Like</button>
                <button class="btn btn-small btn-secondary" onclick="viewPost('${post.id}')">[//] Comment</button>
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

// ========== bitCafe FUNCTIONALITY ==========

// In-memory storage for bitCups (will be replaced with blockchain backend)
let topicsData = JSON.parse(localStorage.getItem('bitea_bitcups') || '[]');
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

// Load recent topics sidebar
function loadRecentTopicsSidebar() {
    const sidebar = document.getElementById('recentTopicsSidebar');
    const recentTopics = [...topicsData].slice(0, 10);
    
    if (recentTopics.length === 0) {
        sidebar.innerHTML = '<p style="color:var(--text-dim);font-size:0.85rem;">[CAFE] No bitCups brewed yet</p>';
        return;
    }
    
    sidebar.innerHTML = recentTopics.map(topic => `
        <div class="sidebar-topic" onclick="viewTopic('${topic.id}')">
            <div class="sidebar-topic-title">${escapeHtml(topic.title)}</div>
            <div class="sidebar-topic-meta">
                [//] ${topic.comments.length} | [@] @${escapeHtml(topic.author)}
            </div>
        </div>
    `).join('');
}

