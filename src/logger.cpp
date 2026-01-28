#include "logger.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define platform_stat _stat
    #define platform_stat_struct struct _stat
#else
    #include <sys/stat.h>
    #define platform_stat stat
    #define platform_stat_struct struct stat
#endif

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : currentLevel_(LogLevel::Info),
      maxFileSize_(10 * 1024 * 1024),  // 10MB
      maxBackups_(5),
      currentFileSize_(0) {
    // 默认日志文件
    setLogFile("logs/app.log");
}

Logger::~Logger() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

void Logger::setLogLevel(const std::string& levelStr) {
    setLogLevel(stringToLevel(levelStr));
}

void Logger::setMaxFileSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxFileSize_ = maxSize;
}

void Logger::setMaxBackups(int maxBackups) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxBackups_ = maxBackups;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    
    logFile_ = filename;
    fileStream_.open(logFile_, std::ios::app);
    
    if (!fileStream_.is_open()) {
        std::cerr << "Failed to open log file: " << logFile_ << std::endl;
    } else {
        currentFileSize_ = getFileSize(logFile_);
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::Debug, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string logEntry = "[" + getCurrentTime() + "] [" + levelToString(level) + "] " + message;
    
    // 输出到控制台
    if (level >= LogLevel::Warning) {
        std::cerr << logEntry << std::endl;
    } else {
        std::cout << logEntry << std::endl;
    }
    
    // 输出到文件
    if (fileStream_.is_open()) {
        fileStream_ << logEntry << std::endl;
        fileStream_.flush();
        
        currentFileSize_ += logEntry.length() + 1;  // +1 for newline
        
        // 检查是否需要轮转
        checkAndRotate();
    }
}

void Logger::audit(const std::string& operation, const std::string& details) {
    std::string auditMessage = "[AUDIT] Operation: " + operation;
    if (!details.empty()) {
        auditMessage += " | Details: " + details;
    }
    log(LogLevel::Info, auditMessage);
}

std::string Logger::getCurrentTime() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

LogLevel Logger::stringToLevel(const std::string& levelStr) {
    std::string lower = levelStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "debug") return LogLevel::Debug;
    if (lower == "info") return LogLevel::Info;
    if (lower == "warning") return LogLevel::Warning;
    if (lower == "error") return LogLevel::Error;
    
    return LogLevel::Info;  // 默认
}

void Logger::checkAndRotate() {
    if (currentFileSize_ >= maxFileSize_) {
        rotateLogFile();
    }
}

void Logger::rotateLogFile() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    
    // 删除最旧的备份
    std::string oldestBackup = logFile_ + "." + std::to_string(maxBackups_);
    remove(oldestBackup.c_str());
    
    // 重命名现有备份
    for (int i = maxBackups_ - 1; i >= 1; i--) {
        std::string oldName = logFile_ + "." + std::to_string(i);
        std::string newName = logFile_ + "." + std::to_string(i + 1);
        rename(oldName.c_str(), newName.c_str());
    }
    
    // 重命名当前日志文件
    std::string backup = logFile_ + ".1";
    rename(logFile_.c_str(), backup.c_str());
    
    // 创建新的日志文件
    fileStream_.open(logFile_, std::ios::app);
    currentFileSize_ = 0;
    
    info("Log file rotated");
}

size_t Logger::getFileSize(const std::string& filename) {
    platform_stat_struct st;
    if (platform_stat(filename.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}
