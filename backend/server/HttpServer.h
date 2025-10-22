/*******************************************************************************
 * HTTPSERVER.H - Lightweight HTTP Web Server
 * 
 * PURPOSE:
 * This header defines a minimal HTTP/1.1 web server implementation for the
 * Bitea platform. Handles REST API requests, routing, and multi-threaded
 * request processing without external dependencies.
 * 
 * SERVER ARCHITECTURE:
 * - Socket-based: Raw TCP sockets (POSIX sockets API)
 * - Multi-threaded: Each request handled in separate thread (std::thread)
 * - RESTful routing: Pattern-based URL matching with path parameters
 * - CORS enabled: Cross-Origin Resource Sharing for frontend access
 * 
 * INTEGRATION WITH OTHER COMPONENTS:
 * - main.cpp: BiteaApp class sets up routes and starts server
 * - All models: Server exposes CRUD operations via REST API
 * - Database: Server coordinates database and blockchain operations
 * 
 * HTTP FEATURES IMPLEMENTED:
 * - Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS
 * - Request parsing: Headers, body, query parameters, path parameters
 * - Response building: Status codes, headers, body
 * - CORS: Pre-flight OPTIONS requests handled
 * - Content types: JSON, HTML, text
 * 
 * ROUTING SYSTEM:
 * Pattern-based with parameter extraction:
 * - "/api/posts" → exact match
 * - "/api/posts/:id" → matches "/api/posts/123", extracts id="123"
 * - "/api/users/:username/posts" → nested parameters
 * 
 * THREADING MODEL:
 * - Main thread: Accept loop (server.start())
 * - Worker threads: One per request (std::thread::detach())
 * - Fire-and-forget: Threads clean up automatically
 * - No thread pool: Simple but creates many threads under load
 * 
 * LIMITATIONS (Educational/MVP):
 * - No HTTPS (use reverse proxy like nginx for production)
 * - No request size limits (vulnerable to large payload attacks)
 * - No rate limiting (vulnerable to DoS)
 * - No keep-alive connections (HTTP/1.0 style)
 * - No compression (gzip, brotli)
 * - No streaming responses
 * - Simple JSON parsing (use real library for production)
 * 
 * PRODUCTION RECOMMENDATIONS:
 * - Use established server: crow, beast, drogon, or oat++
 * - Add HTTPS with OpenSSL/TLS
 * - Implement thread pool (limit concurrent threads)
 * - Add request timeouts
 * - Implement rate limiting
 * - Use proper JSON library (nlohmann/json, RapidJSON)
 * - Add logging framework
 * - Implement graceful shutdown
 * 
 * REFERENCES:
 * - HTTP/1.1 Specification: RFC 7230-7235
 * - RESTful API Design: "REST API Design Rulebook" by Mark Masse
 * - POSIX Sockets: https://man7.org/linux/man-pages/man7/socket.7.html
 * - CORS: https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS
 ******************************************************************************/

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

// ============================================================================
// STANDARD LIBRARY INCLUDES
// ============================================================================

#include <string>       // std::string - URLs, headers, body
#include <map>          // std::map - header/parameter storage (key-value)
#include <sstream>      // std::stringstream - request/response building
#include <functional>   // std::function - route handler callbacks
#include <vector>       // std::vector - route storage, dynamic arrays
#include <iostream>     // std::cout, std::cerr - logging
#include <thread>       // std::thread - multi-threaded request handling
#include <regex>        // std::regex - URL pattern matching

// ============================================================================
// POSIX SOCKET INCLUDES (Network Programming)
// ============================================================================

#include <sys/socket.h> // socket(), bind(), listen(), accept() - TCP server
#include <netinet/in.h> // sockaddr_in structure - IPv4 addressing
#include <unistd.h>     // read(), write(), close() - I/O operations
#include <cstring>      // memset() - memory operations

// ============================================================================
// HTTP METHOD ENUMERATION
// ============================================================================

/**
 * @enum HttpMethod
 * @brief HTTP request methods (verbs) for RESTful operations
 * 
 * REST CONVENTIONS:
 * - GET: Retrieve resources (idempotent, safe)
 * - POST: Create resources (not idempotent)
 * - PUT: Update/replace resources (idempotent)
 * - DELETE: Remove resources (idempotent)
 * - PATCH: Partial update (not idempotent)
 * - OPTIONS: Pre-flight CORS requests, capability discovery
 * 
 * WHY enum class:
 * - Type-safe: Cannot accidentally use integers
 * - Scoped: HttpMethod::GET not just GET
 * - Clear intent: Method types explicit
 * 
 * USAGE IN ROUTING:
 * server.get("/api/posts", handler);    // GET method
 * server.post("/api/posts", handler);   // POST method
 * server.del("/api/posts/:id", handler); // DELETE method
 * 
 * IDEMPOTENCE:
 * GET, PUT, DELETE: Multiple identical requests = same effect
 * POST, PATCH: Multiple identical requests = different effects
 * 
 * SAFETY:
 * GET: Read-only, no side effects (safe)
 * POST/PUT/DELETE: Modify state (unsafe)
 */
