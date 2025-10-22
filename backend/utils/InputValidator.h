/*******************************************************************************
 * INPUTVALIDATOR.H - Input Validation and Sanitization Utilities
 * 
 * PURPOSE:
 * This header defines the InputValidator class, providing static methods for
 * validating and sanitizing user input. Prevents injection attacks (XSS, SQL,
 * NoSQL), enforces data constraints, and ensures data integrity.
 * 
 * SECURITY IMPORTANCE:
 * User input is the #1 attack vector in web applications
 * Never trust client data - always validate and sanitize
 * Defense in depth: Multiple layers of validation
 * 
 * VALIDATION TYPES:
 * 1. Format validation: Does input match expected pattern?
 * 2. Range validation: Is input within allowed bounds?
 * 3. Sanitization: Remove/escape dangerous characters
 * 4. Business logic: Does input make sense for use case?
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - main.cpp: API handlers use these validators before processing
 * - User.h: Username, email, password validation
 * - Post.h: Content validation before storage
 * - Session.h: Session ID format validation
 * - Database: Prevents injection attacks
 * 
 * ATTACK VECTORS PREVENTED:
 * - XSS (Cross-Site Scripting): Escape HTML characters
 * - SQL Injection: Escape quotes and special characters
 * - NoSQL Injection: Sanitize for MongoDB queries
 * - Buffer overflow: Length limits on all inputs
 * - Control character injection: Remove non-printable chars
 * 
 * DESIGN PATTERN:
 * Static utility class (all methods static, no instance needed)
 * Stateless functions (pure, no side effects)
 * 
 * WHY STATIC:
 * No need for object instance (no state to maintain)
 * Call directly: InputValidator::sanitize(input)
 * 
 * REFERENCES:
 * - OWASP Input Validation: https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html
 * - OWASP XSS Prevention: https://cheatsheetseries.owasp.org/cheatsheets/Cross_Site_Scripting_Prevention_Cheat_Sheet.html
 * - Regex Best Practices: "Mastering Regular Expressions" by Jeffrey Friedl
 ******************************************************************************/

#ifndef INPUTVALIDATOR_H
#define INPUTVALIDATOR_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>      // std::string - input/output strings
#include <regex>       // std::regex - pattern matching for validation
#include <algorithm>   // std::any_of, std::all_of - validation helpers
#include <cctype>      // std::isalpha, std::isdigit, std::isprint - character classification

// ============================================================================
// INPUT VALIDATOR CLASS (Static Utility Class)
// ============================================================================

/**
 * @class InputValidator
 * @brief Static utility class for input validation and sanitization
 * 
 * DESIGN: Pure static methods (no instance state)
 * 
 * USAGE:
 * if (InputValidator::isValidEmail(email)) { ... }
 * std::string safe = InputValidator::sanitize(userInput);
 * 
 * VALIDATION PHILOSOPHY:
 * - Whitelist > Blacklist: Specify what IS allowed, not what ISN'T
 * - Fail securely: Reject invalid input, don't try to "fix" it
 * - Multiple checks: Combine regex, length, character class validation
 * - Early validation: Check at API boundary before processing
 * 
 * SECURITY LAYERS:
 * 1. Client-side validation (convenience, UX)
 * 2. Server-side validation (CRITICAL - never trust client)
 * 3. Database layer (parameterized queries, additional checks)
 * 4. Sanitization (escape output for display)
 */
class InputValidator {
public:
    // ========================================================================
    // SANITIZATION METHODS (XSS Prevention)
    // ========================================================================
    
