#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct SystemConfig {
    int maxUploadFiles = 1;              // 每个任务最大上传文件数
    int maxTasks = 50;                   // 最大任务数
    int maxConcurrentTasks = 1;          // 最大并发任务数（总体）
    int maxConcurrentTasksPerModel = 1;  // 同一模型最大并发任务数
    int maxTranslationThreads = 1;       // 任务内翻译线程数
    int maxRetries = 3;                  // 翻译重试次数
    int consecutiveFailureThreshold = 5; // 连续失败阈值
    std::string adminPasswordHash = "";  // SHA-256哈希
    std::string passwordSalt = "";       // 密码盐值
    int serverPort = 8080;
    std::string logLevel = "info";
    int sessionTimeoutMinutes = 30;      // 会话超时时间（分钟）
    int maxLoginAttempts = 3;            // 最大登录尝试次数
    int lockoutDurationMinutes = 5;      // 锁定时长（分钟）
};

struct ModelConfig {
    std::string id;
    std::string name;
    std::string url;
    std::string apiKey;
    std::string modelId;
    float temperature = 0.3f;
    std::string systemPrompt;
};

struct Session {
    std::string token;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastAccessedAt;
    bool isValid;
};

struct LoginAttempt {
    int failedCount;
    std::chrono::system_clock::time_point lastAttemptTime;
    std::chrono::system_clock::time_point lockoutUntil;
};

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // 系统配置
    SystemConfig loadSystemConfig();
    bool saveSystemConfig(const SystemConfig& config);
    SystemConfig& getSystemConfig() { return systemConfig_; }
    
    // 模型配置
    std::vector<ModelConfig> loadModelConfigs();
    bool saveModelConfig(const ModelConfig& config);
    bool updateModelConfig(const std::string& id, const ModelConfig& config);
    bool deleteModelConfig(const std::string& id);
    ModelConfig getModelConfig(const std::string& id);
    
    // 密码验证和哈希
    bool verifyPassword(const std::string& password);
    std::string hashPassword(const std::string& password, const std::string& salt);
    std::string generateSalt();
    bool changePassword(const std::string& oldPassword, const std::string& newPassword);
    
    // 会话管理
    std::string createSession();
    bool validateSession(const std::string& token);
    void invalidateSession(const std::string& token);
    void cleanupExpiredSessions();
    
    // 防暴力破解
    bool checkLoginAttempt(const std::string& clientId);
    void recordFailedLogin(const std::string& clientId);
    void recordSuccessfulLogin(const std::string& clientId);
    bool isLockedOut(const std::string& clientId);
    
    // 输入验证
    bool validateInput(const std::string& input, const std::string& type);
    std::string sanitizeInput(const std::string& input);
    
    // 初始化
    void initialize();
    
private:
    ConfigManager();
    ~ConfigManager() = default;
    
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    std::string systemConfigPath_;
    std::string modelsConfigPath_;
    
    SystemConfig systemConfig_;
    std::vector<ModelConfig> modelConfigs_;
    
    // 会话管理
    std::map<std::string, Session> sessions_;
    std::mutex sessionMutex_;
    
    // 登录尝试跟踪
    std::map<std::string, LoginAttempt> loginAttempts_;
    std::mutex loginAttemptMutex_;
    
    void createDefaultConfigs();
    std::string generateRandomToken();
};

#endif // CONFIG_MANAGER_H
