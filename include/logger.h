#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

// 日志管理模式
enum class LogManageMode {
    AutoDelete = 0,   // 定时删除
    AutoArchive = 1   // 定时归档
};

class Logger {
public:
    static Logger& getInstance();
    
    void setLogLevel(LogLevel level);
    void setLogLevel(const std::string& levelStr);
    void setLogFile(const std::string& filename);
    void setMaxFileSize(size_t maxSize);  // 最大日志文件大小（字节）
    void setMaxBackups(int maxBackups);   // 最大备份文件数
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    void log(LogLevel level, const std::string& message);
    
    // 关键操作审计日志
    void audit(const std::string& operation, const std::string& details);
    
    // 日志管理
    struct LogStats {
        uint64_t totalSize;
        int fileCount;
    };
    LogStats getLogStats();
    int clearAllLogs();
    
    // 定时日志管理
    void setLogManageMode(LogManageMode mode);
    void setLogRetentionDays(int days);
    void setLogArchiveIntervalDays(int days);
    void startLogManager();
    void stopLogManager();
    
    // 手动触发日志管理
    int deleteOldLogs();      // 删除过期日志，返回删除数量
    int archiveCurrentLog();  // 归档当前日志，返回1成功0失败
    
private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string getCurrentTime();
    std::string levelToString(LogLevel level);
    LogLevel stringToLevel(const std::string& levelStr);
    
    void rotateLogFile();
    void checkAndRotate();
    size_t getFileSize(const std::string& filename);
    
    // 日志管理线程
    void logManagerThread();
    std::string getDateString();
    time_t getFileModTime(const std::string& filename);
    
    LogLevel currentLevel_;
    std::string logFile_;
    std::string logDir_;
    std::ofstream fileStream_;
    std::mutex mutex_;
    
    size_t maxFileSize_;      // 默认10MB
    int maxBackups_;          // 默认保留5个备份
    size_t currentFileSize_;
    
    // 日志管理配置
    LogManageMode logManageMode_;
    int logRetentionDays_;
    int logArchiveIntervalDays_;
    std::thread logManagerThread_;
    std::atomic<bool> logManagerRunning_;
    time_t lastArchiveTime_;
};

#endif // LOGGER_H