    /**
     * @brief Sanitizes string by escaping HTML special characters
     * @param input String to sanitize
     * @return std::string - Sanitized string safe for HTML display
     * 
     * PURPOSE: Prevents XSS (Cross-Site Scripting) attacks
     * 
     * XSS ATTACK EXAMPLE:
     * User posts: "<script>alert('XSS')</script>"
     * Without sanitization: Script executes in victim's browser
     * With sanitization: "&lt;script&gt;alert('XSS')&lt;/script&gt;" (harmless text)
     * 
     * ESCAPING RULES (HTML Entity Encoding):
     * < → &lt;     (Less than, starts HTML tags)
     * > → &gt;     (Greater than, ends HTML tags)
     * & → &amp;    (Ampersand, starts entities)
     * " → &quot;   (Quote, attribute delimiter)
     * ' → &#39;    (Apostrophe, attribute delimiter)
     * 
     * CONTROL CHARACTERS:
     * Characters < 32 (ASCII control chars) removed EXCEPT:
     * - \n (newline, 10): Allowed for multi-line text
     * - \t (tab, 9): Allowed for formatting
     * 
     * WHY REMOVE CONTROL CHARS:
     * - Prevent terminal injection
     * - Avoid display issues
     * - Block non-printable characters
     * 
     * PERFORMANCE:
     * - reserve(): Pre-allocate space (reduces reallocation)
     * - Single pass: O(n) where n = input length
     * 
     * USAGE:
     * Display user content: Always sanitize before showing
     * Database storage: Can store original, sanitize on output
     * 
     * ALTERNATIVE APPROACHES:
     * - HTML parser (more thorough but complex)
     * - Content Security Policy (CSP headers)
     * - Template engines with auto-escaping
     * 
     * LIMITATIONS:
     * Basic XSS prevention only
     * Production: Use comprehensive library or framework
     */
    static std::string sanitize(const std::string& input) {
        std::string result;
        result.reserve(input.length());  // Pre-allocate space (optimization)
        
        for (char c : input) {
            // Remove control characters except newline and tab
            // Control chars: 0-31 (non-printable)
            if (c < 32 && c != '\n' && c != '\t') {
                continue;  // Skip this character
            }
            
            // Escape special HTML characters for XSS prevention
            switch (c) {
                case '<':
                    result += "&lt;";    // Prevent tag injection
                    break;
                case '>':
                    result += "&gt;";    // Prevent tag injection
                    break;
                case '&':
                    result += "&amp;";   // Must escape & (entity prefix)
                    break;
                case '"':
                    result += "&quot;";  // Prevent attribute injection
                    break;
                case '\'':
                    result += "&#39;";   // Prevent attribute injection
                    break;
                default:
                    result += c;         // Safe character, keep as-is
            }
        }
        
        return result;
    }

    // ========================================================================
    // FORMAT VALIDATION METHODS
    // ========================================================================
    
    /**
     * @brief Validates username format
     * @param username Username to validate
     * @return bool - true if valid, false otherwise
     * 
     * PURPOSE: Ensures username meets platform requirements
     * 
     * RULES:
     * - Length: 3-20 characters
     * - Characters: Letters (a-z, A-Z), digits (0-9), underscore (_)
     * - No special characters: Prevents display issues, injection
     * 
     * WHY THESE RULES:
     * - Min 3: Prevents very short usernames (spam, squatting)
     * - Max 20: Keeps display reasonable, prevents abuse
     * - Alphanumeric+underscore: Simple, URL-safe, database-safe
     * 
     * REGEX PATTERN: "^[a-zA-Z0-9_]+$"
     * - ^: Start of string
     * - [a-zA-Z0-9_]: Character class (allowed characters)
     * - +: One or more (required)
     * - $: End of string
     * 
     * EXAMPLES:
     * Valid: "alice", "bob_123", "user_name_42"
     * Invalid: "ab" (too short), "alice!", "user@name"
     * 
     * USAGE: User registration, profile updates
     */
    static bool isValidUsername(const std::string& username) {
        // Check length bounds
        if (username.length() < 3 || username.length() > 20) {
            return false;
        }
        
        // Check character pattern (alphanumeric + underscore only)
        std::regex usernamePattern("^[a-zA-Z0-9_]+$");
        return std::regex_match(username, usernamePattern);
    }

