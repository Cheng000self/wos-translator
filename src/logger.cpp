#include "logger.h"
#include <iostream>
#include <algorithm>
#include <dirent.h>
#include <cstring>
#include <chrono>

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
      logDir_("logs"),
      maxFileSize_(10 * 1024 * 1024),  // 10MB
      maxBackups_(5),
      currentFileSize_(0),
      logManageMode_(LogManageMode::AutoDelete),
      logRetentionDays_(7),
      logArchiveIntervalDays_(30),
      logManagerRunning_(false),
      lastArchiveTime_(0) {
    // 默认日志文件
    setLogFile("logs/app.log");
}

Logger::~Logger() {
    stopLogManager();
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
    
    // 提取日志目录
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        logDir_ = filename.substr(0, lastSlash);
    } else {
        logDir_ = ".";
    }
    
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

std::string Logger::getDateString() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
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
    
    // 不在这里调用info()，避免递归锁
    std::string logEntry = "[" + getCurrentTime() + "] [INFO] Log file rotated";
    if (fileStream_.is_open()) {
        fileStream_ << logEntry << std::endl;
        fileStream_.flush();
    }
    std::cout << logEntry << std::endl;
}

size_t Logger::getFileSize(const std::string& filename) {
    platform_stat_struct st;
    if (platform_stat(filename.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

time_t Logger::getFileModTime(const std::string& filename) {
    platform_stat_struct st;
    if (platform_stat(filename.c_str(), &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

Logger::LogStats Logger::getLogStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LogStats stats = {0, 0};
    
    // 扫描日志目录中的所有文件
    DIR* dir = opendir(logDir_.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {  // 只处理普通文件
                std::string filename = logDir_ + "/" + entry->d_name;
                // 检查是否是日志文件（以app.log开头或包含-app.log）
                std::string name = entry->d_name;
                if (name.find("app.log") != std::string::npos || 
                    name.find("-app.log") != std::string::npos) {
                    size_t fileSize = getFileSize(filename);
                    if (fileSize > 0) {
                        stats.totalSize += fileSize;
                        stats.fileCount++;
                    }
                }
            }
        }
        closedir(dir);
    }
    
    return stats;
}

int Logger::clearAllLogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int deletedCount = 0;
    
    // 关闭当前日志文件
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    
    // 扫描并删除日志目录中的所有日志文件
    DIR* dir = opendir(logDir_.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
                std::string name = entry->d_name;
                // 删除所有日志文件（app.log开头或包含-app.log的归档文件）
                if (name.find("app.log") != std::string::npos || 
                    name.find("-app.log") != std::string::npos) {
                    std::string filepath = logDir_ + "/" + name;
                    if (remove(filepath.c_str()) == 0) {
                        deletedCount++;
                    }
                }
            }
        }
        closedir(dir);
    }
    
    // 重新打开日志文件
    fileStream_.open(logFile_, std::ios::app);
    currentFileSize_ = 0;
    
    return deletedCount;
}

// 定时日志管理配置
void Logger::setLogManageMode(LogManageMode mode) {
    logManageMode_ = mode;
}

void Logger::setLogRetentionDays(int days) {
    if (days > 0) {
        logRetentionDays_ = days;
    }
}

void Logger::setLogArchiveIntervalDays(int days) {
    if (days > 0) {
        logArchiveIntervalDays_ = days;
    }
}

void Logger::startLogManager() {
    if (logManagerRunning_) {
        return;
    }
    
    logManagerRunning_ = true;
    logManagerThread_ = std::thread(&Logger::logManagerThread, this);
    info("Log manager started");
}

void Logger::stopLogManager() {
    if (!logManagerRunning_) {
        return;
    }
    
    logManagerRunning_ = false;
    if (logManagerThread_.joinable()) {
        logManagerThread_.join();
    }
}

void Logger::logManagerThread() {
    // 每小时检查一次
    const int checkIntervalSeconds = 3600;
    
    while (logManagerRunning_) {
        // 等待一段时间
        for (int i = 0; i < checkIntervalSeconds && logManagerRunning_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (!logManagerRunning_) break;
        
        if (logManageMode_ == LogManageMode::AutoDelete) {
            // 定时删除模式：删除过期日志
            int deleted = deleteOldLogs();
            if (deleted > 0) {
                info("Auto-deleted " + std::to_string(deleted) + " old log files");
            }
        } else if (logManageMode_ == LogManageMode::AutoArchive) {
            // 定时归档模式：检查是否需要归档
            time_t now = std::time(nullptr);
            time_t intervalSeconds = logArchiveIntervalDays_ * 24 * 3600;
            
            if (lastArchiveTime_ == 0) {
                // 首次运行，检查主日志文件的修改时间
                time_t logModTime = getFileModTime(logFile_);
                if (logModTime > 0 && (now - logModTime) >= intervalSeconds) {
                    if (archiveCurrentLog() > 0) {
                        lastArchiveTime_ = now;
                        info("Auto-archived log file");
                    }
                }
            } else if ((now - lastArchiveTime_) >= intervalSeconds) {
                if (archiveCurrentLog() > 0) {
                    lastArchiveTime_ = now;
                    info("Auto-archived log file");
                }
            }
        }
    }
}

int Logger::deleteOldLogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int deletedCount = 0;
    time_t now = std::time(nullptr);
    time_t retentionSeconds = logRetentionDays_ * 24 * 3600;
    
    DIR* dir = opendir(logDir_.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
                std::string name = entry->d_name;
                std::string filepath = logDir_ + "/" + name;
                
                // 跳过当前正在使用的主日志文件
                if (filepath == logFile_) {
                    continue;
                }
                
                // 检查是否是日志文件
                if (name.find("app.log") != std::string::npos || 
                    name.find("-app.log") != std::string::npos) {
                    
                    time_t modTime = getFileModTime(filepath);
                    if (modTime > 0 && (now - modTime) > retentionSeconds) {
                        if (remove(filepath.c_str()) == 0) {
                            deletedCount++;
                        }
                    }
                }
            }
        }
        closedir(dir);
    }
    
    return deletedCount;
}

int Logger::archiveCurrentLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查当前日志文件是否存在且有内容
    size_t currentSize = getFileSize(logFile_);
    if (currentSize == 0) {
        return 0;
    }
    
    // 关闭当前日志文件
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    
    // 生成归档文件名：YYYY-MM-DD-app.log
    std::string archiveName = logDir_ + "/" + getDateString() + "-app.log";
    
    // 如果归档文件已存在，添加序号
    int seq = 1;
    while (getFileSize(archiveName) > 0) {
        archiveName = logDir_ + "/" + getDateString() + "-app-" + std::to_string(seq) + ".log";
        seq++;
    }
    
    // 重命名当前日志文件为归档文件
    int result = rename(logFile_.c_str(), archiveName.c_str());
    
    // 重新打开日志文件
    fileStream_.open(logFile_, std::ios::app);
    currentFileSize_ = 0;
    
    if (result == 0) {
        // 不在这里调用info()，避免递归锁
        std::string logEntry = "[" + getCurrentTime() + "] [INFO] Log archived to: " + archiveName;
        if (fileStream_.is_open()) {
            fileStream_ << logEntry << std::endl;
            fileStream_.flush();
        }
        std::cout << logEntry << std::endl;
        return 1;
    }
    
    return 0;
}
