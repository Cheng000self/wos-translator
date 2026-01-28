/**
 * @file platform.h
 * @brief 跨平台兼容层
 * 
 * 提供 Windows 和 Linux/Unix 的统一 API 接口
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    // Windows 平台
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    
    // 类型定义
    typedef SOCKET socket_t;
    typedef int socklen_t;
    typedef SSIZE_T ssize_t;
    
    // 常量
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    
    // 函数映射
    #define platform_close_socket closesocket
    #define platform_read(fd, buf, len) recv(fd, (char*)(buf), len, 0)
    #define platform_write(fd, buf, len) send(fd, (const char*)(buf), len, 0)
    #define platform_mkdir(path, mode) _mkdir(path)
    #define platform_stat _stat
    #define platform_stat_struct struct _stat
    #define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
    
    // 信号处理
    #ifndef SIGINT
        #define SIGINT 2
    #endif
    #ifndef SIGTERM
        #define SIGTERM 15
    #endif
    
    // 初始化 Winsock
    inline bool platform_init_network() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }
    
    inline void platform_cleanup_network() {
        WSACleanup();
    }
    
    // 获取最后的 socket 错误
    inline int platform_socket_error() {
        return WSAGetLastError();
    }
    
    // 设置 socket 选项
    inline int platform_setsockopt(socket_t s, int level, int optname, const void* optval, int optlen) {
        return setsockopt(s, level, optname, (const char*)optval, optlen);
    }
    
#else
    // Linux/Unix 平台
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <signal.h>
    #include <errno.h>
    
    // 类型定义
    typedef int socket_t;
    
    // 常量
    #define INVALID_SOCKET_VALUE (-1)
    #define SOCKET_ERROR_VALUE (-1)
    
    // 函数映射
    #define platform_close_socket close
    #define platform_read read
    #define platform_write write
    #define platform_mkdir mkdir
    #define platform_stat stat
    #define platform_stat_struct struct stat
    
    // 初始化网络（Linux 不需要）
    inline bool platform_init_network() {
        return true;
    }
    
    inline void platform_cleanup_network() {
        // Linux 不需要清理
    }
    
    // 获取最后的 socket 错误
    inline int platform_socket_error() {
        return errno;
    }
    
    // 设置 socket 选项
    inline int platform_setsockopt(socket_t s, int level, int optname, const void* optval, socklen_t optlen) {
        return setsockopt(s, level, optname, optval, optlen);
    }
    
#endif

// 跨平台目录操作
#ifdef _WIN32
    #include <filesystem>
    namespace fs = std::filesystem;
    
    // Windows 目录遍历
    inline bool platform_list_directory(const std::string& path, 
                                        std::vector<std::string>& entries,
                                        bool includeHidden = false) {
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                std::string name = entry.path().filename().string();
                if (!includeHidden && name[0] == '.') continue;
                entries.push_back(name);
            }
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Windows 递归删除目录
    inline bool platform_remove_directory(const std::string& path) {
        try {
            fs::remove_all(path);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Windows 检查目录是否存在
    inline bool platform_directory_exists(const std::string& path) {
        return fs::exists(path) && fs::is_directory(path);
    }
    
    // Windows 创建目录（递归）
    inline bool platform_create_directories(const std::string& path) {
        try {
            fs::create_directories(path);
            return true;
        } catch (...) {
            return false;
        }
    }
    
#else
    // Linux 目录遍历
    inline bool platform_list_directory(const std::string& path, 
                                        std::vector<std::string>& entries,
                                        bool includeHidden = false) {
        DIR* dir = opendir(path.c_str());
        if (!dir) return false;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (!includeHidden && name[0] == '.') continue;
            if (name == "." || name == "..") continue;
            entries.push_back(name);
        }
        closedir(dir);
        return true;
    }
    
    // Linux 递归删除目录
    inline bool platform_remove_directory(const std::string& path) {
        std::string command = "rm -rf \"" + path + "\"";
        return system(command.c_str()) == 0;
    }
    
    // Linux 检查目录是否存在
    inline bool platform_directory_exists(const std::string& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    
    // Linux 创建目录（递归）
    inline bool platform_create_directories(const std::string& path) {
        std::string command = "mkdir -p \"" + path + "\"";
        return system(command.c_str()) == 0;
    }
#endif

// 跨平台睡眠
#ifdef _WIN32
    #define platform_sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define platform_sleep_ms(ms) usleep((ms) * 1000)
#endif

#endif // PLATFORM_H