    /**
     * @brief Validates email address format
     * @param email Email to validate
     * @return bool - true if valid format, false otherwise
     * 
     * PURPOSE: Ensures email is properly formatted
     * 
     * RULES:
     * - Max length: 254 characters (RFC 5321 limit)
     * - Pattern: local@domain.tld
     * 
     * REGEX PATTERN:
     * "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
     * 
     * BREAKDOWN:
     * - [a-zA-Z0-9._%+-]+: Local part (before @)
     * - @: Required separator
     * - [a-zA-Z0-9.-]+: Domain name
     * - \\.: Literal dot (escaped)
     * - [a-zA-Z]{2,}: TLD (top-level domain, min 2 chars)
     * 
     * EXAMPLES:
     * Valid: "alice@example.com", "bob.smith@company.co.uk"
     * Invalid: "alice", "alice@", "@example.com", "alice@example"
     * 
     * LIMITATIONS:
     * Simplified email regex (full RFC 5322 is very complex)
     * Doesn't catch all invalid emails but prevents most common errors
     * 
     * PRODUCTION:
     * Send verification email to confirm email works
     * 
     * USAGE: User registration, email updates
     */
    static bool isValidEmail(const std::string& email) {
        // RFC 5321: Max email length is 254 characters
        if (email.length() > 254) {
            return false;
        }
        
        // Simplified email pattern (catches most cases)
        std::regex emailPattern(
            "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
        );
        return std::regex_match(email, emailPattern);
    }

    /**
     * @brief Validates password strength
     * @param password Password to validate
     * @return bool - true if meets strength requirements
     * 
     * PURPOSE: Enforces minimum password security standards
     * 
     * REQUIREMENTS:
     * - Length: 8-128 characters
     * - Complexity: At least one letter AND one digit
     * 
     * WHY THESE RULES:
     * - Min 8: NIST/OWASP minimum recommendation
     * - Max 128: Prevent extremely long passwords (DoS via bcrypt)
     * - Letter+digit: Basic complexity (harder to guess/crack)
     * 
     * STRENGTH LEVELS:
     * WEAK (rejected):
     * - "password" (no digit)
     * - "12345678" (no letter)
     * 
     * ACCEPTABLE (minimum):
     * - "password1" (letter + digit)
     * - "hello123"
     * 
     * STRONG (recommended, not enforced):
     * - "MyP@ssw0rd!2024" (mixed case, special chars, long)
     * 
     * FUTURE ENHANCEMENTS:
     * - Require special characters: !@#$%
     * - Require mixed case: Aa
     * - Check against common password list
     * - Entropy calculation
     * - Password strength meter
     * 
     * SECURITY TRADE-OFF:
     * Stricter rules = more secure but worse UX
     * Current: Balanced approach (basic security + usability)
     * 
     * USAGE: User registration, password change
     */
    static bool isValidPassword(const std::string& password) {
        // Check length bounds (8-128 characters)
        if (password.length() < 8 || password.length() > 128) {
            return false;
        }
        
        // Check for required character types
        bool hasLetter = false;
        bool hasDigit = false;
        
        for (char c : password) {
            if (std::isalpha(c)) hasLetter = true;   // a-z, A-Z
            if (std::isdigit(c)) hasDigit = true;    // 0-9
        }
        
        // Must have both letter and digit
        return hasLetter && hasDigit;
    }

    /**
     * @brief Validates post content
     * @param content Post content text
     * @return bool - true if valid content
     * 
     * PURPOSE: Ensures posts have meaningful content
     * 
     * RULES:
     * - Not empty
     * - Max 5000 characters (long-form posts)
     * - Not all whitespace
     * 
     * WHY 5000 CHARS:
     * - Twitter: 280 (very short)
     * - Facebook: ~63,206 (very long)
     * - Our choice: 5000 (medium-form, like blog post)
     * 
     * WHITESPACE CHECK:
     * std::any_of with !std::isspace
     * Returns true if ANY character is non-whitespace
     * Prevents posts like "   \n\t  " (only whitespace)
     * 
     * USAGE: Post creation, before storing in blockchain
     */
    static bool isValidPostContent(const std::string& content) {
        // Check length bounds
        if (content.empty() || content.length() > 5000) {
            return false;
        }
        
        // Check if content contains at least one non-whitespace character
        return std::any_of(content.begin(), content.end(), 
                          [](unsigned char c) { return !std::isspace(c); });
    }