enum class HttpMethod {
    GET,        // Retrieve resource (safe, idempotent)
    POST,       // Create resource (unsafe, not idempotent)
    PUT,        // Update resource (unsafe, idempotent)
    DELETE,     // Delete resource (unsafe, idempotent)
    PATCH,      // Partial update (unsafe, not idempotent)
    OPTIONS     // CORS pre-flight, metadata (safe, idempotent)
};

// ============================================================================
// HTTP REQUEST STRUCTURE
// ============================================================================

/**
 * @struct HttpRequest
 * @brief Represents parsed HTTP request from client
 * 
 * PURPOSE: Packages all request information for route handlers
 * 
 * LIFECYCLE:
 * 1. Raw HTTP text received from socket
 * 2. parseRequest() extracts components into HttpRequest
 * 3. Route handler receives HttpRequest reference
 * 4. Handler reads method, path, headers, body, etc.
 * 
 * DESIGN: Simple struct (public members) for easy access
 */
struct HttpRequest {
    /**
     * @brief HTTP method (GET, POST, etc.)
     * PURPOSE: Determines operation type (CRUD mapping)
     */
    HttpMethod method;
    
    /**
     * @brief Request path without query string
     * EXAMPLE: "/api/posts/123" (query string removed)
     * USAGE: Matched against route patterns
     */
    std::string path;
    
    /**
     * @brief HTTP headers (key-value pairs)
     * @type std::map<std::string, std::string>
     * 
     * COMMON HEADERS:
     * - "Content-Type": "application/json"
     * - "Authorization": "Bearer <sessionId>"
     * - "User-Agent": Browser/client identifier
     * 
     * WHY std::map:
     * - Key-value lookup: O(log n)
     * - Unique keys: Each header appears once
     * - Sorted: Consistent iteration order
     */
    std::map<std::string, std::string> headers;
    
    /**
     * @brief Path parameters extracted from URL patterns
     * @type std::map<std::string, std::string>
     * 
     * EXAMPLE:
     * Route pattern: "/api/posts/:id"
     * Actual request: "/api/posts/123"
     * Result: params["id"] = "123"
     * 
     * USAGE: Access dynamic URL segments in handlers
     * std::string postId = req.params.at("id");
     */
    std::map<std::string, std::string> params;
    
    /**
     * @brief Request body content
     * @type std::string
     * 
     * CONTENT:
     * - POST/PUT/PATCH: Usually JSON payload
     * - GET/DELETE: Typically empty
     * 
     * EXAMPLE POST body:
     * {"username":"alice","password":"secret"}
     * 
     * PARSING: Handler extracts values (currently manual, should use JSON library)
     */
    std::string body;
    
    /**
     * @brief Query string parameters
     * @type std::map<std::string, std::string>
     * 
     * EXAMPLE:
     * URL: "/api/posts?page=2&limit=10"
     * Result: query["page"] = "2", query["limit"] = "10"
     * 
     * USAGE: Pagination, filtering, sorting
     * Common patterns: ?page=X, ?sort=date, ?filter=active
     */
    std::map<std::string, std::string> query;
};

// ============================================================================
// HTTP RESPONSE STRUCTURE
// ============================================================================

/**
 * @struct HttpResponse
 * @brief Represents HTTP response to send to client
 * 
 * PURPOSE: Packages response data for transmission
 * 
 * LIFECYCLE:
 * 1. Route handler receives HttpResponse reference
 * 2. Handler sets statusCode, body, headers
 * 3. toString() converts to raw HTTP format
 * 4. Raw text sent over socket to client
 * 
 * DEFAULT CONFIGURATION:
 * - Status: 200 OK
 * - Content-Type: application/json
 * - CORS headers: Allow all origins (development-friendly)
 */
struct HttpResponse {
    /**
     * @brief HTTP status code
     * @type int
     * 
     * COMMON CODES:
     * - 200: OK (success)
     * - 201: Created (resource created)
     * - 400: Bad Request (client error)
     * - 401: Unauthorized (authentication required)
     * - 404: Not Found (resource doesn't exist)
     * - 500: Internal Server Error (server error)
     * 
     * CATEGORIES:
     * - 2xx: Success
     * - 3xx: Redirection
     * - 4xx: Client error
     * - 5xx: Server error
     * 
     * DEFAULT: 200 (OK)
     */
    int statusCode;
    
    /**
     * @brief Response headers (key-value pairs)
     * @type std::map<std::string, std::string>
     * 
     * AUTO-SET HEADERS:
     * - Content-Type: application/json (default)
     * - Content-Length: Calculated automatically
     * - CORS headers: Allow cross-origin requests
     * 
     * WHY std::map: Same reasons as request headers
     */
    std::map<std::string, std::string> headers;
    
    /**
     * @brief Response body content
     * @type std::string
     * 
     * CONTENT:
     * Typically JSON for API responses
     * Can be HTML, text, or other formats
     * 
     * SET VIA:
     * - response.json("{...}")  // JSON with Content-Type
     * - response.html("<html>") // HTML with Content-Type
     * - response.text("...")    // Plain text
     */
    std::string body;

