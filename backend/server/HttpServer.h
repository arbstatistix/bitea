#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <string>
#include <map>
#include <sstream>
#include <functional>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <regex>

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS
};

struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::string body;
    std::map<std::string, std::string> query;
};

struct HttpResponse {
    int statusCode;
    std::map<std::string, std::string> headers;
    std::string body;

    HttpResponse() : statusCode(200) {
        headers["Content-Type"] = "application/json";
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "HTTP/1.1 " << statusCode << " " << getStatusText() << "\r\n";
        
        for (const auto& header : headers) {
            ss << header.first << ": " << header.second << "\r\n";
        }
        
        ss << "Content-Length: " << body.length() << "\r\n";
        ss << "\r\n";
        ss << body;
        
        return ss.str();
    }

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

    void json(const std::string& jsonBody) {
        body = jsonBody;
        headers["Content-Type"] = "application/json";
    }

    void html(const std::string& htmlBody) {
        body = htmlBody;
        headers["Content-Type"] = "text/html";
    }

    void text(const std::string& textBody) {
        body = textBody;
        headers["Content-Type"] = "text/plain";
    }
};

using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

class HttpServer {
private:
    int port;
    int serverSocket;
    bool running;
    
    struct Route {
        std::string pattern;
        HttpMethod method;
        RouteHandler handler;
        std::regex regex;
        std::vector<std::string> paramNames;
    };
    
    std::vector<Route> routes;

    HttpMethod parseMethod(const std::string& method) {
        if (method == "GET") return HttpMethod::GET;
        if (method == "POST") return HttpMethod::POST;
        if (method == "PUT") return HttpMethod::PUT;
        if (method == "DELETE") return HttpMethod::DELETE;
        if (method == "PATCH") return HttpMethod::PATCH;
        if (method == "OPTIONS") return HttpMethod::OPTIONS;
        return HttpMethod::GET;
    }

    HttpRequest parseRequest(const std::string& rawRequest) {
        HttpRequest request;
        std::istringstream stream(rawRequest);
        std::string line;
        
        // Parse request line
        if (std::getline(stream, line)) {
            std::istringstream lineStream(line);
            std::string method, path, version;
            lineStream >> method >> path >> version;
            
            request.method = parseMethod(method);
            
            // Parse query string
            size_t queryPos = path.find('?');
            if (queryPos != std::string::npos) {
                std::string queryString = path.substr(queryPos + 1);
                request.path = path.substr(0, queryPos);
                parseQueryString(queryString, request.query);
            } else {
                request.path = path;
            }
        }
        
        // Parse headers
        while (std::getline(stream, line) && line != "\r" && !line.empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 2);
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                request.headers[key] = value;
            }
        }
        
        // Parse body
        std::stringstream bodyStream;
        while (std::getline(stream, line)) {
            bodyStream << line;
        }
        request.body = bodyStream.str();
        
        return request;
    }

    void parseQueryString(const std::string& query, std::map<std::string, std::string>& result) {
        std::istringstream stream(query);
        std::string pair;
        
        while (std::getline(stream, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);
                std::string value = pair.substr(eqPos + 1);
                result[key] = value;
            }
        }
    }

    void handleClient(int clientSocket) {
        char buffer[65536] = {0};
        ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0) {
            std::string rawRequest(buffer, bytesRead);
            HttpRequest request = parseRequest(rawRequest);
            HttpResponse response;
            
            // Handle OPTIONS for CORS
            if (request.method == HttpMethod::OPTIONS) {
                response.statusCode = 200;
                response.body = "";
            } else {
                // Find matching route
                bool handled = false;
                for (const auto& route : routes) {
                    if (route.method == request.method) {
                        std::smatch matches;
                        if (std::regex_match(request.path, matches, route.regex)) {
                            // Extract path parameters
                            for (size_t i = 0; i < route.paramNames.size(); i++) {
                                request.params[route.paramNames[i]] = matches[i + 1].str();
                            }
                            
                            route.handler(request, response);
                            handled = true;
                            break;
                        }
                    }
                }
                
                if (!handled) {
                    response.statusCode = 404;
                    response.json("{\"error\":\"Route not found\"}");
                }
            }
            
            std::string responseStr = response.toString();
            write(clientSocket, responseStr.c_str(), responseStr.length());
        }
        
        close(clientSocket);
    }

    std::string routeToRegex(const std::string& pattern, std::vector<std::string>& paramNames) {
        std::string regexPattern = pattern;
        std::regex paramRegex(":([a-zA-Z_][a-zA-Z0-9_]*)");
        
        std::string::const_iterator searchStart(regexPattern.cbegin());
        std::smatch match;
        
        std::vector<std::string> params;
        while (std::regex_search(searchStart, regexPattern.cend(), match, paramRegex)) {
            params.push_back(match[1].str());
            searchStart = match.suffix().first;
        }
        
        paramNames = params;
        regexPattern = std::regex_replace(regexPattern, paramRegex, "([^/]+)");
        
        return "^" + regexPattern + "$";
    }

public:
    HttpServer(int port = 3000) : port(port), serverSocket(-1), running(false) {}

    ~HttpServer() {
        stop();
    }

    void get(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::GET, handler);
    }

    void post(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::POST, handler);
    }

    void put(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::PUT, handler);
    }

    void del(const std::string& pattern, RouteHandler handler) {
        addRoute(pattern, HttpMethod::DELETE, handler);
    }

    void addRoute(const std::string& pattern, HttpMethod method, RouteHandler handler) {
        Route route;
        route.pattern = pattern;
        route.method = method;
        route.handler = handler;
        route.regex = std::regex(routeToRegex(pattern, route.paramNames));
        routes.push_back(route);
    }

    bool start() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind to port " << port << std::endl;
            close(serverSocket);
            return false;
        }

        if (listen(serverSocket, 10) < 0) {
            std::cerr << "Failed to listen on port " << port << std::endl;
            close(serverSocket);
            return false;
        }

        running = true;
        std::cout << "Server started on http://localhost:" << port << std::endl;

        while (running) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket < 0) {
                if (running) {
                    std::cerr << "Failed to accept connection" << std::endl;
                }
                continue;
            }

            // Handle each request in a separate thread
            std::thread(&HttpServer::handleClient, this, clientSocket).detach();
        }

        return true;
    }

    void stop() {
        running = false;
        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
    }
};

#endif // HTTPSERVER_H

