// Frontend Configuration Example
// Copy this to config.js and modify for your environment

const CONFIG = {
    // API endpoint - change this to your production API URL
    API_BASE: window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1'
        ? 'http://localhost:3000'
        : 'https://your-production-api.com',
    
    // Feature flags
    FEATURES: {
        BLOCKCHAIN_VIEW: true,
        DEBUG_MODE: false
    },
    
    // Local storage keys
    STORAGE_KEYS: {
        SESSION: 'bitea_session',
        USERNAME: 'bitea_username'
    }
};

// Make config available globally
window.BITEA_CONFIG = CONFIG;