    /**
     * @brief Constructor - sets defaults and CORS headers
     * 
     * PURPOSE: Every response starts with safe defaults
     * 
     * CORS (Cross-Origin Resource Sharing):
     * Allows frontend (localhost:8080) to call API (localhost:3000)
     * Without CORS, browsers block cross-origin requests (security)
     * 
     * CORS HEADERS EXPLAINED:
     * - Access-Control-Allow-Origin: "*"
     *   Allows requests from any domain (development mode)
     *   Production: Restrict to specific frontend domain
     * 
     * - Access-Control-Allow-Methods: "GET, POST, PUT, DELETE, OPTIONS"
     *   Which HTTP methods are allowed
     * 
     * - Access-Control-Allow-Headers: "Content-Type, Authorization"
     *   Which request headers are allowed
     * 
     * SECURITY NOTE:
     * Wildcard "*" origin is convenient but insecure for production
     * Should restrict to specific trusted domains
     */
    HttpResponse() : statusCode(200) {
        headers["Content-Type"] = "application/json";  // Default to JSON API
        headers["Access-Control-Allow-Origin"] = "*";  // CORS: Allow all origins
        headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    }

    /**
     * @brief Converts response object to raw HTTP/1.1 format
     * @return std::string - Raw HTTP response ready for socket transmission
     * 
     * PURPOSE: Serializes response into wire protocol format
     * 
     * HTTP/1.1 RESPONSE FORMAT:
     * HTTP/1.1 200 OK\r\n
     * Content-Type: application/json\r\n
     * Content-Length: 42\r\n
     * \r\n
     * {"message":"hello"}
     * 
     * COMPONENTS:
     * 1. Status line: "HTTP/1.1 {code} {text}"
     * 2. Headers: "Key: Value" (each on separate line)
     * 3. Content-Length: Auto-calculated from body size
     * 4. Blank line: "\r\n" separates headers from body
     * 5. Body: Actual response content
     * 
     * LINE ENDINGS:
     * HTTP spec requires \r\n (CRLF), not just \n
     * \r = carriage return, \n = line feed
     * 
     * WHY const: Read-only serialization
     */
    std::string toString() const {
        std::stringstream ss;
        // Status line: HTTP version, code, status text
        ss << "HTTP/1.1 " << statusCode << " " << getStatusText() << "\r\n";
        
        // Headers: Each header as "Key: Value"
        for (const auto& header : headers) {
            ss << header.first << ": " << header.second << "\r\n";
        }
        
        // Content-Length header (auto-calculated)
        ss << "Content-Length: " << body.length() << "\r\n";
        
        // Blank line separates headers from body (HTTP spec requirement)
        ss << "\r\n";
        
        // Response body
        ss << body;
        
        return ss.str();
    }

    /**
     * @brief Returns human-readable status text for code
     * @return std::string - Status text (e.g., "OK", "Not Found")
     * 
     * PURPOSE: Converts numeric code to readable text per HTTP spec
     * 
     * STANDARD TEXTS:
     * RFC 7231 defines standard reason phrases
     * Not strictly required but improves readability
     */
    std::string getStatusText() const {
        switch (statusCode) {
            case 200: return "OK";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }

    /**
     * @brief Sets response body as JSON and Content-Type header
     * @param jsonBody JSON string to send
     * 
     * PURPOSE: Convenience method for JSON API responses
     * 
     * USAGE:
     * response.json("{\"message\":\"success\"}");
     * 
     * ALTERNATIVE: Manually set body and header
     */
    void json(const std::string& jsonBody) {
        body = jsonBody;
        headers["Content-Type"] = "application/json";
    }

    /**
     * @brief Sets response body as HTML and Content-Type header
     * @param htmlBody HTML string to send
     * USAGE: For serving web pages (not typical for API)
     */
    void html(const std::string& htmlBody) {
        body = htmlBody;
        headers["Content-Type"] = "text/html";
    }

    /**
     * @brief Sets response body as plain text and Content-Type header
     * @param textBody Plain text to send
     * USAGE: For simple text responses
     */
    void text(const std::string& textBody) {
        body = textBody;
        headers["Content-Type"] = "text/plain";
    }
};

// ============================================================================
// ROUTE HANDLER TYPE DEFINITION
// ============================================================================

/**
 * @typedef RouteHandler
 * @brief Function signature for route handler callbacks
 * 
 * TYPE: std::function<void(const HttpRequest&, HttpResponse&)>
 * 
 * PURPOSE: Defines contract for route handler functions
 * 
 * PARAMETERS:
 * - const HttpRequest&: Read request data (method, path, body, etc.)
 * - HttpResponse&: Modify response (set status, body, headers)
 * 
 * USAGE EXAMPLE:
 * server.get("/api/users", [](const HttpRequest& req, HttpResponse& res) {
 *     // Handler code here
 *     res.json("{\"users\":[...]}");
 * });
 * 
 * WHY std::function:
 * - Flexible: Accepts lambda functions, function pointers, functors
 * - Type-safe: Compiler enforces signature
 * - Capture support: Lambdas can capture local variables
 * 
 * ALTERNATIVE:
 * Could use function pointer: void (*)(const HttpRequest&, HttpResponse&)
 * std::function more flexible (supports lambdas with captures)
 */
using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// ============================================================================
// HTTP SERVER CLASS
// ============================================================================

/**
 * @class HttpServer
 * @brief Lightweight HTTP/1.1 server with RESTful routing
 * 
 * PURPOSE: Provides web server functionality for API endpoints
 * 
 * ARCHITECTURE:
 * - TCP socket server (POSIX sockets API)
 * - Multi-threaded request handling
 * - Pattern-based routing with parameter extraction
 * - CORS support for frontend integration
 * 
 * WORKFLOW:
 * 1. Constructor: Initialize with port number
 * 2. Route registration: server.get(), server.post(), etc.
 * 3. Start: Creates socket, binds, listens, accepts connections
 * 4. Request handling: Each request in separate thread
 * 5. Stop: Closes socket, stops accepting new requests
 * 
 * THREADING:
 * Main thread runs accept loop
 * Each connection handled in detached thread
 * No limit on thread count (production should use thread pool)
 */
class HttpServer {
private:
    // ========================================================================
    // PRIVATE MEMBER VARIABLES (Server State)
    // ========================================================================
    
