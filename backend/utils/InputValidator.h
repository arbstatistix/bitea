#ifndef INPUTVALIDATOR_H
#define INPUTVALIDATOR_H

#include <string>
#include <regex>
#include <algorithm>
#include <cctype>

class InputValidator {
public:
    // Sanitize string input by removing/escaping dangerous characters
    static std::string sanitize(const std::string& input) {
        std::string result;
        result.reserve(input.length());
        
        for (char c : input) {
            // Remove control characters except newline and tab
            if (c < 32 && c != '\n' && c != '\t') {
                continue;
            }
            
            // Escape special characters that could be used for injection
            switch (c) {
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '&':
                    result += "&amp;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                case '\'':
                    result += "&#39;";
                    break;
                default:
                    result += c;
            }
        }
        
        return result;
    }

    // Validate username (alphanumeric + underscore, 3-20 chars)
    static bool isValidUsername(const std::string& username) {
        if (username.length() < 3 || username.length() > 20) {
            return false;
        }
        
        std::regex usernamePattern("^[a-zA-Z0-9_]+$");
        return std::regex_match(username, usernamePattern);
    }

    // Validate email format
    static bool isValidEmail(const std::string& email) {
        if (email.length() > 254) {
            return false;
        }
        
        std::regex emailPattern(
            "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
        );
        return std::regex_match(email, emailPattern);
    }

    // Validate password strength (min 8 chars, at least one letter and one number)
    static bool isValidPassword(const std::string& password) {
        if (password.length() < 8 || password.length() > 128) {
            return false;
        }
        
        bool hasLetter = false;
        bool hasDigit = false;
        
        for (char c : password) {
            if (std::isalpha(c)) hasLetter = true;
            if (std::isdigit(c)) hasDigit = true;
        }
        
        return hasLetter && hasDigit;
    }

    // Validate post content (not empty, max length)
    static bool isValidPostContent(const std::string& content) {
        if (content.empty() || content.length() > 5000) {
            return false;
        }
        
        // Check if content is all whitespace
        return std::any_of(content.begin(), content.end(), 
                          [](unsigned char c) { return !std::isspace(c); });
    }

    // Validate bio (max length)
    static bool isValidBio(const std::string& bio) {
        return bio.length() <= 500;
    }

    // Validate display name (max length, printable characters)
    static bool isValidDisplayName(const std::string& name) {
        if (name.empty() || name.length() > 50) {
            return false;
        }
        
        return std::all_of(name.begin(), name.end(), 
                          [](unsigned char c) { return std::isprint(c); });
    }

    // Remove extra whitespace from string
    static std::string trimWhitespace(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            return "";
        }
        
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    // Check if string contains only safe characters (for IDs, etc.)
    static bool isSafeString(const std::string& str) {
        std::regex safePattern("^[a-zA-Z0-9_-]+$");
        return std::regex_match(str, safePattern);
    }

    // Validate session ID format
    static bool isValidSessionId(const std::string& sessionId) {
        // Session IDs should be hex strings of certain length
        if (sessionId.length() != 64) {
            return false;
        }
        
        std::regex hexPattern("^[a-fA-F0-9]+$");
        return std::regex_match(sessionId, hexPattern);
    }

    // Limit string length
    static std::string truncate(const std::string& str, size_t maxLength) {
        if (str.length() <= maxLength) {
            return str;
        }
        return str.substr(0, maxLength);
    }

    // SQL/NoSQL injection prevention (for raw queries)
    static std::string escapeForQuery(const std::string& input) {
        std::string result = input;
        
        // Escape backslashes first
        size_t pos = 0;
        while ((pos = result.find('\\', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\\\");
            pos += 2;
        }
        
        // Escape quotes
        pos = 0;
        while ((pos = result.find('\'', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\'");
            pos += 2;
        }
        
        pos = 0;
        while ((pos = result.find('"', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\\"");
            pos += 2;
        }
        
        return result;
    }
};

#endif // INPUTVALIDATOR_H

