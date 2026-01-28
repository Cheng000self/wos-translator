#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
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
    
    LogLevel currentLevel_;
    std::string logFile_;
    std::ofstream fileStream_;
    std::mutex mutex_;
    
    size_t maxFileSize_;      // 默认10MB
    int maxBackups_;          // 默认保留5个备份
    size_t currentFileSize_;
};

#endif // LOGGER_H
