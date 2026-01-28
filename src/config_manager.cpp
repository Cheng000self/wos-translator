#include "config_manager.h"
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <cmath>
#include <openssl/sha.h>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() 
    : systemConfigPath_("config/system.json"),
      modelsConfigPath_("config/models.json") {
}

void ConfigManager::initialize() {
    // 创建必要的目录
    mkdir("config", 0755);
    mkdir("data", 0755);
    mkdir("logs", 0755);
    mkdir("web", 0755);
    
    // 如果配置文件不存在，创建默认配置
    createDefaultConfigs();
    
    // 加载配置
    systemConfig_ = loadSystemConfig();
    modelConfigs_ = loadModelConfigs();
    
    Logger::getInstance().info("ConfigManager initialized");
}

void ConfigManager::createDefaultConfigs() {
    // 创建默认系统配置
    std::ifstream sysFile(systemConfigPath_);
    if (!sysFile.good()) {
        SystemConfig defaultConfig;
        defaultConfig.passwordSalt = generateSalt();
        defaultConfig.adminPasswordHash = hashPassword("admin123", defaultConfig.passwordSalt);
        saveSystemConfig(defaultConfig);
        Logger::getInstance().info("Created default system config with password: admin123");
    }
    
    // 创建默认模型配置
    std::ifstream modelsFile(modelsConfigPath_);
    if (!modelsFile.good()) {
        std::ofstream out(modelsConfigPath_);
        json j = json::array();
        out << j.dump(2);
        out.close();
        chmod(modelsConfigPath_.c_str(), 0600);
        Logger::getInstance().info("Created default models config");
    }
}

SystemConfig ConfigManager::loadSystemConfig() {
    SystemConfig config;
    
    try {
        std::ifstream file(systemConfigPath_);
        if (!file.is_open()) {
            Logger::getInstance().warning("System config file not found, using defaults");
            return config;
        }
        
        json j;
        file >> j;
        
        if (j.contains("maxUploadFiles")) config.maxUploadFiles = j["maxUploadFiles"];
        if (j.contains("maxTasks")) config.maxTasks = j["maxTasks"];
        if (j.contains("maxConcurrentTasks")) config.maxConcurrentTasks = j["maxConcurrentTasks"];
        if (j.contains("maxConcurrentTasksPerModel")) config.maxConcurrentTasksPerModel = j["maxConcurrentTasksPerModel"];
        if (j.contains("maxTranslationThreads")) config.maxTranslationThreads = j["maxTranslationThreads"];
        if (j.contains("maxRetries")) config.maxRetries = j["maxRetries"];
        if (j.contains("consecutiveFailureThreshold")) config.consecutiveFailureThreshold = j["consecutiveFailureThreshold"];
        if (j.contains("adminPasswordHash")) config.adminPasswordHash = j["adminPasswordHash"];
        if (j.contains("passwordSalt")) config.passwordSalt = j["passwordSalt"];
        if (j.contains("serverPort")) config.serverPort = j["serverPort"];
        if (j.contains("logLevel")) config.logLevel = j["logLevel"];
        if (j.contains("sessionTimeoutMinutes")) config.sessionTimeoutMinutes = j["sessionTimeoutMinutes"];
        if (j.contains("maxLoginAttempts")) config.maxLoginAttempts = j["maxLoginAttempts"];
        if (j.contains("lockoutDurationMinutes")) config.lockoutDurationMinutes = j["lockoutDurationMinutes"];
        
        // 日志管理配置
        if (j.contains("logManageMode")) config.logManageMode = static_cast<LogManageMode>(j["logManageMode"].get<int>());
        if (j.contains("logRetentionDays")) config.logRetentionDays = j["logRetentionDays"];
        if (j.contains("logArchiveIntervalDays")) config.logArchiveIntervalDays = j["logArchiveIntervalDays"];
        
        // 不再打印日志，避免频繁输出
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load system config: " + std::string(e.what()));
    }
    
    return config;
}