    /**
     * @brief TCP port number for server to listen on
     * @type int (1-65535, typically 3000-8000 for development)
     * 
     * PURPOSE: Network endpoint for incoming connections
     * 
     * COMMON PORTS:
     * - 80: HTTP (requires root/admin privileges)
     * - 443: HTTPS (requires root/admin privileges)
     * - 3000: Development default (no privileges needed)
     * - 8080: Alternative development port
     * 
     * RANGE:
     * - 1-1023: System ports (require elevated privileges)
     * - 1024-49151: Registered ports
     * - 49152-65535: Dynamic/private ports
     * 
     * DEFAULT: 3000 (set in constructor)
     */
    int port;
    
    /**
     * @brief File descriptor for server socket
     * @type int (POSIX file descriptor, typically small positive integer)
     * 
     * PURPOSE: Handle for TCP listening socket
     * 
     * LIFECYCLE:
     * - Uninitialized: -1 (invalid fd)
     * - After socket(): Positive integer (valid fd)
     * - After close(): -1 again
     * 
     * SOCKET OPERATIONS:
     * - socket(): Create socket, returns fd
     * - bind(): Attach socket to port
     * - listen(): Mark as passive socket
     * - accept(): Accept connections, returns client fd
     * - close(): Clean up socket resources
     * 
     * WHY int: POSIX standard for file descriptors
     */
    int serverSocket;
    
    /**
     * @brief Flag indicating if server is running
     * @type bool
     * 
     * PURPOSE: Controls accept loop, enables graceful shutdown
     * 
     * STATES:
     * - false: Server not started or stopped
     * - true: Server running, accepting connections
     * 
     * USAGE:
     * while (running) {
     *     // Accept connections
     * }
     * 
     * SHUTDOWN:
     * stop() sets running=false, causing accept loop to exit
     */
    bool running;
    
    /**
     * @struct Route
     * @brief Represents a registered API endpoint
     * 
     * PURPOSE: Stores routing information for URL pattern matching
     * 
     * COMPONENTS:
     * - pattern: Original pattern string ("/api/posts/:id")
     * - method: HTTP method (GET, POST, etc.)
     * - handler: Callback function to execute
     * - regex: Compiled regex for matching
     * - paramNames: Names of path parameters (["id"])
     */
    struct Route {
        std::string pattern;              // Original pattern "/api/posts/:id"
        HttpMethod method;                // HTTP method for this route
        RouteHandler handler;             // Function to call when matched
        std::regex regex;                 // Compiled regex for fast matching
        std::vector<std::string> paramNames;  // ["id", "username", ...]
    };
    
    /**
     * @brief Vector of registered routes
     * @type std::vector<Route>
     * 
     * PURPOSE: Stores all API endpoints
     * 
     * MATCHING:
     * Iterates through routes in order, returns first match
     * Order matters! Specific routes before general ones
     * 
     * TYPICAL SIZE: 10-50 routes for API server
     */
    std::vector<Route> routes;

    // ========================================================================
    // PRIVATE HELPER METHODS (Request Processing)
    // ========================================================================
    
    /**
     * @brief Converts HTTP method string to enum
     * @param method Method string ("GET", "POST", etc.)
     * @return HttpMethod - Corresponding enum value
     * 
     * PURPOSE: Parses method from raw HTTP request
     * 
     * DEFAULT: Returns GET if unrecognized (safe fallback)
     * 
     * CASE SENSITIVE: HTTP spec requires uppercase method names
     */
    HttpMethod parseMethod(const std::string& method) {
        if (method == "GET") return HttpMethod::GET;
        if (method == "POST") return HttpMethod::POST;
        if (method == "PUT") return HttpMethod::PUT;
        if (method == "DELETE") return HttpMethod::DELETE;
        if (method == "PATCH") return HttpMethod::PATCH;
        if (method == "OPTIONS") return HttpMethod::OPTIONS;
        return HttpMethod::GET;  // Default fallback
    }

