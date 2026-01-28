#include "logger.h"
#include "config_manager.h"
#include "web_server.h"
#include "task_queue.h"
#include <iostream>
#include <csignal>

#ifdef _WIN32
    #include <windows.h>
#endif

// 全局Web服务器指针，用于信号处理
WebServer* g_server = nullptr;

#ifdef _WIN32
// Windows 控制台事件处理
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        Logger::getInstance().info("Received shutdown signal");
        
        // 停止任务队列
        TaskQueue::getInstance().stop();
        
        if (g_server) {
            g_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Logger::getInstance().info("Received shutdown signal");
        
        // 停止任务队列
        TaskQueue::getInstance().stop();
        
        if (g_server) {
            g_server->stop();
        }
        exit(0);
    }
}
#endif

int main(int argc, char* argv[]) {
    (void)argc;  // 避免未使用参数警告
    (void)argv;
    
    // 设置信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#endif
    
    // 初始化日志系统
    Logger::getInstance().setLogLevel(LogLevel::Info);
    Logger::getInstance().info("=== WoS Translator Starting ===");
    
    // 初始化配置管理器
    ConfigManager::getInstance().initialize();
    
    // 加载系统配置
    SystemConfig config = ConfigManager::getInstance().getSystemConfig();
    
    // 设置日志级别（从配置读取）
    Logger::getInstance().setLogLevel(config.logLevel);
    Logger::getInstance().info("Log level set to: " + config.logLevel);
    
    try {
        // 启动任务队列
        TaskQueue::getInstance().start();
        
        // 创建并启动Web服务器
        WebServer server(config.serverPort);
        g_server = &server;
        
        Logger::getInstance().info("Starting web server on port " + std::to_string(config.serverPort));
        server.start();
        
        // 服务器运行在独立线程中，主线程等待
        Logger::getInstance().info("Server is running. Press Ctrl+C to stop.");
        Logger::getInstance().info("Access the web interface at: http://localhost:" + std::to_string(config.serverPort));
        
        // 保持主线程运行
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Fatal error: " + std::string(e.what()));
        return 1;
    }
    
    Logger::getInstance().info("=== WoS Translator Stopped ===");
    return 0;
}