bool ConfigManager::saveSystemConfig(const SystemConfig& config) {
    try {
        json j;
        j["maxUploadFiles"] = config.maxUploadFiles;
        j["maxTasks"] = config.maxTasks;
        j["maxConcurrentTasks"] = config.maxConcurrentTasks;
        j["maxConcurrentTasksPerModel"] = config.maxConcurrentTasksPerModel;
        j["maxTranslationThreads"] = config.maxTranslationThreads;
        j["maxRetries"] = config.maxRetries;
        j["consecutiveFailureThreshold"] = config.consecutiveFailureThreshold;
        j["adminPasswordHash"] = config.adminPasswordHash;
        j["passwordSalt"] = config.passwordSalt;
        j["serverPort"] = config.serverPort;
        j["logLevel"] = config.logLevel;
        j["sessionTimeoutMinutes"] = config.sessionTimeoutMinutes;
        j["maxLoginAttempts"] = config.maxLoginAttempts;
        j["lockoutDurationMinutes"] = config.lockoutDurationMinutes;
        
        // 日志管理配置
        j["logManageMode"] = static_cast<int>(config.logManageMode);
        j["logRetentionDays"] = config.logRetentionDays;
        j["logArchiveIntervalDays"] = config.logArchiveIntervalDays;
        
        std::ofstream file(systemConfigPath_);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open system config file for writing");
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        // 设置文件权限为600（仅所有者可读写）
        chmod(systemConfigPath_.c_str(), 0600);
        
        systemConfig_ = config;
        Logger::getInstance().info("System config saved");
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save system config: " + std::string(e.what()));
        return false;
    }
}

std::vector<ModelConfig> ConfigManager::loadModelConfigs() {
    std::vector<ModelConfig> configs;
    
    try {
        std::ifstream file(modelsConfigPath_);
        if (!file.is_open()) {
            Logger::getInstance().warning("Models config file not found");
            return configs;
        }
        
        json j;
        file >> j;
        
        for (const auto& item : j) {
            ModelConfig config;
            config.id = item["id"];
            config.name = item["name"];
            config.url = item["url"];
            config.apiKey = item["apiKey"];
            config.modelId = item["modelId"];
            
            // 四舍五入温度到2位小数
            float temp = item["temperature"];
            config.temperature = std::round(temp * 100.0f) / 100.0f;
            
            config.systemPrompt = item["systemPrompt"];
            configs.push_back(config);
        }
        
        Logger::getInstance().info("Loaded " + std::to_string(configs.size()) + " model configs");
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load model configs: " + std::string(e.what()));
    }
    
    return configs;
}