    /**
     * @brief Parses raw HTTP request into HttpRequest structure
     * @param rawRequest Raw HTTP text from socket
     * @return HttpRequest - Structured request data
     * 
     * PURPOSE: Converts wire format to usable object
     * 
     * RAW HTTP REQUEST FORMAT:
     * GET /api/posts?page=1 HTTP/1.1\r\n
     * Host: localhost:3000\r\n
     * Content-Type: application/json\r\n
     * \r\n
     * {"data":"body"}
     * 
     * PARSING STAGES:
     * 1. Request line: "METHOD PATH VERSION"
     * 2. Headers: "Key: Value" pairs
     * 3. Blank line: Separator
     * 4. Body: Remaining content
     * 
     * QUERY STRING HANDLING:
     * Path contains "?": Split into path + query
     * "/api/posts?page=1" → path="/api/posts", query["page"]="1"
     * 
     * HEADER PARSING:
     * "Content-Type: application/json"
     * Split at colon, trim spaces
     * 
     * BODY PARSING:
     * Everything after blank line = body
     * Not decoded/parsed (handler's responsibility)
     * 
     * EDGE CASES:
     * - Missing components: Empty strings/defaults
     * - Malformed requests: Best-effort parsing
     * - Production: Should validate and reject malformed requests
     */
    HttpRequest parseRequest(const std::string& rawRequest) {
        HttpRequest request;
        std::istringstream stream(rawRequest);
        std::string line;
        
        // Parse request line: "GET /api/posts HTTP/1.1"
        if (std::getline(stream, line)) {
            std::istringstream lineStream(line);
            std::string method, path, version;
            lineStream >> method >> path >> version;  // Extract three tokens
            
            request.method = parseMethod(method);
            
            // Parse query string from path
            size_t queryPos = path.find('?');
            if (queryPos != std::string::npos) {
                // Path contains query string: "/api/posts?page=1"
                std::string queryString = path.substr(queryPos + 1);  // "page=1"
                request.path = path.substr(0, queryPos);              // "/api/posts"
                parseQueryString(queryString, request.query);
            } else {
                // No query string: "/api/posts"
                request.path = path;
            }
        }
        
        // Parse headers: "Key: Value" pairs until blank line
        while (std::getline(stream, line) && line != "\r" && !line.empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);           // Before colon
                std::string value = line.substr(colonPos + 2);        // After ": " (skip space)
                
                // Remove trailing \r if present (CRLF line endings)
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                request.headers[key] = value;
            }
        }
        
        // Parse body: Everything after blank line
        std::stringstream bodyStream;
        while (std::getline(stream, line)) {
            bodyStream << line;
        }
        request.body = bodyStream.str();
        
