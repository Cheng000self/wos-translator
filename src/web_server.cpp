#include "web_server.h"
#include "logger.h"
#include "task_queue.h"
#include "config_manager.h"
#include "html_parser.h"
#include "exporter.h"
#include "nlohmann/json.hpp"
#ifdef EMBED_RESOURCES
#include "embedded_resources.h"
#endif

// 跨平台头文件
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    typedef int socklen_t;
    #define platform_close_socket closesocket
    #define platform_read(fd, buf, len) recv(fd, (char*)(buf), len, 0)
    #define platform_write(fd, buf, len) send(fd, (const char*)(buf), (int)(len), 0)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define platform_close_socket close
    #define platform_read read
    #define platform_write write
#endif

#include <cstring>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <chrono>
#include <iomanip>

#ifndef _WIN32
#include <dirent.h>
#endif

using json = nlohmann::json;

WebServer::WebServer(int port) : port_(port), serverSocket_(-1), running_(false), webRoot_("web") {
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (running_) {
        return;
    }
    
    // 注册默认路由
    registerDefaultRoutes();
    
    running_ = true;
    serverThread_ = std::thread(&WebServer::run, this);
    Logger::getInstance().info("WebServer started on port " + std::to_string(port_));
}

void WebServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
#ifdef _WIN32
    if (serverSocket_ != INVALID_SOCKET) {
        platform_close_socket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
#else
    if (serverSocket_ >= 0) {
        platform_close_socket(serverSocket_);
        serverSocket_ = -1;
    }
#endif
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    Logger::getInstance().info("WebServer stopped");
}

void WebServer::registerRoute(const std::string& method, 
                              const std::string& path,
                              RouteHandler handler) {
    routes_[method][path] = handler;
}

void WebServer::serveStatic(const std::string& webRoot) {
    webRoot_ = webRoot;
}

void WebServer::run() {
    // 将在任务8中实现完整的HTTP服务器
    // 这里是简化的占位符
    
#ifdef _WIN32
    // Windows: 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::getInstance().error("Failed to initialize Winsock");
        return;
    }
#endif
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (serverSocket_ == INVALID_SOCKET) {
#else
    if (serverSocket_ < 0) {
#endif
        Logger::getInstance().error("Failed to create socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    
    int opt = 1;
#ifdef _WIN32
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(serverSocket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        Logger::getInstance().error("Failed to bind socket");
        platform_close_socket(serverSocket_);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    
    if (listen(serverSocket_, 10) < 0) {
        Logger::getInstance().error("Failed to listen on socket");
        platform_close_socket(serverSocket_);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    
    Logger::getInstance().info("Server listening on port " + std::to_string(port_));
    
    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (running_) {
                Logger::getInstance().warning("Failed to accept connection");
            }
            continue;
        }
        
        // 在新线程中处理客户端请求
        std::thread(&WebServer::handleClient, this, clientSocket).detach();
    }
}

void WebServer::handleClient(int clientSocket) {
    // 首先读取请求头（最多64KB应该足够）
    std::string headerData;
    char buffer[4096];
    size_t headerEndPos = std::string::npos;
    
    // 读取直到找到请求头结束标记 \r\n\r\n
    while (headerEndPos == std::string::npos) {
#ifdef _WIN32
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
#else
        ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer));
#endif
        if (bytesRead <= 0) {
            platform_close_socket(clientSocket);
            return;
        }
        headerData.append(buffer, bytesRead);
        headerEndPos = headerData.find("\r\n\r\n");
        
        // 防止无限循环，限制头部大小
        if (headerData.size() > 65536) {
            Logger::getInstance().error("Request header too large");
            platform_close_socket(clientSocket);
            return;
        }
    }
    
    // 分离头部和已读取的body部分
    std::string headers = headerData.substr(0, headerEndPos + 4);
    std::string partialBody = headerData.substr(headerEndPos + 4);
    
    // 解析Content-Length
    size_t contentLength = 0;
    size_t clPos = headers.find("Content-Length:");
    if (clPos == std::string::npos) {
        clPos = headers.find("content-length:");
    }
    if (clPos != std::string::npos) {
        size_t valueStart = clPos + 15; // "Content-Length:" 长度
        size_t valueEnd = headers.find("\r\n", valueStart);
        std::string clValue = headers.substr(valueStart, valueEnd - valueStart);
        // 去除空格
        clValue.erase(0, clValue.find_first_not_of(" \t"));
        clValue.erase(clValue.find_last_not_of(" \t\r\n") + 1);
        contentLength = std::stoul(clValue);
    }
    
    // 读取剩余的body
    std::string body = partialBody;
    while (body.size() < contentLength) {
#ifdef _WIN32
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
#else
        ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer));
#endif
        if (bytesRead <= 0) {
            break;
        }
        body.append(buffer, bytesRead);
    }
    
    // 构建完整请求字符串
    std::string requestStr = headers + body;
    HttpRequest request = parseRequest(requestStr);
    
    Logger::getInstance().info("Request: " + request.method + " " + request.path + " (body: " + std::to_string(request.body.size()) + " bytes)");
    
    HttpResponse response;
    
    // 尝试匹配路由
    bool routeFound = false;
    auto methodRoutes = routes_.find(request.method);
    if (methodRoutes != routes_.end()) {
        auto handler = methodRoutes->second.find(request.path);
        if (handler != methodRoutes->second.end()) {
            response = handler->second(request);
            routeFound = true;
        } else {
            // 尝试匹配带参数的路由（如 /api/tasks/:id）
            // 按模式长度降序排序，让更具体的路由优先匹配
            std::vector<std::pair<std::string, RouteHandler>> sortedRoutes(
                methodRoutes->second.begin(), methodRoutes->second.end());
            std::sort(sortedRoutes.begin(), sortedRoutes.end(),
                [](const auto& a, const auto& b) {
                    // 计算固定部分数量（非参数部分）
                    auto countFixed = [](const std::string& pattern) {
                        int count = 0;
                        size_t pos = 0;
                        while ((pos = pattern.find('/', pos)) != std::string::npos) {
                            count++;
                            pos++;
                        }
                        return count;
                    };
                    return countFixed(a.first) > countFixed(b.first);
                });
            
            for (const auto& route : sortedRoutes) {
                if (matchRoute(route.first, request.path, request.params)) {
                    response = route.second(request);
                    routeFound = true;
                    break;
                }
            }
        }
    }
    
    // 如果没有匹配的路由，尝试静态文件服务
    if (!routeFound) {
        response = serveStaticFile(request.path);
    }
    
    std::string responseStr = buildResponse(response);