bool ConfigManager::saveModelConfig(const ModelConfig& config) {
    try {
        // 检查ID是否已存在
        for (auto& existing : modelConfigs_) {
            if (existing.id == config.id) {
                // 更新现有配置
                existing = config;
                
                // 保存到文件
                json j = json::array();
                for (const auto& mc : modelConfigs_) {
                    json item;
                    item["id"] = mc.id;
                    item["name"] = mc.name;
                    item["url"] = mc.url;
                    item["apiKey"] = mc.apiKey;
                    item["modelId"] = mc.modelId;
                    // 使用double计算并四舍五入到2位小数
                    double temp = std::round(static_cast<double>(mc.temperature) * 100.0) / 100.0;
                    item["temperature"] = temp;
                    item["systemPrompt"] = mc.systemPrompt;
                    j.push_back(item);
                }
                
                std::ofstream file(modelsConfigPath_);
                file << j.dump(2);
                file.close();
                
                chmod(modelsConfigPath_.c_str(), 0600);
                Logger::getInstance().info("Model config updated: " + config.id);
                return true;
            }
        }
        
        // 添加新配置
        modelConfigs_.push_back(config);
        
        json j = json::array();
        for (const auto& mc : modelConfigs_) {
            json item;
            item["id"] = mc.id;
            item["name"] = mc.name;
            item["url"] = mc.url;
            item["apiKey"] = mc.apiKey;
            item["modelId"] = mc.modelId;
            // 使用double计算并四舍五入到2位小数
            double temp = std::round(static_cast<double>(mc.temperature) * 100.0) / 100.0;
            item["temperature"] = temp;
            item["systemPrompt"] = mc.systemPrompt;
            j.push_back(item);
        }
        
        std::ofstream file(modelsConfigPath_);
        file << j.dump(2);
        file.close();
        
        chmod(modelsConfigPath_.c_str(), 0600);
        Logger::getInstance().info("Model config saved: " + config.id);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save model config: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::updateModelConfig(const std::string& id, const ModelConfig& config) {
    ModelConfig updatedConfig = config;
    updatedConfig.id = id;
    return saveModelConfig(updatedConfig);
}

bool ConfigManager::deleteModelConfig(const std::string& id) {
    try {
        auto it = std::remove_if(modelConfigs_.begin(), modelConfigs_.end(),
            [&id](const ModelConfig& config) { return config.id == id; });
        
        if (it == modelConfigs_.end()) {
            Logger::getInstance().warning("Model config not found: " + id);
            return false;
        }
        
        modelConfigs_.erase(it, modelConfigs_.end());
        
        // 保存到文件
        json j = json::array();
        for (const auto& mc : modelConfigs_) {
            json item;
            item["id"] = mc.id;
            item["name"] = mc.name;
            item["url"] = mc.url;
            item["apiKey"] = mc.apiKey;
            item["modelId"] = mc.modelId;
            item["temperature"] = mc.temperature;
            item["systemPrompt"] = mc.systemPrompt;
            j.push_back(item);
        }
        
        std::ofstream file(modelsConfigPath_);
        file << j.dump(2);
        file.close();
        
        Logger::getInstance().info("Model config deleted: " + id);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to delete model config: " + std::string(e.what()));
        return false;
    }
}

ModelConfig ConfigManager::getModelConfig(const std::string& id) {
    for (const auto& config : modelConfigs_) {
        if (config.id == id) {
            return config;
        }
    }
    return ModelConfig();
}

// 密码验证和哈希
std::string ConfigManager::generateSalt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    return ss.str();
}

std::string ConfigManager::hashPassword(const std::string& password, const std::string& salt) {
    // 使用SHA-256哈希密码+盐值
    std::string combined = password + salt;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool ConfigManager::verifyPassword(const std::string& password) {
    std::string hashedInput = hashPassword(password, systemConfig_.passwordSalt);
    bool isValid = (hashedInput == systemConfig_.adminPasswordHash);
    
    if (isValid) {
        Logger::getInstance().info("Password verification successful");
    } else {
        Logger::getInstance().warning("Password verification failed");
    }
    
    return isValid;
}

bool ConfigManager::changePassword(const std::string& oldPassword, const std::string& newPassword) {
    if (!verifyPassword(oldPassword)) {
        Logger::getInstance().warning("Failed to change password: old password incorrect");
        return false;
    }
    
    // 验证新密码强度
    if (newPassword.length() < 6) {
        Logger::getInstance().warning("Failed to change password: new password too short");
        return false;
    }
    
    // 生成新盐值和哈希
    systemConfig_.passwordSalt = generateSalt();
    systemConfig_.adminPasswordHash = hashPassword(newPassword, systemConfig_.passwordSalt);
    
    bool success = saveSystemConfig(systemConfig_);
    if (success) {
        Logger::getInstance().info("Password changed successfully");
    }
    
    return success;
}

// 会话管理
std::string ConfigManager::generateRandomToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    return ss.str();
}

std::string ConfigManager::createSession() {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    // 清理过期会话
    cleanupExpiredSessions();
    
    Session session;
    session.token = generateRandomToken();
    session.createdAt = std::chrono::system_clock::now();
    session.lastAccessedAt = session.createdAt;
    session.isValid = true;
    
    sessions_[session.token] = session;
    
    Logger::getInstance().info("Session created: " + session.token.substr(0, 8) + "...");
    return session.token;
}

bool ConfigManager::validateSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }
    
    Session& session = it->second;
    if (!session.isValid) {
        return false;
    }
    
    // 检查会话是否过期
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
        now - session.lastAccessedAt).count();
    
    if (elapsed > systemConfig_.sessionTimeoutMinutes) {
        session.isValid = false;
        Logger::getInstance().info("Session expired: " + token.substr(0, 8) + "...");
        return false;
    }
    
    // 更新最后访问时间
    session.lastAccessedAt = now;
    return true;
}

void ConfigManager::invalidateSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    auto it = sessions_.find(token);
    if (it != sessions_.end()) {
        it->second.isValid = false;
        Logger::getInstance().info("Session invalidated: " + token.substr(0, 8) + "...");
    }
}

void ConfigManager::cleanupExpiredSessions() {
    auto now = std::chrono::system_clock::now();
    
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - it->second.lastAccessedAt).count();
        
        if (elapsed > systemConfig_.sessionTimeoutMinutes || !it->second.isValid) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