    /**
     * @brief Validates user bio/description
     * @param bio Biography text
     * @return bool - true if valid
     * 
     * PURPOSE: Limit bio length to reasonable size
     * 
     * RULE: Max 500 characters
     * 
     * WHY 500:
     * Twitter: 160 chars
     * Instagram: ~150 chars
     * Our choice: 500 (more expressive)
     * 
     * EMPTY ALLOWED:
     * Bio is optional, empty string is valid
     */
    static bool isValidBio(const std::string& bio) {
        return bio.length() <= 500;
    }

    /**
     * @brief Validates display name
     * @param name Display name to validate
     * @return bool - true if valid
     * 
     * PURPOSE: Ensure display names are reasonable
     * 
     * RULES:
     * - Not empty: Must have a name
     * - Max 50 characters: Keep reasonable length
     * - Printable only: No control characters
     * 
     * PRINTABLE CHECK:
     * std::all_of with std::isprint
     * Returns true if ALL characters are printable (ASCII 32-126)
     * Blocks: Control chars, non-ASCII (emojis need special handling)
     * 
     * USAGE: Profile updates
     */
    static bool isValidDisplayName(const std::string& name) {
        // Check length bounds
        if (name.empty() || name.length() > 50) {
            return false;
        }
        
        // All characters must be printable (no control chars)
        return std::all_of(name.begin(), name.end(), 
                          [](unsigned char c) { return std::isprint(c); });
    }

    // ========================================================================
    // STRING MANIPULATION UTILITIES
    // ========================================================================
    
    /**
     * @brief Removes leading and trailing whitespace
     * @param str String to trim
     * @return std::string - Trimmed string
     * 
     * PURPOSE: Clean up user input (spaces, tabs, newlines)
     * 
     * ALGORITHM:
     * 1. Find first non-whitespace character
     * 2. Find last non-whitespace character
     * 3. Extract substring between them
     * 
     * WHITESPACE: " \t\n\r" (space, tab, newline, carriage return)
     * 
     * EXAMPLES:
     * "  hello  " → "hello"
     * "\n\tworld\r\n" → "world"
     * "   " → "" (all whitespace returns empty)
     * 
     * USAGE:
     * Clean user input before validation
     * Often: trim THEN validate
     */
    static std::string trimWhitespace(const std::string& str) {
        // Find first non-whitespace character
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            return "";  // All whitespace, return empty
        }
        
        // Find last non-whitespace character
        size_t end = str.find_last_not_of(" \t\n\r");
        
