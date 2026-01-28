#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <map>
#include <functional>
#include <thread>

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::string body;
};

struct HttpResponse {
    int statusCode;
    std::map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse() : statusCode(200) {
        headers["Content-Type"] = "text/html; charset=utf-8";
    }
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class WebServer {
public:
    WebServer(int port);
    ~WebServer();
    
    void start();
    void stop();
    
    void registerRoute(const std::string& method, 
                      const std::string& path,
                      RouteHandler handler);
    
    void serveStatic(const std::string& webRoot);
    
private:
    void run();
    void handleClient(int clientSocket);
    HttpRequest parseRequest(const std::string& requestStr);
    std::string buildResponse(const HttpResponse& response);
    
    bool matchRoute(const std::string& pattern, const std::string& path,
                   std::map<std::string, std::string>& params);
    HttpResponse serveStaticFile(const std::string& path);
    void parseQueryString(const std::string& query, 
                         std::map<std::string, std::string>& params);
    std::string urlDecode(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string getFileExtension(const std::string& path);
    std::string getMimeType(const std::string& ext);
    bool requireAuth(const HttpRequest& req, HttpResponse& res);
    std::string getAuthToken(const HttpRequest& req);
    
    int port_;
    int serverSocket_;
    bool running_;
    std::thread serverThread_;
    std::string webRoot_;
    
    std::map<std::string, std::map<std::string, RouteHandler>> routes_;
    
    void registerDefaultRoutes();
};

#endif // WEB_SERVER_H