// 防暴力破解
bool ConfigManager::checkLoginAttempt(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(loginAttemptMutex_);
    
    auto it = loginAttempts_.find(clientId);
    if (it == loginAttempts_.end()) {
        return true;  // 首次尝试
    }
    
    LoginAttempt& attempt = it->second;
    auto now = std::chrono::system_clock::now();
    
    // 检查是否在锁定期内
    if (now < attempt.lockoutUntil) {
        auto remaining = std::chrono::duration_cast<std::chrono::minutes>(
            attempt.lockoutUntil - now).count();
        Logger::getInstance().warning("Login attempt blocked for " + clientId + 
            ", locked for " + std::to_string(remaining) + " more minutes");
        return false;
    }
    
    // 锁定期已过，重置计数
    if (now >= attempt.lockoutUntil && attempt.failedCount >= systemConfig_.maxLoginAttempts) {
        attempt.failedCount = 0;
        attempt.lockoutUntil = std::chrono::system_clock::time_point();
    }
    
    return true;
}

void ConfigManager::recordFailedLogin(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(loginAttemptMutex_);
    
    auto now = std::chrono::system_clock::now();
    LoginAttempt& attempt = loginAttempts_[clientId];
    
    attempt.failedCount++;
    attempt.lastAttemptTime = now;
    
    if (attempt.failedCount >= systemConfig_.maxLoginAttempts) {
        attempt.lockoutUntil = now + std::chrono::minutes(systemConfig_.lockoutDurationMinutes);
        Logger::getInstance().warning("Client " + clientId + " locked out for " + 
            std::to_string(systemConfig_.lockoutDurationMinutes) + " minutes after " + 
            std::to_string(attempt.failedCount) + " failed attempts");
    } else {
        Logger::getInstance().warning("Failed login attempt for " + clientId + 
            " (" + std::to_string(attempt.failedCount) + "/" + 
            std::to_string(systemConfig_.maxLoginAttempts) + ")");
    }
}

void ConfigManager::recordSuccessfulLogin(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(loginAttemptMutex_);
    
    auto it = loginAttempts_.find(clientId);
    if (it != loginAttempts_.end()) {
        loginAttempts_.erase(it);
        Logger::getInstance().info("Successful login for " + clientId + ", reset attempt counter");
    }
}

bool ConfigManager::isLockedOut(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(loginAttemptMutex_);
    
    auto it = loginAttempts_.find(clientId);
    if (it == loginAttempts_.end()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    return now < it->second.lockoutUntil;
}

// 输入验证
bool ConfigManager::validateInput(const std::string& input, const std::string& type) {
    if (input.empty()) {
        return false;
    }
    
    if (type == "filename") {
        // 文件名验证：只允许字母、数字、下划线、连字符和点
        std::regex pattern("^[a-zA-Z0-9_\\-\\.]+$");
        return std::regex_match(input, pattern);
    } else if (type == "path") {
        // 路径验证：防止路径遍历攻击
        if (input.find("..") != std::string::npos) {
            return false;
        }
        std::regex pattern("^[a-zA-Z0-9_\\-\\./]+$");
        return std::regex_match(input, pattern);
    } else if (type == "taskid") {
        // 任务ID验证：YYYY-MM-DD/序号格式
        std::regex pattern("^\\d{4}-\\d{2}-\\d{2}/\\d+$");
        return std::regex_match(input, pattern);
    } else if (type == "url") {
        // URL验证：基本的URL格式检查
        std::regex pattern("^https?://[a-zA-Z0-9\\-\\._~:/?#\\[\\]@!$&'()*+,;=]+$");
        return std::regex_match(input, pattern);
    } else if (type == "apikey") {
        // API密钥验证：只允许字母、数字和连字符
        std::regex pattern("^[a-zA-Z0-9\\-_]+$");
        return std::regex_match(input, pattern);
    }
    
    return true;
}

std::string ConfigManager::sanitizeInput(const std::string& input) {
    std::string sanitized = input;
    
    // 移除控制字符
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) { return std::iscntrl(static_cast<unsigned char>(c)); }), 
        sanitized.end());
    
    // 转义HTML特殊字符
    std::string result;
    for (char c : sanitized) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c; break;
        }
    }
    
    return result;
}