        return request;
    }

    /**
     * @brief Parses URL query string into key-value map
     * @param query Query string (without leading '?')
     * @param result Output map to populate
     * 
     * PURPOSE: Extract query parameters from URL
     * 
     * QUERY STRING FORMAT:
     * "key1=value1&key2=value2"
     * 
     * ALGORITHM:
     * 1. Split on '&' to get pairs
     * 2. Split each pair on '=' to get key and value
     * 3. Store in result map
     * 
     * EXAMPLE:
     * Input: "page=2&limit=10&sort=date"
     * Output: {{"page","2"}, {"limit","10"}, {"sort","date"}}
     * 
     * URL ENCODING:
     * Should URL-decode values (spaces as +, %20, etc.)
     * Current: No decoding (simplified)
     * Production: Use proper URL decoding
     */
    void parseQueryString(const std::string& query, std::map<std::string, std::string>& result) {
        std::istringstream stream(query);
        std::string pair;
        
        // Split query on '&' delimiter
        while (std::getline(stream, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);      // Before '='
                std::string value = pair.substr(eqPos + 1);  // After '='
                result[key] = value;
            }
        }
    }

    /**
     * @brief Handles incoming client connection (runs in separate thread)
     * @param clientSocket File descriptor for client connection
     * 
     * PURPOSE: Process one HTTP request/response cycle
     * 
     * THREADING:
     * This method runs in detached thread (one per request)
     * Created by: std::thread(&HttpServer::handleClient, this, clientSocket).detach()
     * 
     * WORKFLOW:
     * 1. Read raw HTTP request from socket (blocking I/O)
     * 2. Parse request into HttpRequest structure
     * 3. Create default HttpResponse
     * 4. Route to appropriate handler OR return 404
     * 5. Serialize response and send back
     * 6. Close client socket (cleanup)
     * 
     * BUFFER SIZE:
     * 65536 bytes = 64KB maximum request size
     * Larger requests truncated (should be validated)
     * 
     * CORS OPTIONS HANDLING:
     * Browsers send OPTIONS request before actual request (pre-flight)
     * Server responds with CORS headers to allow cross-origin
     * 
     * ROUTE MATCHING:
     * 1. Check method matches (GET vs POST, etc.)
     * 2. Check path matches regex pattern
     * 3. Extract path parameters from URL
     * 4. Call handler with request and response
     * 5. If no match, return 404
     * 
     * SOCKET OPERATIONS:
     * - read(): Blocks until data available or connection closed
     * - write(): Sends response back to client
     * - close(): Closes connection (HTTP/1.0 style, no keep-alive)
     * 
     * RESOURCE CLEANUP:
     * Socket always closed at end (RAII would be better)
     * Thread terminates after close (detached, cleans up automatically)
     */
    void handleClient(int clientSocket) {
        // Buffer for reading HTTP request (64KB max)
        char buffer[65536] = {0};
        
        // Read request from socket (blocking call)
        ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0) {
            // Convert buffer to string for parsing
            std::string rawRequest(buffer, bytesRead);
            
            // Parse raw HTTP into structured format
            HttpRequest request = parseRequest(rawRequest);
            
            // Create response object with defaults
            HttpResponse response;
            
            // Handle CORS pre-flight requests
            if (request.method == HttpMethod::OPTIONS) {
                response.statusCode = 200;
                response.body = "";  // Empty body for OPTIONS
            } else {
                // Find matching route and execute handler
                bool handled = false;
                
                for (const auto& route : routes) {
                    // Check if HTTP method matches
                    if (route.method == request.method) {
                        std::smatch matches;
                        
                        // Check if path matches regex pattern
                        if (std::regex_match(request.path, matches, route.regex)) {
                            // Extract path parameters (:id, :username, etc.)
                            for (size_t i = 0; i < route.paramNames.size(); i++) {
                                // matches[0] = full match, matches[1+] = captured groups
                                request.params[route.paramNames[i]] = matches[i + 1].str();
                            }
                            
                            // Call route handler
                            route.handler(request, response);
                            handled = true;
                            break;  // Stop at first match
                        }
                    }
                }
                
                // No route matched - return 404
                if (!handled) {
                    response.statusCode = 404;
                    response.json("{\"error\":\"Route not found\"}");
                }
            }
            
            // Serialize response to HTTP format
            std::string responseStr = response.toString();
            
            // Send response back to client
            write(clientSocket, responseStr.c_str(), responseStr.length());
        }
        
        // Close connection (HTTP/1.0 style, no keep-alive)
        close(clientSocket);
    }

    /**
     * @brief Converts route pattern to regex for matching
     * @param pattern Route pattern with :param placeholders
     * @param paramNames Output vector for parameter names
     * @return std::string - Regex pattern for matching
     * 
     * PURPOSE: Enables dynamic URL segments (/api/posts/:id)
     * 
     * PATTERN SYNTAX:
     * :paramName → Matches any non-slash characters
     * 
     * EXAMPLES:
     * "/api/posts/:id"
     * → Regex: "^/api/posts/([^/]+)$"
     * → Matches: "/api/posts/123", "/api/posts/abc"
     * → Captures: id="123" or id="abc"
     * → paramNames: ["id"]
     * 
     * "/api/users/:username/posts/:postId"
     * → Regex: "^/api/users/([^/]+)/posts/([^/]+)$"
     * → paramNames: ["username", "postId"]
     * 
     * ALGORITHM:
     * 1. Find all :paramName occurrences
     * 2. Extract parameter names ("id", "username")
     * 3. Replace :paramName with ([^/]+) capturing group
     * 4. Add ^...$ anchors for exact matching
     * 
     * REGEX COMPONENTS:
     * - ^: Start of string
     * - $: End of string
     * - ([^/]+): Capture one or more non-slash characters
     * - [^/]: Character class excluding /
     * - +: One or more (path params can't be empty)
     * 
     * WHY NOT WILDCARDS:
     * [^/]+ ensures params don't cross path segments
     * "/posts/:id" matches "/posts/123" not "/posts/123/comments"
     */
    std::string routeToRegex(const std::string& pattern, std::vector<std::string>& paramNames) {
        std::string regexPattern = pattern;
        
        // Regex to find :paramName patterns
        std::regex paramRegex(":([a-zA-Z_][a-zA-Z0-9_]*)");
        
        std::string::const_iterator searchStart(regexPattern.cbegin());
        std::smatch match;
        
        // Extract all parameter names
        std::vector<std::string> params;
        while (std::regex_search(searchStart, regexPattern.cend(), match, paramRegex)) {
            params.push_back(match[1].str());  // Parameter name without ':'
            searchStart = match.suffix().first;
        }
        
        // Store parameter names for later extraction
        paramNames = params;
        
        // Replace :paramName with ([^/]+) capturing group
        regexPattern = std::regex_replace(regexPattern, paramRegex, "([^/]+)");
        
        // Add anchors for exact matching
        return "^" + regexPattern + "$";
    }

    // ========================================================================
    // PUBLIC INTERFACE (Server Control and Routing)
    // ========================================================================