        // Extract substring (inclusive of end)
        return str.substr(start, end - start + 1);
    }

    /**
     * @brief Checks if string contains only safe characters
     * @param str String to check
     * @return bool - true if safe (alphanumeric, _, -)
     * 
     * PURPOSE: Validate IDs, tokens, safe identifiers
     * 
     * ALLOWED: Letters, digits, underscore, hyphen
     * 
     * PATTERN: "^[a-zA-Z0-9_-]+$"
     * 
     * USAGE:
     * - Validate IDs before database queries
     * - Check tokens before processing
     * - Ensure filenames are safe
     */
    static bool isSafeString(const std::string& str) {
        std::regex safePattern("^[a-zA-Z0-9_-]+$");
        return std::regex_match(str, safePattern);
    }

    /**
     * @brief Validates session ID format
     * @param sessionId Session ID to validate
     * @return bool - true if valid format
     * 
     * PURPOSE: Verify session ID before lookup
     * 
     * RULES:
     * - Length: Exactly 64 characters (32 bytes hex)
     * - Pattern: Hexadecimal only (0-9, a-f, A-F)
     * 
     * WHY 64 CHARS:
     * Session IDs are 256-bit SHA-256 hashes = 32 bytes = 64 hex chars
     * (Note: Session.h generates 32-char IDs, this validates 64-char)
     * 
     * SECURITY:
     * Reject malformed IDs before database lookup
     * Prevents injection attacks via session ID parameter
     * 
     * NOTE: Mismatch with Session.h (32 vs 64)
     * Should be updated to match actual session ID length
     */
    static bool isValidSessionId(const std::string& sessionId) {
        // Session IDs should be hex strings of certain length
        if (sessionId.length() != 64) {
            return false;
        }
        
        // Hexadecimal pattern only
        std::regex hexPattern("^[a-fA-F0-9]+$");
        return std::regex_match(sessionId, hexPattern);
    }

    /**
     * @brief Truncates string to maximum length
     * @param str String to truncate
     * @param maxLength Maximum allowed length
     * @return std::string - Truncated string
     * 
     * PURPOSE: Enforce length limits while preserving content
     * 
     * BEHAVIOR:
     * - If str.length() <= maxLength: Return unchanged
     * - If str.length() > maxLength: Return first maxLength chars
     * 
     * USAGE:
     * Enforce limits without rejecting valid data
     * Alternative to validation failure
     * 
     * CONSIDERATION:
     * May cut in middle of word or sentence
     * Better UX: Add "..." ellipsis to indicate truncation
     */
    static std::string truncate(const std::string& str, size_t maxLength) {
        if (str.length() <= maxLength) {
            return str;
        }
        return str.substr(0, maxLength);
    }

    // ========================================================================
    // INJECTION PREVENTION METHODS
    // ========================================================================
    
    /**
     * @brief Escapes string for safe use in database queries
     * @param input String to escape
     * @return std::string - Escaped string
     * 
     * PURPOSE: Prevents SQL/NoSQL injection attacks
     * 
     * SQL INJECTION EXAMPLE:
     * User input: "; DROP TABLE users; --"
     * Without escaping: Query becomes: "SELECT * FROM users WHERE name=''; DROP TABLE users; --'"
     * With escaping: Single quotes escaped, query safe
     * 
     * ESCAPING RULES:
     * - Backslash (\) → \\\\ (escape backslash first!)
     * - Single quote (') → \\'
     * - Double quote (") → \\"
     * 
     * ORDER MATTERS:
     * Must escape backslash FIRST
     * Otherwise: "'" → "\'" → "\\'" (double escaping)
     * 
     * WHY ESCAPE BACKSLASH:
     * Backslash is escape character in many systems
     * Without escaping: Can bypass quote escaping
     * 
     * ALGORITHM:
     * 1. Find and replace all backslashes
     * 2. Find and replace all single quotes
     * 3. Find and replace all double quotes
     * 
     * BETTER APPROACH:
     * Use parameterized queries/prepared statements (eliminates injection)
     * Current approach: Defense-in-depth backup
     * 
     * USAGE:
     * When building raw queries (should be rare with modern drivers)
     * 
     * EFFICIENCY: O(n*m) where n = string length, m = number of special chars
     * Each replace shifts remaining string (could be optimized)
     */
    static std::string escapeForQuery(const std::string& input) {
        std::string result = input;
        
        /*
         * Escape backslashes FIRST (must come before quote escaping)
         */
        size_t pos = 0;
        while ((pos = result.find('\\', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\\\");  // Backslash → double backslash
            pos += 2;  // Skip past inserted characters
        }
        
        // Escape single quotes (SQL string delimiter)
        pos = 0;
        while ((pos = result.find('\'', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\'");  // ' → \'
            pos += 2;
        }
        
        // Escape double quotes (alternate SQL string delimiter)
        pos = 0;
        while ((pos = result.find('"', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\\"");  // " → \"
            pos += 2;
        }
        
        return result;
    }
};

// ============================================================================
// END OF INPUTVALIDATOR CLASS
// ============================================================================

#endif // INPUTVALIDATOR_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * SECURITY PHILOSOPHY:
 * - Validate early: Check at API boundary before processing
 * - Whitelist approach: Define what IS allowed, reject everything else
 * - Multiple layers: Validation + sanitization + database protection
 * - Fail securely: Reject invalid input, don't try to "fix" it
 * 
 * VALIDATION STRATEGY:
 * 
 * STEP 1: FORMAT VALIDATION
 * Check if input matches expected pattern (regex, character class)
 * Reject if format wrong (return error to user)
 * 
 * STEP 2: SANITIZATION
 * Escape dangerous characters for safe display
 * Apply before storing or rendering
 * 
 * STEP 3: CONTEXT-SPECIFIC ENCODING
 * - HTML context: HTML entity encoding (sanitize)
 * - SQL context: Parameterized queries (escapeForQuery as backup)
 * - JavaScript context: JSON encoding
 * - URL context: URL encoding
 * 
 * REGEX PATTERNS:
 * - Anchored: Always use ^ and $ for exact matching
 * - Character classes: [a-z] specifies allowed characters
 * - Quantifiers: + (one or more), * (zero or more), {n,m} (range)
 * - Escaping: Use \\ for literal special characters
 * 
 * COMMON VALIDATION PITFALLS:
 * 
 * INCOMPLETE REGEX:
 * Bad: "[a-z]+" allows "abc!!!" (no anchors)
 * Good: "^[a-z]+$" only allows "abc" (anchored)
 * 
 * BLACKLIST APPROACH:
 * Bad: Block "<script>" (attackers use "<SCRIPT>", "<scr<script>ipt>")
 * Good: Allow only [a-zA-Z0-9] (whitelist)
 * 
 * CLIENT-SIDE ONLY:
 * Never rely on JavaScript validation alone
 * Always validate on server (client can be bypassed)
 * 
 * LENGTH LIMITS:
 * Always enforce both min and max
 * Prevents empty strings and DoS via huge inputs
 * 
 * PERFORMANCE CONSIDERATIONS:
 * - Regex compilation: Expensive (but done once with static patterns)
 * - String operations: O(n) for most validators
 * - Early rejection: Length checks before expensive regex
 * - Reserve space: Use result.reserve() for building strings
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: Valid inputs, invalid inputs, edge cases
 * - Security tests: Injection attempts, XSS payloads
 * - Edge cases: Empty, very long, special characters
 * - Boundary values: Min length - 1, max length + 1
 * - Fuzzing: Random input to find crashes/errors
 * 
 * POTENTIAL IMPROVEMENTS:
 * - Internationalization: Support Unicode (usernames, display names)
 * - Password strength meter: Entropy calculation, common password check
 * - Custom error messages: Return WHY validation failed
 * - Configurable limits: Make length limits configurable
 * - Rich validation: Return validation object with details, not just bool
 * - Async validation: Check username uniqueness during validation
 * 
 * COMPARISON TO LIBRARIES:
 * 
 * VALIDATOR.JS (JavaScript):
 * - More validators: Credit cards, URLs, IPs, etc.
 * - Sanitization: Trim, escape, normalize
 * - Our approach: Similar but C++ and domain-specific
 * 
 * OWASP VALIDATION REGEX REPOSITORY:
 * - Comprehensive patterns for common use cases
 * - Battle-tested against attacks
 * - Our patterns inspired by OWASP recommendations
 * 
 * EXTENSIBILITY:
 * - Phone number validation
 * - URL validation
 * - Credit card validation
 * - Date/time validation
 * - File upload validation (MIME types, extensions)
 * - JSON schema validation
 * - Custom business rules
 ******************************************************************************/