#ifdef _WIN32
    send(clientSocket, responseStr.c_str(), (int)responseStr.length(), 0);
#else
    write(clientSocket, responseStr.c_str(), responseStr.length());
#endif
    
    platform_close_socket(clientSocket);
}

HttpRequest WebServer::parseRequest(const std::string& requestStr) {
    HttpRequest request;
    std::istringstream stream(requestStr);
    std::string line;
    
    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream lineStream(line);
        lineStream >> request.method >> request.path;
        
        // 解析查询参数
        size_t queryPos = request.path.find('?');
        if (queryPos != std::string::npos) {
            std::string query = request.path.substr(queryPos + 1);
            request.path = request.path.substr(0, queryPos);
            parseQueryString(query, request.params);
        }
    }
    
    // 解析请求头
    size_t contentLength = 0;
    while (std::getline(stream, line) && line != "\r") {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // 去除前后空格
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \r\n") + 1);
            request.headers[key] = value;
            
            // 记录Content-Length
            if (key == "Content-Length") {
                contentLength = std::stoul(value);
            }
        }
    }
    
    // 解析请求体 - 根据Content-Length读取
    if (contentLength > 0) {
        std::vector<char> buffer(contentLength);
        stream.read(buffer.data(), contentLength);
        request.body = std::string(buffer.begin(), buffer.begin() + stream.gcount());
    }
    
    return request;
}

std::string WebServer::buildResponse(const HttpResponse& response) {
    std::ostringstream oss;
    
    // 状态行
    oss << "HTTP/1.1 " << response.statusCode << " ";
    switch (response.statusCode) {
        case 200: oss << "OK"; break;
        case 201: oss << "Created"; break;
        case 400: oss << "Bad Request"; break;
        case 401: oss << "Unauthorized"; break;
        case 404: oss << "Not Found"; break;
        case 500: oss << "Internal Server Error"; break;
        default: oss << "Unknown"; break;
    }
    oss << "\r\n";
    
    // 响应头
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    // Content-Length
    oss << "Content-Length: " << response.body.length() << "\r\n";
    oss << "\r\n";
    
    // 响应体
    oss << response.body;
    
    return oss.str();
}

