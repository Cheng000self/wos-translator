#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>
#include <set>
#include "storage_manager.h"
#include "translator.h"

enum class TaskStatus {
    Parsing,
    Pending,
    Running,
    Paused,
    Completed,
    Failed
};

struct TaskInfo {
    std::string taskId;
    std::string taskName;    // 任务名称
    std::string fileName;
    std::string modelName;   // 翻译模型名称
    TaskStatus status;
    int totalCount;
    int completedCount;
    int failedCount;
    std::string createdAt;
    std::string updatedAt;
    bool deleted = false;
};

class TaskQueue {
public:
    static TaskQueue& getInstance();
    
    std::string createTask(const std::string& fileName,
                          const std::string& htmlContent,
                          const TaskConfig& config);
    
    // 多文件创建任务
    std::string createTaskMultiFile(const std::vector<std::string>& fileNames,
                                    const std::vector<std::string>& htmlContents,
                                    const TaskConfig& config);
    
    std::vector<TaskInfo> listTasks(bool includeDeleted = false);
    TaskInfo getTaskInfo(const std::string& taskId);
    std::vector<LiteratureData> getTaskLiteratures(const std::string& taskId);
    
    std::string getOriginalHtml(const std::string& taskId);
    std::string getTranslatedHtml(const std::string& taskId);
    
    bool pauseTask(const std::string& taskId);
    bool resumeTask(const std::string& taskId);
    bool deleteTask(const std::string& taskId);  // 软删除
    
    void start();
    void stop();
    
private:
    TaskQueue();
    ~TaskQueue();
    
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    
    void schedulerLoop();  // 调度器循环
    void executeTask(const std::string& taskId, const std::string& modelId);  // 执行单个任务
    void parseAndSaveTask(const std::string& taskId, const std::string& htmlContent);
    void parseAndSaveTaskMultiFile(const std::string& taskId, 
                                   const std::vector<std::string>& htmlContents);
    void translateTask(const std::string& taskId);
    void translateTaskMultiThread(const std::string& taskId, int numThreads);
    void rebuildTranslatedHtml(const std::string& taskId);
    
    // 模型并发控制
    bool canStartTask(const std::string& modelId);
    void onTaskStarted(const std::string& taskId, const std::string& modelId);
    void onTaskFinished(const std::string& taskId, const std::string& modelId);
    int getTotalRunningTasks();
    
    std::string generateTaskId();
    
    std::thread schedulerThread_;  // 调度器线程
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    
    // 模型并发跟踪: modelId -> 正在运行的任务数
    std::map<std::string, int> modelRunningTasks_;
    std::mutex modelMutex_;
    
    // 当前运行的任务及其模型ID
    std::map<std::string, std::string> runningTaskModels_;
    
    // 正在执行的任务线程
    std::map<std::string, std::thread> taskThreads_;
    std::mutex taskThreadsMutex_;
    
    // 已经在调度中的任务（防止重复调度）
    std::set<std::string> scheduledTasks_;
    std::mutex scheduledMutex_;
};

#endif // TASK_QUEUE_H