public:
    /**
     * @brief Constructs HTTP server instance
     * @param port TCP port to listen on (default: 3000)
     * 
     * PURPOSE: Initialize server with configuration
     * 
     * INITIALIZATION:
     * - port: Set from parameter
     * - serverSocket: -1 (not created yet)
     * - running: false (not started)
     * - routes: Empty vector (populate via get/post/etc.)
     * 
     * USAGE:
     * HttpServer server(3000);  // Custom port
     * HttpServer server;        // Default port 3000
     */
    HttpServer(int port = 3000) : port(port), serverSocket(-1), running(false) {}

    /**
     * @brief Destructor - ensures clean shutdown
     * 
     * PURPOSE: RAII cleanup of server resources
     * 
     * CLEANUP:
     * Calls stop() to close socket if still open
     * Prevents resource leaks
     * 
     * AUTOMATIC:
     * Called when HttpServer object goes out of scope
     */
    ~HttpServer() {
        stop();
    }

    /**
     * @brief Registers GET route handler
     * @param pattern URL pattern ("/api/posts/:id")
     * @param handler Function to call when route matches
     * 
     * PURPOSE: Convenience method for GET endpoints
     * 
     * REST CONVENTION:
     * GET for retrieving/reading resources
     * 
     * USAGE:
     * server.get("/api/posts", [](const HttpRequest& req, HttpResponse& res) {
     *     // Handler code
     * });
     */
    void get(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::GET, handler);
    }

    /**
     * @brief Registers POST route handler
     * @param pattern URL pattern
     * @param handler Function to call
     * 
     * REST CONVENTION: POST for creating resources
     */
    void post(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::POST, handler);
    }

    /**
     * @brief Registers PUT route handler
     * @param pattern URL pattern
     * @param handler Function to call
     * 
     * REST CONVENTION: PUT for updating resources
     */
    void put(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::PUT, handler);
    }

    /**
     * @brief Registers DELETE route handler
     * @param pattern URL pattern
     * @param handler Function to call
     * 
     * REST CONVENTION: DELETE for removing resources
     * 
     * NOTE: Named 'del' not 'delete' (delete is C++ keyword)
     */
    void del(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::DELETE, handler);
    }

    /**
     * @brief Registers route with any HTTP method
     * @param pattern URL pattern with optional :params
     * @param method HTTP method enum
     * @param handler Callback function
     * 
     * PURPOSE: Core routing registration logic
     * 
     * WORKFLOW:
     * 1. Create Route object
     * 2. Convert pattern to regex
     * 3. Extract parameter names
     * 4. Compile regex for fast matching
     * 5. Add to routes vector
     * 
     * CALLED BY:
     * get(), post(), put(), del() convenience methods
     * 
     * PATTERN EXAMPLES:
     * - "/api/posts" - exact match
     * - "/api/posts/:id" - with parameter
     * - "/api/users/:username/posts/:postId" - multiple params
     */
    void addRoute(const std::string& pattern, HttpMethod method, RouteHandler handler) {
        Route route;
        route.pattern = pattern;
        route.method = method;
        route.handler = handler;
        
        // Convert pattern to regex and extract param names
        route.regex = std::regex(routeToRegex(pattern, route.paramNames));
        
        // Add to route list
        routes.push_back(route);
    }

    /**
     * @brief Starts HTTP server (blocking call)
     * @return bool - true if started successfully, false on error
     * 
     * PURPOSE: Initialize socket and begin accepting connections
     * 
     * SOCKET SETUP SEQUENCE:
     * 1. socket(): Create TCP socket
     * 2. setsockopt(): Configure socket options
     * 3. bind(): Attach to port
     * 4. listen(): Mark as listening socket
     * 5. accept loop: Wait for and handle connections
     * 
     * POSIX SOCKET API CALLS:
     * 
     * socket(AF_INET, SOCK_STREAM, 0):
     * - AF_INET: IPv4 addressing
     * - SOCK_STREAM: TCP (reliable, ordered, connection-based)
     * - Returns: Socket file descriptor or -1 on error
     * 
     * setsockopt(SOL_SOCKET, SO_REUSEADDR):
     * - Allows immediate port reuse after server restart
     * - Without this: "Address already in use" errors
     * - Useful for development (frequent restarts)
     * 
     * bind():
     * - Associates socket with specific port and IP
     * - INADDR_ANY: Listen on all network interfaces
     * - htons(): Convert port to network byte order (big-endian)
     * 
     * listen(backlog=10):
     * - Marks socket as passive (waiting for connections)
     * - Backlog: Max queued connections (10 = reasonable)
     * 
     * accept():
     * - BLOCKING: Waits until client connects
     * - Returns: New socket for client communication
     * - clientAddr: Filled with client's IP address
     * 
     * THREADING MODEL:
     * Main thread runs accept loop
     * Each accepted connection spawns detached thread
     * Detached thread: Runs independently, cleans up automatically
     * 
     * BLOCKING:
     * This method blocks until server stopped
     * Run in background thread or separate process for async operation
     * 
     * ERROR HANDLING:
     * Returns false on socket/bind/listen errors
     * Logs errors to stderr
     * Cleans up partial state (closes socket on failure)
     * 
     * SHUTDOWN:
     * stop() sets running=false → accept loop exits → start() returns
     */
    bool start() {
        // Create TCP socket (IPv4, stream-based)
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Enable port reuse (allows immediate restart)
        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Configure server address
        struct sockaddr_in address;
        address.sin_family = AF_INET;              // IPv4
        address.sin_addr.s_addr = INADDR_ANY;      // All interfaces (0.0.0.0)
        address.sin_port = htons(port);            // Port (host to network byte order)

        // Bind socket to port
        if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind to port " << port << std::endl;
            close(serverSocket);
            return false;
        }

        // Start listening for connections (backlog = 10)
        if (listen(serverSocket, 10) < 0) {
            std::cerr << "Failed to listen on port " << port << std::endl;
            close(serverSocket);
            return false;
        }

        // Mark server as running
        running = true;
        std::cout << "Server started on http://localhost:" << port << std::endl;

        // Accept loop (blocks until server stopped)
        while (running) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            // Wait for client connection (BLOCKING)
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket < 0) {
                // Error accepting (may be due to shutdown)
                if (running) {
                    std::cerr << "Failed to accept connection" << std::endl;
                }
                continue;  // Try again
            }

            // Handle request in separate detached thread
            // Detached: Thread runs independently, no join() needed
            // this: Pass server object pointer for member access
            // clientSocket: Pass to thread for processing
            std::thread(&HttpServer::handleClient, this, clientSocket).detach();
        }

        return true;
    }

    /**
     * @brief Stops HTTP server
     * 
     * PURPOSE: Graceful shutdown of server
     * 
     * SHUTDOWN SEQUENCE:
     * 1. Set running = false (stops accept loop)
     * 2. Close server socket (wakes up blocked accept())
     * 3. Reset socket to -1 (invalid state)
     * 
     * THREAD SAFETY:
     * - Main thread exits accept loop
     * - Detached worker threads continue until complete
     * - New connections rejected after close()
     * 
     * IDEMPOTENT:
     * Safe to call multiple times (checks if socket valid)
     * 
     * CALLED BY:
     * - Destructor (automatic cleanup)
     * - Explicit shutdown (server.stop())
     * - Signal handlers (graceful shutdown)
     */
    void stop() {
        running = false;  // Signal accept loop to exit
        
        if (serverSocket >= 0) {
            close(serverSocket);    // Close socket, release port
            serverSocket = -1;      // Mark as invalid
        }
    }
};

