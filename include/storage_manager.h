#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <string>
#include <vector>
#include "config_manager.h"
#include "html_parser.h"

struct TaskConfig {
    std::string taskId;
    std::string taskName;                 // 任务名称（可选，支持中文）
    std::string fileName;
    std::vector<std::string> fileNames;  // 多文件支持
    bool translateTitle;
    bool translateAbstract;
    ModelConfig modelConfig;
    int totalCount;
    int completedCount;
    int failedCount;
    std::string status;
    std::string createdAt;
    std::string updatedAt;
    bool deleted = false;                 // 软删除标志
};

struct LiteratureData {
    int index;
    int recordNumber;
    int totalRecords;
    
    // 来源文件信息（多文件支持）
    std::string sourceFileName;   // 来源文件名
    int sourceFileIndex;          // 来源文件序号（从1开始）
    int indexInFile;              // 在原文件中的序号
    
    // 原文
    std::string originalTitle;
    std::string originalAbstract;
    
    // 译文
    std::string translatedTitle;
    std::string translatedAbstract;
    
    // 扩展字段
    std::string authors;
    std::string source;
    std::string volume;
    std::string issue;
    std::string pages;
    std::string doi;
    std::string earlyAccessDate;
    std::string publishedDate;
    std::string accessionNumber;
    std::string issn;
    std::string eissn;
    
    // 状态
    std::string status;
    std::string errorMessage;
};

class StorageManager {
public:
    static StorageManager& getInstance();
    
    bool createTaskDirectory(const std::string& taskId);
    bool saveTaskConfig(const TaskConfig& config);
    TaskConfig loadTaskConfig(const std::string& taskId);
    
    bool saveOriginalHtml(const std::string& taskId, const std::string& content);
    std::string loadOriginalHtml(const std::string& taskId);
    
    bool saveLiteratureData(const std::string& taskId, int index, const LiteratureData& data);
    LiteratureData loadLiteratureData(const std::string& taskId, int index);
    
    bool saveTranslatedHtml(const std::string& taskId, const std::string& content);
    std::string loadTranslatedHtml(const std::string& taskId);
    
    bool saveIndexJson(const std::string& taskId, const std::vector<int>& indices);
    std::vector<int> loadIndexJson(const std::string& taskId);
    
    // 软删除
    bool softDeleteTask(const std::string& taskId);
    
    // 永久删除（真正删除文件）
    bool permanentDeleteTask(const std::string& taskId);
    
    // 获取已删除的任务列表
    std::vector<std::string> listDeletedTasks();
    
    // 永久删除所有已软删除的任务
    int permanentDeleteAllDeleted();
    
    // 获取存储使用情况（data文件夹大小，单位字节）
    uint64_t getStorageUsage();
    
    // 格式化存储大小
    static std::string formatStorageSize(uint64_t bytes);
    
    bool deleteTask(const std::string& taskId);
    
private:
    StorageManager() = default;
    ~StorageManager() = default;
    
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;
    
    std::string getTaskPath(const std::string& taskId);
};

#endif // STORAGE_MANAGER_H
