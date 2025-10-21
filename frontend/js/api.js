// API Configuration
const API_BASE = 'http://localhost:3000';

// Local storage keys
const SESSION_KEY = 'bitea_session';
const USERNAME_KEY = 'bitea_username';

// Session management
function getSession() {
    return localStorage.getItem(SESSION_KEY);
}

function setSession(sessionId, username) {
    localStorage.setItem(SESSION_KEY, sessionId);
    localStorage.setItem(USERNAME_KEY, username);
}

function clearSession() {
    localStorage.removeItem(SESSION_KEY);
    localStorage.removeItem(USERNAME_KEY);
}

function getUsername() {
    return localStorage.getItem(USERNAME_KEY);
}

// API Helper
async function apiCall(endpoint, method = 'GET', body = null) {
    const options = {
        method,
        headers: {
            'Content-Type': 'application/json'
        }
    };

    const sessionId = getSession();
    if (sessionId) {
        options.headers['Authorization'] = `Bearer ${sessionId}`;
    }

    if (body) {
        options.body = JSON.stringify(body);
    }

    try {
        const response = await fetch(`${API_BASE}${endpoint}`, options);
        const data = await response.json();
        
        if (!response.ok) {
            throw new Error(data.error || 'Request failed');
        }
        
        return data;
    } catch (error) {
        console.error('API Error:', error);
        throw error;
    }
}

// API Endpoints
const API = {
    // Auth
    register: (username, email, password) => 
        apiCall('/api/register', 'POST', { username, email, password }),
    
    login: (username, password) => 
        apiCall('/api/login', 'POST', { username, password }),
    
    logout: () => 
        apiCall('/api/logout', 'POST'),
    
    // Posts
    getPosts: () => 
        apiCall('/api/posts'),
    
    getPost: (id) => 
        apiCall(`/api/posts/${id}`),
    
    createPost: (content) => 
        apiCall('/api/posts', 'POST', { content }),
    
    likePost: (id) => 
        apiCall(`/api/posts/${id}/like`, 'POST'),
    
    commentPost: (id, content) => 
        apiCall(`/api/posts/${id}/comment`, 'POST', { content }),
    
    // Users
    getUser: (username) => 
        apiCall(`/api/users/${username}`),
    
    followUser: (username) => 
        apiCall(`/api/users/${username}/follow`, 'POST'),
    
    // Blockchain
    getBlockchain: () => 
        apiCall('/api/blockchain'),
    
    validateBlockchain: () => 
        apiCall('/api/blockchain/validate'),
    
    mineBlock: () => 
        apiCall('/api/mine'),
    
    // Info
    getApiInfo: () => 
        apiCall('/api')
};