// ============================================================================
// END OF HTTPSERVER CLASS
// ============================================================================

#endif // HTTPSERVER_H

/*******************************************************************************
 * IMPLEMENTATION NOTES:
 * 
 * ARCHITECTURE DECISIONS:
 * - Raw sockets: Educational, no external dependencies
 * - Multi-threaded: Simple concurrency (one thread per request)
 * - No thread pool: Could create many threads under load
 * - HTTP/1.0 style: No keep-alive connections
 * - CORS enabled: Frontend integration without hassle
 * 
 * SOCKET PROGRAMMING:
 * - POSIX sockets API: Standard Unix network programming
 * - TCP: Reliable, ordered, connection-oriented
 * - IPv4 only: Could extend to IPv6 (AF_INET6)
 * - Blocking I/O: Simple but not scalable (could use epoll/kqueue)
 * 
 * ROUTING SYSTEM:
 * - Regex-based: Flexible pattern matching
 * - Parameter extraction: :id becomes captured group
 * - First-match: Order of route registration matters
 * - Express.js inspired: Similar API design
 * 
 * THREAD SAFETY:
 * - No shared state in request handling: Thread-safe
 * - Routes vector: Read-only after setup (safe)
 * - Handlers share blockchain/database: Those provide sync
 * 
 * PERFORMANCE CHARACTERISTICS:
 * - Thread-per-request: Simple but resource-intensive
 * - Typical: ~1MB per thread (stack)
 * - 1000 concurrent requests = ~1GB RAM
 * - Production: Use thread pool or event-driven (epoll, io_uring)
 * 
 * SECURITY LIMITATIONS:
 * - No HTTPS: Traffic unencrypted (use reverse proxy)
 * - No rate limiting: Vulnerable to DoS
 * - No request size limit: Memory exhaustion possible
 * - No input validation: Handler responsibility
 * - No timeout: Slow clients can hold threads
 * 
 * COMPARISON TO PRODUCTION SERVERS:
 * 
 * NGINX/APACHE:
 * - Pro: Battle-tested, HTTPS, load balancing, caching
 * - Con: Separate process, configuration complexity
 * - Use case: Reverse proxy in front of our server
 * 
 * NODE.JS/EXPRESS:
 * - Similar: Routing API, middleware
 * - Different: Event-driven, not multi-threaded
 * 
 * C++ FRAMEWORKS (Crow, Beast, Drogon):
 * - Pro: Production-ready, async I/O, better performance
 * - Con: External dependency, more complex
 * - Our choice: Educational simplicity over production features
 * 
 * POTENTIAL IMPROVEMENTS:
 * 
 * SECURITY:
 * - Add HTTPS/TLS support (OpenSSL)
 * - Implement rate limiting (token bucket, sliding window)
 * - Add request size limits
 * - Add request timeouts
 * - Implement CSRF protection
 * - Add security headers (CSP, X-Frame-Options)
 * 
 * PERFORMANCE:
 * - Thread pool (reuse threads, limit concurrency)
 * - Event-driven I/O (epoll on Linux, kqueue on BSD/Mac)
 * - Keep-alive connections (HTTP/1.1 persistent)
 * - Response compression (gzip, brotli)
 * - Connection pooling
 * 
 * FEATURES:
 * - WebSocket support (real-time communication)
 * - Middleware system (logging, auth, validation)
 * - Static file serving
 * - Template rendering
 * - Session management
 * - File upload handling (multipart/form-data)
 * - Proper JSON library (nlohmann/json)
 * 
 * RELIABILITY:
 * - Graceful shutdown (finish pending requests)
 * - Error recovery (restart on crash)
 * - Health check endpoints
 * - Metrics/monitoring (Prometheus)
 * - Structured logging
 * 
 * SCALABILITY:
 * - Horizontal: Multiple server instances behind load balancer
 * - Vertical: Async I/O to handle more connections per server
 * - Caching: Redis for frequently accessed data
 * - CDN: Static assets served from edge locations
 * 
 * TESTING RECOMMENDATIONS:
 * - Unit tests: Route matching, request parsing, response building
 * - Integration tests: Full HTTP request/response cycle
 * - Load tests: Concurrent requests, thread creation limits
 * - Security tests: Injection attacks, DoS resistance
 * - Performance tests: Request latency, throughput
 ******************************************************************************/

