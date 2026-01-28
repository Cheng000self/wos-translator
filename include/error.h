#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <exception>

// 错误代码枚举
enum class ErrorCode {
    // 通用错误
    SUCCESS = 0,
    UNKNOWN_ERROR = 1,
    
    // 文件相关错误 (100-199)
    FILE_NOT_FOUND = 100,
    FILE_READ_ERROR = 101,
    FILE_WRITE_ERROR = 102,
    FILE_INVALID_FORMAT = 103,
    FILE_TOO_LARGE = 104,
    FILE_PERMISSION_DENIED = 105,
    
    // HTML解析错误 (200-299)
    HTML_PARSE_ERROR = 200,
    HTML_INVALID_WOS_FORMAT = 201,
    HTML_NO_RECORDS_FOUND = 202,
    HTML_INCOMPLETE_DATA = 203,
    
    // 翻译错误 (300-399)
    TRANSLATION_API_ERROR = 300,
    TRANSLATION_NETWORK_ERROR = 301,
    TRANSLATION_TIMEOUT = 302,
    TRANSLATION_AUTH_ERROR = 303,
    TRANSLATION_RATE_LIMIT = 304,
    TRANSLATION_INVALID_RESPONSE = 305,
    
    // 任务管理错误 (400-499)
    TASK_NOT_FOUND = 400,
    TASK_ALREADY_EXISTS = 401,
    TASK_INVALID_STATE = 402,
    TASK_CREATION_FAILED = 403,
    TASK_QUEUE_FULL = 404,
    
    // 配置错误 (500-599)
    CONFIG_LOAD_ERROR = 500,
    CONFIG_SAVE_ERROR = 501,
    CONFIG_INVALID_VALUE = 502,
    CONFIG_MISSING_REQUIRED = 503,
    
    // 认证和安全错误 (600-699)
    AUTH_INVALID_PASSWORD = 600,
    AUTH_SESSION_EXPIRED = 601,
    AUTH_SESSION_INVALID = 602,
    AUTH_ACCOUNT_LOCKED = 603,
    AUTH_PERMISSION_DENIED = 604,
    
    // 网络和服务器错误 (700-799)
    SERVER_START_FAILED = 700,
    SERVER_BIND_ERROR = 701,
    SERVER_REQUEST_INVALID = 702,
    SERVER_RESPONSE_ERROR = 703,
    
    // 存储错误 (800-899)
    STORAGE_CREATE_DIR_FAILED = 800,
    STORAGE_DELETE_FAILED = 801,
    STORAGE_SPACE_INSUFFICIENT = 802,
    STORAGE_CORRUPTED_DATA = 803
};

// 错误结构体
struct Error {
    ErrorCode code;
    std::string message;
    std::string details;
    
    Error() : code(ErrorCode::SUCCESS), message(""), details("") {}
    
    Error(ErrorCode c, const std::string& msg, const std::string& det = "")
        : code(c), message(msg), details(det) {}
    
    bool isSuccess() const { return code == ErrorCode::SUCCESS; }
    bool isError() const { return code != ErrorCode::SUCCESS; }
    
    std::string toString() const {
        std::string result = "[" + std::to_string(static_cast<int>(code)) + "] " + message;
        if (!details.empty()) {
            result += " - " + details;
        }
        return result;
    }
};

// 异常类
class WosException : public std::exception {
public:
    WosException(ErrorCode code, const std::string& message, const std::string& details = "")
        : error_(code, message, details) {}
    
    WosException(const Error& error) : error_(error) {}
    
    const char* what() const noexcept override {
        return error_.message.c_str();
    }
    
    const Error& getError() const { return error_; }
    ErrorCode getCode() const { return error_.code; }
    
private:
    Error error_;
};

// 错误工厂函数
namespace ErrorFactory {
    inline Error fileNotFound(const std::string& filename) {
        return Error(ErrorCode::FILE_NOT_FOUND, "File not found", filename);
    }
    
    inline Error fileReadError(const std::string& filename, const std::string& reason) {
        return Error(ErrorCode::FILE_READ_ERROR, "Failed to read file: " + filename, reason);
    }
    
    inline Error fileWriteError(const std::string& filename, const std::string& reason) {
        return Error(ErrorCode::FILE_WRITE_ERROR, "Failed to write file: " + filename, reason);
    }
    
    inline Error invalidWosFormat(const std::string& reason) {
        return Error(ErrorCode::HTML_INVALID_WOS_FORMAT, "Invalid Web of Science format", reason);
    }
    
    inline Error translationError(const std::string& reason) {
        return Error(ErrorCode::TRANSLATION_API_ERROR, "Translation failed", reason);
    }
    
    inline Error networkError(const std::string& reason) {
        return Error(ErrorCode::TRANSLATION_NETWORK_ERROR, "Network error", reason);
    }
    
    inline Error authError(const std::string& reason) {
        return Error(ErrorCode::TRANSLATION_AUTH_ERROR, "Authentication failed", reason);
    }
    
    inline Error taskNotFound(const std::string& taskId) {
        return Error(ErrorCode::TASK_NOT_FOUND, "Task not found", taskId);
    }
    
    inline Error invalidPassword() {
        return Error(ErrorCode::AUTH_INVALID_PASSWORD, "Invalid password", "");
    }
    
    inline Error sessionExpired() {
        return Error(ErrorCode::AUTH_SESSION_EXPIRED, "Session expired", "");
    }
    
    inline Error accountLocked(int minutes) {
        return Error(ErrorCode::AUTH_ACCOUNT_LOCKED, 
                    "Account temporarily locked", 
                    "Try again in " + std::to_string(minutes) + " minutes");
    }
}

#endif // ERROR_H