void WebServer::registerDefaultRoutes() {
    // 任务相关API
    registerRoute("POST", "/api/tasks", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            Logger::getInstance().info("Parsing task creation request");
            json reqBody = json::parse(req.body);
            
            bool translateTitle = reqBody.value("translateTitle", true);
            bool translateAbstract = reqBody.value("translateAbstract", true);
            std::string taskName = reqBody.value("taskName", "");
            
            // 模型配置
            TaskConfig config;
            config.taskName = taskName;
            config.translateTitle = translateTitle;
            config.translateAbstract = translateAbstract;
            
            if (reqBody.contains("modelConfig")) {
                auto mc = reqBody["modelConfig"];
                config.modelConfig.url = mc["url"];
                config.modelConfig.apiKey = mc["apiKey"];
                config.modelConfig.modelId = mc["modelId"];
                config.modelConfig.temperature = mc.value("temperature", 0.3f);
                config.modelConfig.systemPrompt = mc.value("systemPrompt", 
                    "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。");
                config.modelConfig.provider = mc.value("provider", "openai");
                config.modelConfig.enableThinking = mc.value("enableThinking", false);
                config.modelConfig.autoAppendPath = mc.value("autoAppendPath", true);
            }
            
            // 多模型配置
            if (reqBody.contains("modelConfigs") && reqBody["modelConfigs"].is_array()) {
                for (const auto& mc : reqBody["modelConfigs"]) {
                    ModelWithThreads mwt;
                    mwt.model.url = mc["url"];
                    mwt.model.apiKey = mc["apiKey"];
                    mwt.model.modelId = mc["modelId"];
                    mwt.model.name = mc.value("name", "");
                    mwt.model.temperature = mc.value("temperature", 0.3f);
                    mwt.model.systemPrompt = mc.value("systemPrompt", 
                        "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。");
                    mwt.model.provider = mc.value("provider", "openai");
                    mwt.model.enableThinking = mc.value("enableThinking", false);
                    mwt.model.autoAppendPath = mc.value("autoAppendPath", true);
                    mwt.threads = mc.value("threads", 1);
                    config.modelConfigs.push_back(mwt);
                }
                // 如果有多模型，也设置第一个为兼容的单模型
                if (!config.modelConfigs.empty()) {
                    config.modelConfig = config.modelConfigs[0].model;
                }
            }
            
            std::string taskId;
            
            // 检查是否为多文件上传
            if (reqBody.contains("fileNames") && reqBody.contains("htmlContents")) {
                // 多文件上传
                std::vector<std::string> fileNames = reqBody["fileNames"].get<std::vector<std::string>>();
                std::vector<std::string> htmlContents = reqBody["htmlContents"].get<std::vector<std::string>>();
                
                if (fileNames.empty() || fileNames.size() != htmlContents.size()) {
                    throw std::runtime_error("文件名和内容数量不匹配");
                }
                
                config.fileName = fileNames[0];  // 主文件名
                config.fileNames = fileNames;
                
                Logger::getInstance().info("Creating multi-file task with " + std::to_string(fileNames.size()) + " files");
                taskId = TaskQueue::getInstance().createTaskMultiFile(fileNames, htmlContents, config);
            } else {
                // 单文件上传
                std::string fileName = reqBody["fileName"];
                std::string htmlContent = reqBody["htmlContent"];
                
                config.fileName = fileName;
                
                Logger::getInstance().info("Creating task for file: " + fileName);
                taskId = TaskQueue::getInstance().createTask(fileName, htmlContent, config);
            }
            
            Logger::getInstance().info("Task created successfully: " + taskId);
            
            json response;
            response["success"] = true;
            response["taskId"] = taskId;
            res.body = response.dump();
            res.statusCode = 201;
            
        } catch (const std::exception& e) {
            Logger::getInstance().error("Task creation failed: " + std::string(e.what()));
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/tasks", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            auto tasks = TaskQueue::getInstance().listTasks();
            
            json response = json::array();
            for (const auto& task : tasks) {
                json taskJson;
                taskJson["taskId"] = task.taskId;
                taskJson["taskName"] = task.taskName;
                taskJson["fileName"] = task.fileName;
                taskJson["modelName"] = task.modelName;
                taskJson["modelCount"] = task.modelCount;
                taskJson["status"] = static_cast<int>(task.status);
                taskJson["totalCount"] = task.totalCount;
                taskJson["completedCount"] = task.completedCount;
                taskJson["failedCount"] = task.failedCount;
                taskJson["createdAt"] = task.createdAt;
                taskJson["updatedAt"] = task.updatedAt;
                response.push_back(taskJson);
            }
            
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/tasks/:id", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            auto task = TaskQueue::getInstance().getTaskInfo(taskId);
            
            json response;
            response["taskId"] = task.taskId;
            response["taskName"] = task.taskName;
            response["fileName"] = task.fileName;
            response["status"] = static_cast<int>(task.status);
            response["totalCount"] = task.totalCount;
            response["completedCount"] = task.completedCount;
            response["failedCount"] = task.failedCount;
            response["createdAt"] = task.createdAt;
            response["updatedAt"] = task.updatedAt;
            
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 404;
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/tasks/:id/literatures", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            auto literatures = TaskQueue::getInstance().getTaskLiteratures(taskId);
            
            json response = json::array();
            for (const auto& lit : literatures) {
                json litJson;
                litJson["index"] = lit.index;
                litJson["recordNumber"] = lit.recordNumber;
                litJson["totalRecords"] = lit.totalRecords;
                litJson["sourceFileName"] = lit.sourceFileName;
                litJson["sourceFileIndex"] = lit.sourceFileIndex;
                litJson["indexInFile"] = lit.indexInFile;
                litJson["originalTitle"] = lit.originalTitle;
                litJson["originalAbstract"] = lit.originalAbstract;
                litJson["translatedTitle"] = lit.translatedTitle;
                litJson["translatedAbstract"] = lit.translatedAbstract;
                litJson["authors"] = lit.authors;
                litJson["source"] = lit.source;
                litJson["volume"] = lit.volume;
                litJson["issue"] = lit.issue;
                litJson["pages"] = lit.pages;
                litJson["doi"] = lit.doi;
                litJson["earlyAccessDate"] = lit.earlyAccessDate;
                litJson["publishedDate"] = lit.publishedDate;
                litJson["accessionNumber"] = lit.accessionNumber;
                litJson["issn"] = lit.issn;
                litJson["eissn"] = lit.eissn;
                litJson["status"] = lit.status;
                litJson["errorMessage"] = lit.errorMessage;
                litJson["translatedByModel"] = lit.translatedByModel;
                response.push_back(litJson);
            }
            
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 404;
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/tasks/:id/literature/:index", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            int index = std::stoi(req.params.at("index"));
            
            auto literatures = TaskQueue::getInstance().getTaskLiteratures(taskId);
            
            for (const auto& lit : literatures) {
                if (lit.index == index) {
                    json litJson;
                    litJson["index"] = lit.index;
                    litJson["recordNumber"] = lit.recordNumber;
                    litJson["totalRecords"] = lit.totalRecords;
                    litJson["originalTitle"] = lit.originalTitle;
                    litJson["originalAbstract"] = lit.originalAbstract;
                    litJson["translatedTitle"] = lit.translatedTitle;
                    litJson["translatedAbstract"] = lit.translatedAbstract;
                    litJson["authors"] = lit.authors;
                    litJson["source"] = lit.source;
                    litJson["volume"] = lit.volume;
                    litJson["issue"] = lit.issue;
                    litJson["pages"] = lit.pages;
                    litJson["doi"] = lit.doi;
                    litJson["earlyAccessDate"] = lit.earlyAccessDate;
                    litJson["publishedDate"] = lit.publishedDate;
                    litJson["accessionNumber"] = lit.accessionNumber;
                    litJson["issn"] = lit.issn;
                    litJson["eissn"] = lit.eissn;
                    litJson["status"] = lit.status;
                    litJson["errorMessage"] = lit.errorMessage;
                    
                    res.body = litJson.dump();
                    return res;
                }
            }
            
            throw std::runtime_error("Literature not found");
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 404;
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/tasks/:id/original.html", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            std::string html = TaskQueue::getInstance().getOriginalHtml(taskId);
            res.body = html;
            
        } catch (const std::exception& e) {
            res.statusCode = 404;
            res.body = "404 Not Found";
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/tasks/:id/translated.html", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            std::string html = TaskQueue::getInstance().getTranslatedHtml(taskId);
            res.body = html;
            
        } catch (const std::exception& e) {
            res.statusCode = 404;
            res.body = "404 Not Found";
        }
        
        return res;
    });
    
    registerRoute("PUT", "/api/tasks/:id/pause", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            bool success = TaskQueue::getInstance().pauseTask(taskId);
            
            json response;
            response["success"] = success;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("PUT", "/api/tasks/:id/resume", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            bool success = TaskQueue::getInstance().resumeTask(taskId);
            
            json response;
            response["success"] = success;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("DELETE", "/api/tasks/:id", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            bool success = TaskQueue::getInstance().deleteTask(taskId);
            
            json response;
            response["success"] = success;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // 重试失败项API
    registerRoute("POST", "/api/tasks/:id/retry-failed", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            auto& storage = StorageManager::getInstance();
            TaskConfig config = storage.loadTaskConfig(taskId);
            
            // 只允许在暂停、完成、失败状态下重试
            if (config.status != "paused" && config.status != "completed" && config.status != "failed") {
                json error;
                error["success"] = false;
                error["error"] = "任务状态不允许重试";
                res.body = error.dump();
                res.statusCode = 400;
                return res;
            }
            
            // 可选：更新模型配置
            if (!req.body.empty()) {
                json reqBody = json::parse(req.body);
                if (reqBody.contains("modelConfigs") && reqBody["modelConfigs"].is_array()) {
                    config.modelConfigs.clear();
                    for (const auto& mj : reqBody["modelConfigs"]) {
                        ModelWithThreads mwt;
                        mwt.model.url = mj.value("url", "");
                        mwt.model.apiKey = mj.value("apiKey", "");
                        mwt.model.modelId = mj.value("modelId", "");
                        mwt.model.name = mj.value("name", "");
                        mwt.model.temperature = mj.value("temperature", 0.3f);
                        mwt.model.systemPrompt = mj.value("systemPrompt", "");
                        mwt.model.provider = mj.value("provider", "openai");
                        mwt.model.enableThinking = mj.value("enableThinking", false);
                        mwt.model.autoAppendPath = mj.value("autoAppendPath", true);
                        mwt.threads = mj.value("threads", 1);
                        config.modelConfigs.push_back(mwt);
                    }
                    // 同步单模型配置
                    if (!config.modelConfigs.empty()) {
                        config.modelConfig = config.modelConfigs[0].model;
                    }
                }
            }
            
            // 重置失败的文献为pending
            std::vector<int> indices = storage.loadIndexJson(taskId);
            int resetCount = 0;
            for (int idx : indices) {
                LiteratureData lit = storage.loadLiteratureData(taskId, idx);
                if (lit.status == "failed") {
                    lit.status = "pending";
                    lit.errorMessage = "";
                    lit.translatedTitle = "";
                    lit.translatedAbstract = "";
                    lit.translatedByModel = "";
                    storage.saveLiteratureData(taskId, idx, lit);
                    resetCount++;
                }
            }
            
            // 更新任务状态
            config.failedCount = 0;
            config.status = "pending";
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            config.updatedAt = oss.str();
            
            storage.saveTaskConfig(config);
            
            json response;
            response["success"] = true;
            response["resetCount"] = resetCount;
            res.body = response.dump();
            
            Logger::getInstance().info("Retry failed items for task: " + taskId + ", reset " + std::to_string(resetCount) + " items");
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // 重置整个任务API
    registerRoute("POST", "/api/tasks/:id/reset", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string taskId = req.params.at("id");
            auto& storage = StorageManager::getInstance();
            TaskConfig config = storage.loadTaskConfig(taskId);
            
            // 只允许在暂停、完成、失败状态下重置
            if (config.status != "paused" && config.status != "completed" && config.status != "failed") {
                json error;
                error["success"] = false;
                error["error"] = "任务状态不允许重置";
                res.body = error.dump();
                res.statusCode = 400;
                return res;
            }
            
            // 可选：更新模型配置
            if (!req.body.empty()) {
                json reqBody = json::parse(req.body);
                if (reqBody.contains("modelConfigs") && reqBody["modelConfigs"].is_array()) {
                    config.modelConfigs.clear();
                    for (const auto& mj : reqBody["modelConfigs"]) {
                        ModelWithThreads mwt;
                        mwt.model.url = mj.value("url", "");
                        mwt.model.apiKey = mj.value("apiKey", "");
                        mwt.model.modelId = mj.value("modelId", "");
                        mwt.model.name = mj.value("name", "");
                        mwt.model.temperature = mj.value("temperature", 0.3f);
                        mwt.model.systemPrompt = mj.value("systemPrompt", "");
                        mwt.model.provider = mj.value("provider", "openai");
                        mwt.model.enableThinking = mj.value("enableThinking", false);
                        mwt.model.autoAppendPath = mj.value("autoAppendPath", true);
                        mwt.threads = mj.value("threads", 1);
                        config.modelConfigs.push_back(mwt);
                    }
                    // 同步单模型配置
                    if (!config.modelConfigs.empty()) {
                        config.modelConfig = config.modelConfigs[0].model;
                    }
                }
            }
            
            // 重置所有文献为pending
            std::vector<int> indices = storage.loadIndexJson(taskId);
            for (int idx : indices) {
                LiteratureData lit = storage.loadLiteratureData(taskId, idx);
                lit.status = "pending";
                lit.errorMessage = "";
                lit.translatedTitle = "";
                lit.translatedAbstract = "";
                lit.translatedByModel = "";
                storage.saveLiteratureData(taskId, idx, lit);
            }
            
            // 更新任务状态
            config.completedCount = 0;
            config.failedCount = 0;
            config.status = "pending";
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            config.updatedAt = oss.str();
            
            storage.saveTaskConfig(config);
            
            json response;
            response["success"] = true;
            response["resetCount"] = (int)indices.size();
            res.body = response.dump();
            
            Logger::getInstance().info("Reset entire task: " + taskId + ", reset " + std::to_string(indices.size()) + " items");
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // 导出API
    registerRoute("POST", "/api/tasks/:id/export", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        
        try {
            std::string taskId = req.params.at("id");
            json reqBody = json::parse(req.body);
            
            std::string format = reqBody.value("format", "json");
            std::vector<int> recordNumbers;
            
            // 支持recordNumbers或indices参数
            if (reqBody.contains("recordNumbers")) {
                recordNumbers = reqBody["recordNumbers"].get<std::vector<int>>();
            } else if (reqBody.contains("indices")) {
                recordNumbers = reqBody["indices"].get<std::vector<int>>();
            }
            
            // 获取所有文献
            auto allLiteratures = TaskQueue::getInstance().getTaskLiteratures(taskId);
            
            // 筛选要导出的文献
            std::vector<LiteratureData> literaturesToExport;
            if (recordNumbers.empty()) {
                literaturesToExport = allLiteratures;
            } else {
                for (const auto& lit : allLiteratures) {
                    if (std::find(recordNumbers.begin(), recordNumbers.end(), lit.recordNumber) != recordNumbers.end()) {
                        literaturesToExport.push_back(lit);
                    }
                }
            }
            
            // 确定导出格式
            ExportFormat exportFormat;
            std::string contentType;
            std::string fileExt;
            
            if (format == "txt") {
                exportFormat = ExportFormat::TXT;
                contentType = "text/plain; charset=utf-8";
                fileExt = ".txt";
            } else if (format == "json") {
                exportFormat = ExportFormat::JSON;
                contentType = "application/json; charset=utf-8";
                fileExt = ".json";
            } else if (format == "csv") {
                exportFormat = ExportFormat::CSV;
                contentType = "text/csv; charset=utf-8";
                fileExt = ".csv";
            } else if (format == "html") {
                exportFormat = ExportFormat::HTML;
                contentType = "text/html; charset=utf-8";
                fileExt = ".html";
            } else {
                throw std::runtime_error("Unsupported format: " + format);
            }
            
            // 导出
            auto taskInfo = TaskQueue::getInstance().getTaskInfo(taskId);
            std::string content = Exporter::exportLiteratures(literaturesToExport, exportFormat, taskInfo.fileName);
            
            // 生成文件名
            std::string fileName = taskInfo.fileName;
            size_t dotPos = fileName.find_last_of('.');
            if (dotPos != std::string::npos) {
                fileName = fileName.substr(0, dotPos);
            }
            fileName += "_zh" + fileExt;
            
            res.headers["Content-Type"] = contentType;
            res.headers["Content-Disposition"] = "attachment; filename=\"" + fileName + "\"";
            res.body = content;
            
        } catch (const std::exception& e) {
            res.headers["Content-Type"] = "application/json; charset=utf-8";
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // HTML转JSON API
    registerRoute("POST", "/api/convert", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            json reqBody = json::parse(req.body);
            
            std::string fileName = reqBody["fileName"];
            std::string htmlContent = reqBody["htmlContent"];
            
            Logger::getInstance().info("Converting HTML to JSON: " + fileName);
            
            // 验证HTML格式
            HTMLParser parser;
            if (!parser.validate(htmlContent)) {
                Logger::getInstance().error("Invalid WoS HTML format");
                throw std::runtime_error("Invalid Web of Science HTML format");
            }
            
            // 解析HTML
            auto literatures = parser.parse(htmlContent);
            Logger::getInstance().info("Parsed " + std::to_string(literatures.size()) + " literatures");
            
            // 转换为JSON
            json result;
            result["success"] = true;
            result["fileName"] = fileName;
            result["totalRecords"] = literatures.size();
            result["convertedAt"] = std::time(nullptr);
            
            json litArray = json::array();
            for (const auto& lit : literatures) {
                json litJson;
                litJson["recordNumber"] = lit.recordNumber;
                litJson["title"] = lit.title;
                litJson["abstract"] = lit.abstract;
                litJson["authors"] = lit.authors;
                litJson["source"] = lit.source;
                litJson["volume"] = lit.volume;
                litJson["issue"] = lit.issue;
                litJson["pages"] = lit.pages;
                litJson["doi"] = lit.doi;
                litJson["earlyAccessDate"] = lit.earlyAccessDate;
                litJson["publishedDate"] = lit.publishedDate;
                litJson["accessionNumber"] = lit.accessionNumber;
                litJson["issn"] = lit.issn;
                litJson["eissn"] = lit.eissn;
                litArray.push_back(litJson);
            }
            result["literatures"] = litArray;
            
            res.body = result.dump(2);
            Logger::getInstance().info("Conversion successful");
            
        } catch (const std::exception& e) {
            Logger::getInstance().error("Conversion failed: " + std::string(e.what()));
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // 登出API
    registerRoute("POST", "/api/settings/logout", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        std::string token = getAuthToken(req);
        if (!token.empty()) {
            ConfigManager::getInstance().invalidateSession(token);
        }
        
        json response;
        response["success"] = true;
        res.body = response.dump();
        
        return res;
    });
    
    // 模型相关API
    registerRoute("POST", "/api/models/test", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            json reqBody = json::parse(req.body);
            
            ModelConfig config;
            config.url = reqBody["url"];
            config.apiKey = reqBody["apiKey"];
            config.modelId = reqBody["modelId"];
            config.temperature = reqBody.value("temperature", 0.3f);
            config.systemPrompt = reqBody.value("systemPrompt", 
                "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。");
            config.provider = reqBody.value("provider", "openai");
            config.enableThinking = reqBody.value("enableThinking", false);
            config.autoAppendPath = reqBody.value("autoAppendPath", true);
            
            Translator translator(config);
            auto testResult = translator.testConnection();
            
            json response;
            response["success"] = testResult.success;
            if (!testResult.success) {
                response["error"] = testResult.errorMessage;
                response["httpCode"] = testResult.httpCode;
            }
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("GET", "/api/models", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            auto models = ConfigManager::getInstance().loadModelConfigs();
            
            json response = json::array();
            for (const auto& model : models) {
                json modelJson;
                modelJson["id"] = model.id;
                modelJson["name"] = model.name;
                modelJson["url"] = model.url;
                modelJson["apiKey"] = model.apiKey;
                modelJson["modelId"] = model.modelId;
                // 使用double并四舍五入到2位小数
                double temp = std::round(static_cast<double>(model.temperature) * 100.0) / 100.0;
                modelJson["temperature"] = temp;
                modelJson["systemPrompt"] = model.systemPrompt;
                modelJson["provider"] = model.provider;
                modelJson["enableThinking"] = model.enableThinking;
                modelJson["autoAppendPath"] = model.autoAppendPath;
                response.push_back(modelJson);
            }
            
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    registerRoute("POST", "/api/models", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            json reqBody = json::parse(req.body);
            
            ModelConfig config;
            config.id = reqBody.value("id", "");
            config.name = reqBody["name"];
            config.url = reqBody["url"];
            config.apiKey = reqBody["apiKey"];
            config.modelId = reqBody["modelId"];
            
            // 四舍五入温度到2位小数
            float temp = reqBody.value("temperature", 0.3f);
            config.temperature = std::round(temp * 100.0f) / 100.0f;
            
            config.systemPrompt = reqBody.value("systemPrompt", 
                "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。");
            config.provider = reqBody.value("provider", "openai");
            config.enableThinking = reqBody.value("enableThinking", false);
            config.autoAppendPath = reqBody.value("autoAppendPath", true);
            
            // 如果没有提供ID，生成一个
            if (config.id.empty()) {
                config.id = "model_" + std::to_string(std::time(nullptr));
            }
            
            bool success = ConfigManager::getInstance().saveModelConfig(config);
            
            json response;
            response["success"] = success;
            response["id"] = config.id;
            res.body = response.dump();
            res.statusCode = 201;
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("PUT", "/api/models/:id", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string modelId = req.params.at("id");
            json reqBody = json::parse(req.body);
            
            ModelConfig config;
            config.id = modelId;
            config.name = reqBody["name"];
            config.url = reqBody["url"];
            config.apiKey = reqBody["apiKey"];
            config.modelId = reqBody["modelId"];
            
            // 四舍五入温度到2位小数
            float temp = reqBody.value("temperature", 0.3f);
            config.temperature = std::round(temp * 100.0f) / 100.0f;
            config.systemPrompt = reqBody.value("systemPrompt", 
                "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。");
            config.provider = reqBody.value("provider", "openai");
            config.enableThinking = reqBody.value("enableThinking", false);
            config.autoAppendPath = reqBody.value("autoAppendPath", true);
            
            bool success = ConfigManager::getInstance().updateModelConfig(modelId, config);
            
            json response;
            response["success"] = success;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("DELETE", "/api/models/:id", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            std::string modelId = req.params.at("id");
            bool success = ConfigManager::getInstance().deleteModelConfig(modelId);
            
            json response;
            response["success"] = success;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // 设置相关API
    registerRoute("GET", "/api/settings", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            auto config = ConfigManager::getInstance().loadSystemConfig();
            
            json response;
            response["maxUploadFiles"] = config.maxUploadFiles;
            response["maxTasks"] = config.maxTasks;
            response["maxConcurrentTasks"] = config.maxConcurrentTasks;
            response["maxConcurrentTasksPerModel"] = config.maxConcurrentTasksPerModel;
            response["maxTranslationThreads"] = config.maxTranslationThreads;
            response["maxModelsPerTask"] = config.maxModelsPerTask;
            response["maxRetries"] = config.maxRetries;
            response["consecutiveFailureThreshold"] = config.consecutiveFailureThreshold;
            response["serverPort"] = config.serverPort;
            response["logLevel"] = config.logLevel;
            // 日志管理配置
            response["logManageMode"] = static_cast<int>(config.logManageMode);
            response["logRetentionDays"] = config.logRetentionDays;
            response["logArchiveIntervalDays"] = config.logArchiveIntervalDays;
            // 不返回密码
            
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    registerRoute("PUT", "/api/settings", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        // 要求认证
        if (!requireAuth(req, res)) {
            return res;
        }
        
        try {
            json reqBody = json::parse(req.body);
            
            auto config = ConfigManager::getInstance().getSystemConfig();
            
            // 更新配置
            if (reqBody.contains("maxUploadFiles")) {
                config.maxUploadFiles = reqBody["maxUploadFiles"];
            }
            if (reqBody.contains("maxTasks")) {
                config.maxTasks = reqBody["maxTasks"];
            }
            if (reqBody.contains("maxConcurrentTasks")) {
                config.maxConcurrentTasks = reqBody["maxConcurrentTasks"];
            }
            if (reqBody.contains("maxConcurrentTasksPerModel")) {
                config.maxConcurrentTasksPerModel = reqBody["maxConcurrentTasksPerModel"];
            }
            if (reqBody.contains("maxTranslationThreads")) {
                config.maxTranslationThreads = reqBody["maxTranslationThreads"];
            }
            if (reqBody.contains("maxModelsPerTask")) {
                config.maxModelsPerTask = reqBody["maxModelsPerTask"];
            }
            if (reqBody.contains("maxRetries")) {
                config.maxRetries = reqBody["maxRetries"];
            }
            if (reqBody.contains("consecutiveFailureThreshold")) {
                config.consecutiveFailureThreshold = reqBody["consecutiveFailureThreshold"];
            }
            if (reqBody.contains("logLevel")) {
                config.logLevel = reqBody["logLevel"];
            }
            if (reqBody.contains("sessionTimeoutMinutes")) {
                config.sessionTimeoutMinutes = reqBody["sessionTimeoutMinutes"];
            }
            if (reqBody.contains("maxLoginAttempts")) {
                config.maxLoginAttempts = reqBody["maxLoginAttempts"];
            }
            if (reqBody.contains("lockoutDurationMinutes")) {
                config.lockoutDurationMinutes = reqBody["lockoutDurationMinutes"];
            }
            
            // 日志管理配置
            if (reqBody.contains("logManageMode")) {
                config.logManageMode = static_cast<LogManageMode>(reqBody["logManageMode"].get<int>());
                Logger::getInstance().setLogManageMode(config.logManageMode);
            }
            if (reqBody.contains("logRetentionDays")) {
                config.logRetentionDays = reqBody["logRetentionDays"];
                Logger::getInstance().setLogRetentionDays(config.logRetentionDays);
            }
            if (reqBody.contains("logArchiveIntervalDays")) {
                config.logArchiveIntervalDays = reqBody["logArchiveIntervalDays"];
                Logger::getInstance().setLogArchiveIntervalDays(config.logArchiveIntervalDays);
            }
            
            // 如果要修改密码
            if (reqBody.contains("oldPassword") && reqBody.contains("newPassword")) {
                std::string oldPassword = reqBody["oldPassword"];
                std::string newPassword = reqBody["newPassword"];
                
                if (!ConfigManager::getInstance().changePassword(oldPassword, newPassword)) {
                    json error;
                    error["success"] = false;
                    error["error"] = "Failed to change password. Check old password.";
                    res.body = error.dump();
                    res.statusCode = 400;
                    return res;
                }
            }
            
            bool success = ConfigManager::getInstance().saveSystemConfig(config);
            
            json response;
            response["success"] = success;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    registerRoute("POST", "/api/settings/verify", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            json reqBody = json::parse(req.body);
            std::string password = reqBody["password"];
            
            // 获取客户端标识（使用IP地址）
            std::string clientId = req.headers.count("X-Forwarded-For") > 0 
                ? req.headers.at("X-Forwarded-For") 
                : "unknown";
            
            // 检查是否被锁定
            if (ConfigManager::getInstance().isLockedOut(clientId)) {
                json error;
                error["success"] = false;
                error["error"] = "Too many failed attempts. Please try again later.";
                res.body = error.dump();
                res.statusCode = 429;
                return res;
            }
            
            // 检查登录尝试
            if (!ConfigManager::getInstance().checkLoginAttempt(clientId)) {
                json error;
                error["success"] = false;
                error["error"] = "Account temporarily locked. Please try again later.";
                res.body = error.dump();
                res.statusCode = 429;
                return res;
            }
            
            // 验证密码
            bool success = ConfigManager::getInstance().verifyPassword(password);
            
            if (success) {
                // 记录成功登录
                ConfigManager::getInstance().recordSuccessfulLogin(clientId);
                
                // 创建会话
                std::string token = ConfigManager::getInstance().createSession();
                
                json response;
                response["success"] = true;
                response["token"] = token;
                res.body = response.dump();
            } else {
                // 记录失败登录
                ConfigManager::getInstance().recordFailedLogin(clientId);
                
                json response;
                response["success"] = false;
                response["error"] = "Invalid password";
                res.body = response.dump();
                res.statusCode = 401;
            }
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 400;
        }
        
        return res;
    });
    
    // 存储使用情况API
    registerRoute("GET", "/api/storage/usage", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            uint64_t bytes = StorageManager::getInstance().getStorageUsage();
            std::string formatted = StorageManager::formatStorageSize(bytes);
            
            json response;
            response["success"] = true;
            response["bytes"] = bytes;
            response["formatted"] = formatted;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 获取已删除任务列表
    registerRoute("GET", "/api/storage/deleted", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            auto deletedTasks = StorageManager::getInstance().listDeletedTasks();
            
            json response;
            response["success"] = true;
            response["count"] = deletedTasks.size();
            response["tasks"] = deletedTasks;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 永久删除所有已删除的任务
    registerRoute("POST", "/api/storage/cleanup", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        // 要求认证
        if (!requireAuth(req, res)) {
            return res;
        }
        
        try {
            int count = StorageManager::getInstance().permanentDeleteAllDeleted();
            
            json response;
            response["success"] = true;
            response["deletedCount"] = count;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 删除除今天以外的所有任务
    registerRoute("POST", "/api/storage/cleanup-old", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        // 要求认证
        if (!requireAuth(req, res)) {
            return res;
        }
        
        try {
            // 获取今天的日期字符串 YYYY-MM-DD
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d");
            std::string today = oss.str();
            
            int deletedCount = 0;
            
            // 遍历data目录下的日期文件夹
            DIR* dir = opendir("data");
            if (dir) {
                struct dirent* dateEntry;
                while ((dateEntry = readdir(dir)) != nullptr) {
                    if (dateEntry->d_name[0] == '.') continue;
                    std::string dateName = dateEntry->d_name;
                    
                    // 跳过今天的文件夹
                    if (dateName == today) continue;
                    
                    std::string datePath = std::string("data/") + dateName;
                    DIR* dateDir = opendir(datePath.c_str());
                    if (!dateDir) continue;
                    
                    struct dirent* taskEntry;
                    while ((taskEntry = readdir(dateDir)) != nullptr) {
                        if (taskEntry->d_name[0] == '.') continue;
                        std::string taskId = dateName + "/" + taskEntry->d_name;
                        if (StorageManager::getInstance().permanentDeleteTask(taskId)) {
                            deletedCount++;
                        }
                    }
                    closedir(dateDir);
                    
                    // 尝试删除空的日期目录
                    rmdir(datePath.c_str());
                }
                closedir(dir);
            }
            
            json response;
            response["success"] = true;
            response["deletedCount"] = deletedCount;
            response["today"] = today;
            res.body = response.dump();
            
            Logger::getInstance().info("Cleaned up " + std::to_string(deletedCount) + " old tasks (kept today: " + today + ")");
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 获取日志文件大小
    registerRoute("GET", "/api/logs/size", [](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        try {
            auto stats = Logger::getInstance().getLogStats();
            std::string formatted = StorageManager::formatStorageSize(stats.totalSize);
            
            json response;
            response["success"] = true;
            response["bytes"] = stats.totalSize;
            response["fileCount"] = stats.fileCount;
            response["formatted"] = formatted;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 清理所有日志文件
    registerRoute("POST", "/api/logs/clear", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        // 要求认证
        if (!requireAuth(req, res)) {
            return res;
        }
        
        try {
            int count = Logger::getInstance().clearAllLogs();
            
            json response;
            response["success"] = true;
            response["deletedCount"] = count;
            res.body = response.dump();
            
            Logger::getInstance().info("Log files cleared by user");
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 手动删除过期日志
    registerRoute("POST", "/api/logs/delete-old", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        // 要求认证
        if (!requireAuth(req, res)) {
            return res;
        }
        
        try {
            int count = Logger::getInstance().deleteOldLogs();
            
            json response;
            response["success"] = true;
            response["deletedCount"] = count;
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
    
    // 手动归档当前日志
    registerRoute("POST", "/api/logs/archive", [this](const HttpRequest& req) -> HttpResponse {
        HttpResponse res;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        
        // 要求认证
        if (!requireAuth(req, res)) {
            return res;
        }
        
        try {
            int result = Logger::getInstance().archiveCurrentLog();
            
            json response;
            response["success"] = result > 0;
            if (result > 0) {
                response["message"] = "Log archived successfully";
            } else {
                response["message"] = "No log to archive";
            }
            res.body = response.dump();
            
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.body = error.dump();
            res.statusCode = 500;
        }
        
        return res;
    });
}

bool WebServer::matchRoute(const std::string& pattern, const std::string& path, 
                           std::map<std::string, std::string>& params) {
    std::vector<std::string> patternParts = split(pattern, '/');
    std::vector<std::string> pathParts = split(path, '/');
    
    // 如果模式部分数量大于路径部分数量，肯定不匹配
    if (patternParts.size() > pathParts.size()) {
        return false;
    }
    
    // 找出参数位置和参数后面的固定部分数量
    int paramIndex = -1;
    for (size_t i = 0; i < patternParts.size(); i++) {
        if (!patternParts[i].empty() && patternParts[i][0] == ':') {
            paramIndex = static_cast<int>(i);
            break;
        }
    }
    
    // 如果没有参数，部分数量必须相等且完全匹配
    if (paramIndex < 0) {
        if (patternParts.size() != pathParts.size()) {
            return false;
        }
        for (size_t i = 0; i < patternParts.size(); i++) {
            if (patternParts[i] != pathParts[i]) {
                return false;
            }
        }
        return true;
    }
    
    // 计算参数后面有多少个固定部分
    size_t suffixCount = patternParts.size() - paramIndex - 1;
    
    // 路径部分数量必须至少等于：参数前的部分 + 1(参数本身至少1个) + 后缀部分
    if (pathParts.size() < static_cast<size_t>(paramIndex) + 1 + suffixCount) {
        return false;
    }
    
    // 匹配参数前的固定部分
    for (int i = 0; i < paramIndex; i++) {
        if (patternParts[i] != pathParts[i]) {
            return false;
        }
    }
    
    // 匹配参数后的固定部分（从后往前）
    for (size_t i = 0; i < suffixCount; i++) {
        size_t patternIdx = patternParts.size() - 1 - i;
        size_t pathIdx = pathParts.size() - 1 - i;
        if (patternParts[patternIdx] != pathParts[pathIdx]) {
            return false;
        }
    }
    
    // 提取参数值（中间的所有部分）
    std::string paramName = patternParts[paramIndex].substr(1);
    size_t paramEndIdx = pathParts.size() - suffixCount;
    std::string paramValue;
    for (size_t i = paramIndex; i < paramEndIdx; i++) {
        if (i > static_cast<size_t>(paramIndex)) paramValue += "/";
        paramValue += pathParts[i];
    }
    params[paramName] = paramValue;
    
    return true;
}

HttpResponse WebServer::serveStaticFile(const std::string& path) {
    HttpResponse response;
    
    // 安全检查：防止目录遍历攻击
    if (path.find("..") != std::string::npos) {
        response.statusCode = 400;
        response.body = "Bad Request";
        return response;
    }
    
    // 首先尝试从嵌入式资源获取
#ifdef EMBED_RESOURCES
    const EmbeddedResource* embedded = getEmbeddedResource(path);
    if (embedded) {
        response.body = std::string(reinterpret_cast<const char*>(embedded->data), embedded->size);
        response.headers["Content-Type"] = embedded->mimeType;
        return response;
    }
    
    // 尝试 /index.html
    if (path == "/" || path.back() == '/') {
        std::string indexPath = path + "index.html";
        embedded = getEmbeddedResource(indexPath);
        if (embedded) {
            response.body = std::string(reinterpret_cast<const char*>(embedded->data), embedded->size);
            response.headers["Content-Type"] = embedded->mimeType;
            return response;
        }
    }
#endif
    
    // 从文件系统读取
    // 构建文件路径
    std::string filePath = webRoot_ + path;
    
    // 如果是目录，尝试index.html
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        filePath += "/index.html";
    }
    
    // 读取文件
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        response.statusCode = 404;
        response.body = "404 Not Found";
        return response;
    }
    
    // 读取文件内容
    std::ostringstream oss;
    oss << file.rdbuf();
    response.body = oss.str();
    
    // 设置Content-Type
    std::string ext = getFileExtension(filePath);
    response.headers["Content-Type"] = getMimeType(ext);
    
    return response;
}

void WebServer::parseQueryString(const std::string& query, 
                                 std::map<std::string, std::string>& params) {
    std::istringstream stream(query);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);
            params[urlDecode(key)] = urlDecode(value);
        }
    }
}

std::string WebServer::urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::vector<std::string> WebServer::split(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream stream(str);
    std::string part;
    
    while (std::getline(stream, part, delimiter)) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    return parts;
}

std::string WebServer::getFileExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        return path.substr(dotPos + 1);
    }
    return "";
}

std::string WebServer::getMimeType(const std::string& ext) {
    static std::map<std::string, std::string> mimeTypes = {
        {"html", "text/html; charset=utf-8"},
        {"htm", "text/html; charset=utf-8"},
        {"css", "text/css; charset=utf-8"},
        {"js", "application/javascript; charset=utf-8"},
        {"json", "application/json; charset=utf-8"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"txt", "text/plain; charset=utf-8"}
    };
    
    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string WebServer::getAuthToken(const HttpRequest& req) {
    // 从Authorization header获取token
    auto it = req.headers.find("Authorization");
    if (it != req.headers.end()) {
        std::string auth = it->second;
        // 格式: "Bearer <token>"
        if (auth.substr(0, 7) == "Bearer ") {
            return auth.substr(7);
        }
    }
    
    // 从X-Session-Token header获取token
    it = req.headers.find("X-Session-Token");
    if (it != req.headers.end()) {
        return it->second;
    }
    
    return "";
}

bool WebServer::requireAuth(const HttpRequest& req, HttpResponse& res) {
    std::string token = getAuthToken(req);
    
    if (token.empty()) {
        json error;
        error["success"] = false;
        error["error"] = "Authentication required";
        res.body = error.dump();
        res.statusCode = 401;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        return false;
    }
    
    if (!ConfigManager::getInstance().validateSession(token)) {
        json error;
        error["success"] = false;
        error["error"] = "Invalid or expired session";
        res.body = error.dump();
        res.statusCode = 401;
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        return false;
    }
    
    return true;
}
