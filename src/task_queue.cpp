#include "task_queue.h"
#include "logger.h"
#include "html_parser.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <future>
#include <atomic>
#include <queue>

TaskQueue& TaskQueue::getInstance() {
    static TaskQueue instance;
    return instance;
}

TaskQueue::TaskQueue() : running_(false) {
}

TaskQueue::~TaskQueue() {
    stop();
}

void TaskQueue::start() {
    if (!running_.load()) {
        running_.store(true);
        schedulerThread_ = std::thread(&TaskQueue::schedulerLoop, this);
        Logger::getInstance().info("TaskQueue started");
    }
}

void TaskQueue::stop() {
    if (running_.load()) {
        running_.store(false);
        cv_.notify_all();
        
        // 等待调度器线程结束
        if (schedulerThread_.joinable()) {
            schedulerThread_.join();
        }
        
        // 等待所有任务线程结束
        {
            std::lock_guard<std::mutex> lock(taskThreadsMutex_);
            for (auto& pair : taskThreads_) {
                if (pair.second.joinable()) {
                    pair.second.join();
                }
            }
            taskThreads_.clear();
        }
        
        Logger::getInstance().info("TaskQueue stopped");
    }
}

std::string TaskQueue::createTask(const std::string& fileName,
                                  const std::string& htmlContent,
                                  const TaskConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 生成任务ID
        std::string taskId = generateTaskId();
        
        // 创建任务目录
        if (!StorageManager::getInstance().createTaskDirectory(taskId)) {
            Logger::getInstance().error("Failed to create task directory");
            return "";
        }
        
        // 保存原始HTML
        if (!StorageManager::getInstance().saveOriginalHtml(taskId, htmlContent)) {
            Logger::getInstance().error("Failed to save original HTML");
            return "";
        }
        
        // 创建任务配置
        TaskConfig taskConfig = config;
        taskConfig.taskId = taskId;
        taskConfig.fileName = fileName;
        taskConfig.status = "parsing";
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        taskConfig.createdAt = oss.str();
        taskConfig.updatedAt = oss.str();
        
        // 保存配置
        if (!StorageManager::getInstance().saveTaskConfig(taskConfig)) {
            Logger::getInstance().error("Failed to save task config");
            return "";
        }
        
        // 解析并保存任务
        parseAndSaveTask(taskId, htmlContent);
        
        // 通知工作线程
        cv_.notify_one();
        
        Logger::getInstance().info("Task created: " + taskId);
        return taskId;
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create task: " + std::string(e.what()));
        return "";
    }
}

std::vector<TaskInfo> TaskQueue::listTasks(bool includeDeleted) {
    std::vector<TaskInfo> tasks;
    
    try {
        // 遍历data目录
        DIR* dir = opendir("data");
        if (!dir) {
            return tasks;
        }
        
        struct dirent* dateEntry;
        while ((dateEntry = readdir(dir)) != nullptr) {
            if (dateEntry->d_name[0] == '.') continue;
            
            std::string datePath = std::string("data/") + dateEntry->d_name;
            DIR* dateDir = opendir(datePath.c_str());
            if (!dateDir) continue;
            
            struct dirent* taskEntry;
            while ((taskEntry = readdir(dateDir)) != nullptr) {
                if (taskEntry->d_name[0] == '.') continue;
                
                std::string taskId = std::string(dateEntry->d_name) + "/" + taskEntry->d_name;
                TaskInfo info = getTaskInfo(taskId);
                if (!info.taskId.empty()) {
                    // 根据includeDeleted参数决定是否包含已删除的任务
                    if (includeDeleted || !info.deleted) {
                        tasks.push_back(info);
                    }
                }
            }
            closedir(dateDir);
        }
        closedir(dir);
        
        // 按创建时间排序（最新的在前，用于显示）
        std::sort(tasks.begin(), tasks.end(), 
            [](const TaskInfo& a, const TaskInfo& b) {
                return a.createdAt > b.createdAt;
            });
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to list tasks: " + std::string(e.what()));
    }
    
    return tasks;
}

TaskInfo TaskQueue::getTaskInfo(const std::string& taskId) {
    TaskInfo info;
    
    try {
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        
        info.taskId = config.taskId;
        info.taskName = config.taskName;
        info.fileName = config.fileName;
        info.totalCount = config.totalCount;
        info.completedCount = config.completedCount;
        info.failedCount = config.failedCount;
        info.createdAt = config.createdAt;
        info.updatedAt = config.updatedAt;
        info.deleted = config.deleted;
        
        // 获取模型名称
        if (!config.modelConfigs.empty()) {
            // 多模型任务
            info.modelCount = config.modelConfigs.size();
            if (config.modelConfigs.size() == 1) {
                info.modelName = config.modelConfigs[0].model.name;
            } else {
                info.modelName = std::to_string(config.modelConfigs.size()) + "个模型";
            }
        } else if (!config.modelConfig.modelId.empty()) {
            // 兼容旧的单模型任务
            info.modelCount = 1;
            ModelConfig savedModel = ConfigManager::getInstance().getModelConfig(config.modelConfig.modelId);
            if (!savedModel.name.empty()) {
                info.modelName = savedModel.name;
            } else {
                info.modelName = config.modelConfig.modelId;
            }
        }
        
        // 转换状态
        if (config.status == "parsing") {
            info.status = TaskStatus::Parsing;
        } else if (config.status == "pending") {
            info.status = TaskStatus::Pending;
        } else if (config.status == "running") {
            info.status = TaskStatus::Running;
        } else if (config.status == "paused") {
            info.status = TaskStatus::Paused;
        } else if (config.status == "completed") {
            info.status = TaskStatus::Completed;
        } else if (config.status == "failed") {
            info.status = TaskStatus::Failed;
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to get task info: " + std::string(e.what()));
    }
    
    return info;
}

std::vector<LiteratureData> TaskQueue::getTaskLiteratures(const std::string& taskId) {
    std::vector<LiteratureData> literatures;
    
    try {
        std::vector<int> indices = StorageManager::getInstance().loadIndexJson(taskId);
        
        for (int index : indices) {
            LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
            literatures.push_back(data);
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to get task literatures: " + std::string(e.what()));
    }
    
    return literatures;
}

std::string TaskQueue::getOriginalHtml(const std::string& taskId) {
    return StorageManager::getInstance().loadOriginalHtml(taskId);
}

std::string TaskQueue::getTranslatedHtml(const std::string& taskId) {
    return StorageManager::getInstance().loadTranslatedHtml(taskId);
}

bool TaskQueue::pauseTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        
        if (config.status == "running") {
            config.status = "paused";
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            config.updatedAt = oss.str();
            
            StorageManager::getInstance().saveTaskConfig(config);
            Logger::getInstance().info("Task paused: " + taskId);
            return true;
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to pause task: " + std::string(e.what()));
    }
    
    return false;
}

bool TaskQueue::resumeTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        
        if (config.status == "paused") {
            config.status = "pending";
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            config.updatedAt = oss.str();
            
            StorageManager::getInstance().saveTaskConfig(config);
            cv_.notify_one();
            Logger::getInstance().info("Task resumed: " + taskId);
            return true;
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to resume task: " + std::string(e.what()));
    }
    
    return false;
}

bool TaskQueue::deleteTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 使用软删除
    return StorageManager::getInstance().softDeleteTask(taskId);
}

// 调度器循环 - 负责调度任务到独立线程执行
void TaskQueue::schedulerLoop() {
    Logger::getInstance().info("TaskQueue scheduler thread started");
    
    while (running_.load()) {
        try {
            // 清理已完成的任务线程
            {
                std::lock_guard<std::mutex> lock(taskThreadsMutex_);
                for (auto it = taskThreads_.begin(); it != taskThreads_.end();) {
                    // 检查线程是否可以join（已完成）
                    if (it->second.joinable()) {
                        // 检查任务是否已完成
                        TaskInfo info = getTaskInfo(it->first);
                        if (info.status != TaskStatus::Running) {
                            it->second.join();
                            
                            // 从已调度集合中移除
                            {
                                std::lock_guard<std::mutex> slock(scheduledMutex_);
                                scheduledTasks_.erase(it->first);
                            }
                            
                            it = taskThreads_.erase(it);
                            continue;
                        }
                    }
                    ++it;
                }
            }
            
            // 获取系统配置
            int maxConcurrent = ConfigManager::getInstance().loadSystemConfig().maxConcurrentTasks;
            int currentRunning = getTotalRunningTasks();
            
            // 如果还有并发余量，尝试调度新任务
            if (currentRunning < maxConcurrent) {
                std::unique_lock<std::mutex> lock(mutex_);
                
                // 查找待处理的任务
                std::vector<TaskInfo> tasks = listTasks(false);
                
                // 按创建时间升序排序（FIFO - 先进先出，最早创建的优先调度）
                std::sort(tasks.begin(), tasks.end(), 
                    [](const TaskInfo& a, const TaskInfo& b) {
                        return a.createdAt < b.createdAt;
                    });
                
                for (const auto& taskInfo : tasks) {
                    if (taskInfo.status == TaskStatus::Pending) {
                        // 检查是否已经在调度中
                        {
                            std::lock_guard<std::mutex> slock(scheduledMutex_);
                            if (scheduledTasks_.find(taskInfo.taskId) != scheduledTasks_.end()) {
                                continue;  // 已经在调度中，跳过
                            }
                        }
                        
                        // 获取任务配置以检查模型
                        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskInfo.taskId);
                        std::string modelId = config.modelConfig.modelId;
                        
                        // 检查是否可以启动该任务（模型并发限制）
                        if (!canStartTask(modelId)) {
                            continue;  // 该模型已达到并发限制，跳过
                        }
                        
                        // 检查总并发数
                        if (getTotalRunningTasks() >= maxConcurrent) {
                            break;  // 已达到最大并发数
                        }
                        
                        std::string taskId = taskInfo.taskId;
                        
                        // 标记为已调度
                        {
                            std::lock_guard<std::mutex> slock(scheduledMutex_);
                            scheduledTasks_.insert(taskId);
                        }
                        
                        // 记录任务启动
                        onTaskStarted(taskId, modelId);
                        
                        // 在独立线程中执行任务
                        {
                            std::lock_guard<std::mutex> tlock(taskThreadsMutex_);
                            taskThreads_[taskId] = std::thread(&TaskQueue::executeTask, this, taskId, modelId);
                        }
                        
                        Logger::getInstance().info("Scheduled task: " + taskId + " (model: " + modelId + 
                                                   ", running: " + std::to_string(getTotalRunningTasks()) + "/" + 
                                                   std::to_string(maxConcurrent) + ")");
                    }
                }
            }
            
            // 短暂等待后继续检查
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
        } catch (const std::exception& e) {
            Logger::getInstance().error("Error in schedulerLoop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    Logger::getInstance().info("TaskQueue scheduler thread stopped");
}

// 执行单个任务（在独立线程中运行）
void TaskQueue::executeTask(const std::string& taskId, const std::string& modelId) {
    try {
        Logger::getInstance().info("Executing task in thread: " + taskId);
        
        // 加载任务配置
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        
        // 判断是否为多模型任务
        if (!config.modelConfigs.empty()) {
            // 多模型：使用连续调度
            translateTaskContinuous(taskId);
        } else {
            // 单模型：使用原有逻辑
            int numThreads = ConfigManager::getInstance().loadSystemConfig().maxTranslationThreads;
            if (numThreads > 1) {
                translateTaskMultiThread(taskId, numThreads);
            } else {
                translateTask(taskId);
            }
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Error executing task " + taskId + ": " + std::string(e.what()));
        
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "failed";
        StorageManager::getInstance().saveTaskConfig(config);
    }
    
    // 记录任务完成
    {
        std::lock_guard<std::mutex> lock(modelMutex_);
        onTaskFinished(taskId, modelId);
    }
    
    Logger::getInstance().info("Task thread finished: " + taskId);
}

void TaskQueue::parseAndSaveTask(const std::string& taskId, const std::string& htmlContent) {
    try {
        Logger::getInstance().info("Parsing task: " + taskId);
        
        // 加载任务配置获取文件名
        TaskConfig taskConfig = StorageManager::getInstance().loadTaskConfig(taskId);
        std::string fileName = taskConfig.fileName;
        
        // 解析HTML
        HTMLParser parser;
        std::vector<Literature> literatures = parser.parse(htmlContent);
        
        if (literatures.empty()) {
            Logger::getInstance().error("No literatures found in HTML");
            
            TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
            config.status = "failed";
            StorageManager::getInstance().saveTaskConfig(config);
            return;
        }
        
        // 保存文献数据
        std::vector<int> indices;
        for (size_t i = 0; i < literatures.size(); i++) {
            const auto& lit = literatures[i];
            
            LiteratureData data;
            data.index = i + 1;
            data.recordNumber = i + 1;  // 全局序号
            data.totalRecords = literatures.size();
            data.sourceFileName = fileName;
            data.sourceFileIndex = 1;
            data.indexInFile = lit.recordNumber;  // 原文件中的序号
            data.originalTitle = lit.title;
            data.originalAbstract = lit.abstract;
            data.authors = lit.authors;
            data.source = lit.source;
            data.volume = lit.volume;
            data.issue = lit.issue;
            data.pages = lit.pages;
            data.doi = lit.doi;
            data.earlyAccessDate = lit.earlyAccessDate;
            data.publishedDate = lit.publishedDate;
            data.accessionNumber = lit.accessionNumber;
            data.issn = lit.issn;
            data.eissn = lit.eissn;
            data.status = "pending";
            
            StorageManager::getInstance().saveLiteratureData(taskId, data.index, data);
            indices.push_back(data.index);
        }
        
        // 保存索引
        StorageManager::getInstance().saveIndexJson(taskId, indices);
        
        // 更新任务配置
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.totalCount = literatures.size();
        config.completedCount = 0;
        config.failedCount = 0;
        config.status = "pending";
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        config.updatedAt = oss.str();
        
        StorageManager::getInstance().saveTaskConfig(config);
        
        Logger::getInstance().info("Task parsed successfully: " + std::to_string(literatures.size()) + " literatures");
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to parse task: " + std::string(e.what()));
        
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "failed";
        StorageManager::getInstance().saveTaskConfig(config);
    }
}

void TaskQueue::translateTask(const std::string& taskId) {
    try {
        Logger::getInstance().info("Translating task: " + taskId);
        
        // 加载任务配置
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "running";
        StorageManager::getInstance().saveTaskConfig(config);
        
        // 创建翻译器
        Translator translator(config.modelConfig);
        
        // 加载文献索引
        std::vector<int> indices = StorageManager::getInstance().loadIndexJson(taskId);
        
        int consecutiveFailures = 0;
        int maxConsecutiveFailures = ConfigManager::getInstance().loadSystemConfig().consecutiveFailureThreshold;
        
        // 翻译每篇文献
        for (int index : indices) {
            // 检查是否被暂停
            TaskConfig currentConfig = StorageManager::getInstance().loadTaskConfig(taskId);
            if (currentConfig.status == "paused") {
                Logger::getInstance().info("Task paused during translation: " + taskId);
                return;
            }
            
            LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
            
            // 跳过已完成的文献
            if (data.status == "completed") {
                continue;
            }
            
            data.status = "translating";
            StorageManager::getInstance().saveLiteratureData(taskId, index, data);
            
            bool success = true;
            
            // 翻译标题
            if (config.translateTitle && !data.originalTitle.empty()) {
                TranslationResult result = translator.translate(data.originalTitle, "标题");
                if (result.success) {
                    data.translatedTitle = result.translatedText;
                } else {
                    success = false;
                    data.errorMessage = "Title translation failed: " + result.errorMessage;
                }
            }
            
            // 翻译摘要
            if (success && config.translateAbstract && !data.originalAbstract.empty()) {
                TranslationResult result = translator.translate(data.originalAbstract, "摘要");
                if (result.success) {
                    data.translatedAbstract = result.translatedText;
                } else {
                    success = false;
                    data.errorMessage = "Abstract translation failed: " + result.errorMessage;
                }
            }
            
            // 更新文献状态
            if (success) {
                data.status = "completed";
                data.errorMessage = "";
                config.completedCount++;
                consecutiveFailures = 0;
            } else {
                data.status = "failed";
                config.failedCount++;
                consecutiveFailures++;
            }
            
            StorageManager::getInstance().saveLiteratureData(taskId, index, data);
            
            // 更新任务进度 - 先检查是否被暂停
            TaskConfig latestConfig = StorageManager::getInstance().loadTaskConfig(taskId);
            if (latestConfig.status == "paused") {
                // 任务被暂停，保持暂停状态，只更新进度
                latestConfig.completedCount = config.completedCount;
                latestConfig.failedCount = config.failedCount;
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                std::tm tm = *std::gmtime(&time);
                std::ostringstream oss;
                oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                latestConfig.updatedAt = oss.str();
                StorageManager::getInstance().saveTaskConfig(latestConfig);
                Logger::getInstance().info("Task paused, stopping translation: " + taskId);
                return;
            }
            
            // 任务未被暂停，正常更新
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            config.updatedAt = oss.str();
            StorageManager::getInstance().saveTaskConfig(config);
            
            // 检查连续失败
            if (consecutiveFailures >= maxConsecutiveFailures) {
                Logger::getInstance().error("Too many consecutive failures, pausing task: " + taskId);
                config.status = "paused";
                StorageManager::getInstance().saveTaskConfig(config);
                return;
            }
        }
        
        // 所有文献处理完成
        config.status = "completed";
        StorageManager::getInstance().saveTaskConfig(config);
        
        // 重建翻译后的HTML
        rebuildTranslatedHtml(taskId);
        
        Logger::getInstance().info("Task translation completed: " + taskId);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to translate task: " + std::string(e.what()));
        
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "failed";
        StorageManager::getInstance().saveTaskConfig(config);
    }
}

void TaskQueue::rebuildTranslatedHtml(const std::string& taskId) {
    try {
        Logger::getInstance().info("Rebuilding translated HTML: " + taskId);
        
        // 加载原始HTML
        std::string originalHtml = StorageManager::getInstance().loadOriginalHtml(taskId);
        
        // 加载所有文献数据
        std::vector<int> indices = StorageManager::getInstance().loadIndexJson(taskId);
        
        std::string translatedHtml = originalHtml;
        
        // 简化版：在原始HTML后添加翻译内容
        // 完整版应该在原始位置插入翻译
        translatedHtml += "\n\n<!-- Translated Content -->\n";
        
        for (int index : indices) {
            LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
            
            if (data.status == "completed") {
                translatedHtml += "<hr>\n<h3>文献 " + std::to_string(data.recordNumber) + " 译文</h3>\n";
                
                if (!data.translatedTitle.empty()) {
                    translatedHtml += "<p><strong>标题：</strong>" + data.translatedTitle + "</p>\n";
                }
                
                if (!data.translatedAbstract.empty()) {
                    translatedHtml += "<p><strong>摘要：</strong>" + data.translatedAbstract + "</p>\n";
                }
            }
        }
        
        StorageManager::getInstance().saveTranslatedHtml(taskId, translatedHtml);
        
        Logger::getInstance().info("Translated HTML rebuilt successfully");
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to rebuild translated HTML: " + std::string(e.what()));
    }
}

std::string TaskQueue::generateTaskId() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    
    std::ostringstream dateOss;
    dateOss << std::put_time(&tm, "%Y-%m-%d");
    std::string dateStr = dateOss.str();
    
    // 查找当天已有的任务数量
    std::string datePath = "data/" + dateStr;
    int maxNum = 0;
    
    DIR* dir = opendir(datePath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            try {
                int num = std::stoi(entry->d_name);
                if (num > maxNum) {
                    maxNum = num;
                }
            } catch (...) {
                // 忽略非数字目录
            }
        }
        closedir(dir);
    }
    
    // 生成新的序号
    int newNum = maxNum + 1;
    std::ostringstream numOss;
    numOss << std::setfill('0') << std::setw(4) << newNum;
    
    return dateStr + "/" + numOss.str();
}

// 多文件创建任务
std::string TaskQueue::createTaskMultiFile(const std::vector<std::string>& fileNames,
                                           const std::vector<std::string>& htmlContents,
                                           const TaskConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 生成任务ID
        std::string taskId = generateTaskId();
        
        // 创建任务目录
        if (!StorageManager::getInstance().createTaskDirectory(taskId)) {
            Logger::getInstance().error("Failed to create task directory");
            return "";
        }
        
        // 合并所有HTML内容
        std::string combinedHtml;
        for (size_t i = 0; i < htmlContents.size(); i++) {
            if (i > 0) {
                combinedHtml += "\n<!-- File: " + fileNames[i] + " -->\n";
            }
            combinedHtml += htmlContents[i];
        }
        
        // 保存合并后的HTML
        if (!StorageManager::getInstance().saveOriginalHtml(taskId, combinedHtml)) {
            Logger::getInstance().error("Failed to save original HTML");
            return "";
        }
        
        // 创建任务配置
        TaskConfig taskConfig = config;
        taskConfig.taskId = taskId;
        taskConfig.fileName = fileNames.empty() ? "" : fileNames[0];
        taskConfig.fileNames = fileNames;
        taskConfig.status = "parsing";
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        taskConfig.createdAt = oss.str();
        taskConfig.updatedAt = oss.str();
        
        // 保存配置
        if (!StorageManager::getInstance().saveTaskConfig(taskConfig)) {
            Logger::getInstance().error("Failed to save task config");
            return "";
        }
        
        // 解析并保存任务（多文件）
        parseAndSaveTaskMultiFile(taskId, htmlContents);
        
        // 通知工作线程
        cv_.notify_one();
        
        Logger::getInstance().info("Multi-file task created: " + taskId);
        return taskId;
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create multi-file task: " + std::string(e.what()));
        return "";
    }
}

void TaskQueue::parseAndSaveTaskMultiFile(const std::string& taskId, 
                                          const std::vector<std::string>& htmlContents) {
    try {
        Logger::getInstance().info("Parsing multi-file task: " + taskId);
        
        // 加载任务配置获取文件名列表
        TaskConfig taskConfig = StorageManager::getInstance().loadTaskConfig(taskId);
        std::vector<std::string> fileNames = taskConfig.fileNames;
        
        HTMLParser parser;
        
        // 存储每个文件的文献及其来源信息
        struct LiteratureWithSource {
            Literature lit;
            std::string sourceFileName;
            int sourceFileIndex;
            int indexInFile;
        };
        std::vector<LiteratureWithSource> allLiteratures;
        
        // 解析所有HTML文件，记录来源信息
        for (size_t fileIdx = 0; fileIdx < htmlContents.size(); fileIdx++) {
            std::vector<Literature> literatures = parser.parse(htmlContents[fileIdx]);
            
            std::string fileName = (fileIdx < fileNames.size()) ? fileNames[fileIdx] : ("file_" + std::to_string(fileIdx + 1));
            
            for (size_t litIdx = 0; litIdx < literatures.size(); litIdx++) {
                LiteratureWithSource lws;
                lws.lit = literatures[litIdx];
                lws.sourceFileName = fileName;
                lws.sourceFileIndex = fileIdx + 1;  // 从1开始
                lws.indexInFile = litIdx + 1;       // 在原文件中的序号，从1开始
                allLiteratures.push_back(lws);
            }
        }
        
        if (allLiteratures.empty()) {
            Logger::getInstance().error("No literatures found in HTML files");
            
            TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
            config.status = "failed";
            StorageManager::getInstance().saveTaskConfig(config);
            return;
        }
        
        // 保存文献数据
        std::vector<int> indices;
        for (size_t i = 0; i < allLiteratures.size(); i++) {
            const auto& lws = allLiteratures[i];
            const auto& lit = lws.lit;
            
            LiteratureData data;
            data.index = i + 1;
            data.recordNumber = i + 1;  // 全局序号
            data.totalRecords = allLiteratures.size();
            data.sourceFileName = lws.sourceFileName;
            data.sourceFileIndex = lws.sourceFileIndex;
            data.indexInFile = lws.indexInFile;
            data.originalTitle = lit.title;
            data.originalAbstract = lit.abstract;
            data.authors = lit.authors;
            data.source = lit.source;
            data.volume = lit.volume;
            data.issue = lit.issue;
            data.pages = lit.pages;
            data.doi = lit.doi;
            data.earlyAccessDate = lit.earlyAccessDate;
            data.publishedDate = lit.publishedDate;
            data.accessionNumber = lit.accessionNumber;
            data.issn = lit.issn;
            data.eissn = lit.eissn;
            data.status = "pending";
            
            StorageManager::getInstance().saveLiteratureData(taskId, data.index, data);
            indices.push_back(data.index);
        }
        
        // 保存索引
        StorageManager::getInstance().saveIndexJson(taskId, indices);
        
        // 更新任务配置
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.totalCount = allLiteratures.size();
        config.completedCount = 0;
        config.failedCount = 0;
        config.status = "pending";
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        config.updatedAt = oss.str();
        
        StorageManager::getInstance().saveTaskConfig(config);
        
        Logger::getInstance().info("Multi-file task parsed successfully: " + std::to_string(allLiteratures.size()) + " literatures");
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to parse multi-file task: " + std::string(e.what()));
        
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "failed";
        StorageManager::getInstance().saveTaskConfig(config);
    }
}

// 多线程翻译
void TaskQueue::translateTaskMultiThread(const std::string& taskId, int numThreads) {
    try {
        Logger::getInstance().info("Translating task with " + std::to_string(numThreads) + " threads: " + taskId);
        
        // 加载任务配置
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "running";
        StorageManager::getInstance().saveTaskConfig(config);
        
        // 加载文献索引
        std::vector<int> indices = StorageManager::getInstance().loadIndexJson(taskId);
        
        // 过滤出待翻译的文献
        std::vector<int> pendingIndices;
        for (int index : indices) {
            LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
            if (data.status != "completed") {
                pendingIndices.push_back(index);
            }
        }
        
        if (pendingIndices.empty()) {
            config.status = "completed";
            StorageManager::getInstance().saveTaskConfig(config);
            rebuildTranslatedHtml(taskId);
            return;
        }
        
        // 共享状态
        std::atomic<int> completedCount(config.completedCount);
        std::atomic<int> failedCount(config.failedCount);
        std::atomic<int> consecutiveFailures(0);
        std::atomic<bool> shouldStop(false);
        std::mutex progressMutex;
        
        int maxConsecutiveFailures = ConfigManager::getInstance().loadSystemConfig().consecutiveFailureThreshold;
        
        // 创建工作函数
        auto translateWorker = [&](int startIdx, int endIdx) {
            // 每个线程创建自己的翻译器
            Translator translator(config.modelConfig);
            
            for (int i = startIdx; i < endIdx && !shouldStop.load(); i++) {
                int index = pendingIndices[i];
                
                // 检查是否被暂停
                TaskConfig currentConfig = StorageManager::getInstance().loadTaskConfig(taskId);
                if (currentConfig.status == "paused") {
                    shouldStop.store(true);
                    return;
                }
                
                LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
                
                // 跳过已完成的文献
                if (data.status == "completed") {
                    continue;
                }
                
                data.status = "translating";
                StorageManager::getInstance().saveLiteratureData(taskId, index, data);
                
                bool success = true;
                
                // 翻译标题
                if (config.translateTitle && !data.originalTitle.empty()) {
                    TranslationResult result = translator.translate(data.originalTitle, "标题");
                    if (result.success) {
                        data.translatedTitle = result.translatedText;
                    } else {
                        success = false;
                        data.errorMessage = "Title translation failed: " + result.errorMessage;
                    }
                }
                
                // 翻译摘要
                if (success && config.translateAbstract && !data.originalAbstract.empty()) {
                    TranslationResult result = translator.translate(data.originalAbstract, "摘要");
                    if (result.success) {
                        data.translatedAbstract = result.translatedText;
                    } else {
                        success = false;
                        data.errorMessage = "Abstract translation failed: " + result.errorMessage;
                    }
                }
                
                // 更新文献状态
                if (success) {
                    data.status = "completed";
                    data.errorMessage = "";
                    completedCount.fetch_add(1);
                    consecutiveFailures.store(0);
                } else {
                    data.status = "failed";
                    failedCount.fetch_add(1);
                    int failures = consecutiveFailures.fetch_add(1) + 1;
                    
                    if (failures >= maxConsecutiveFailures) {
                        shouldStop.store(true);
                    }
                }
                
                StorageManager::getInstance().saveLiteratureData(taskId, index, data);
                
                // 定期更新任务进度
                {
                    std::lock_guard<std::mutex> lock(progressMutex);
                    TaskConfig latestConfig = StorageManager::getInstance().loadTaskConfig(taskId);
                    if (latestConfig.status == "paused") {
                        shouldStop.store(true);
                    } else {
                        latestConfig.completedCount = completedCount.load();
                        latestConfig.failedCount = failedCount.load();
                        
                        auto now = std::chrono::system_clock::now();
                        auto time = std::chrono::system_clock::to_time_t(now);
                        std::tm tm = *std::gmtime(&time);
                        std::ostringstream oss;
                        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                        latestConfig.updatedAt = oss.str();
                        
                        StorageManager::getInstance().saveTaskConfig(latestConfig);
                    }
                }
            }
        };
        
        // 分配工作给线程
        int totalPending = pendingIndices.size();
        int chunkSize = (totalPending + numThreads - 1) / numThreads;
        
        std::vector<std::future<void>> futures;
        for (int t = 0; t < numThreads; t++) {
            int startIdx = t * chunkSize;
            int endIdx = std::min(startIdx + chunkSize, totalPending);
            
            if (startIdx < totalPending) {
                futures.push_back(std::async(std::launch::async, translateWorker, startIdx, endIdx));
            }
        }
        
        // 等待所有线程完成
        for (auto& future : futures) {
            future.wait();
        }
        
        // 更新最终状态
        config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.completedCount = completedCount.load();
        config.failedCount = failedCount.load();
        
        if (config.status != "paused") {
            if (shouldStop.load() && consecutiveFailures.load() >= maxConsecutiveFailures) {
                config.status = "paused";
                Logger::getInstance().error("Too many consecutive failures, pausing task: " + taskId);
            } else if (config.completedCount + config.failedCount >= config.totalCount) {
                config.status = "completed";
                rebuildTranslatedHtml(taskId);
            }
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        config.updatedAt = oss.str();
        
        StorageManager::getInstance().saveTaskConfig(config);
        
        Logger::getInstance().info("Multi-thread translation completed: " + taskId);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to translate task (multi-thread): " + std::string(e.what()));
        
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "failed";
        StorageManager::getInstance().saveTaskConfig(config);
    }
}

// 连续调度翻译 - 多模型工作窃取式调度
void TaskQueue::translateTaskContinuous(const std::string& taskId) {
    try {
        Logger::getInstance().info("Translating task with continuous scheduling: " + taskId);
        
        // 加载任务配置
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "running";
        StorageManager::getInstance().saveTaskConfig(config);
        
        // 加载文献索引
        std::vector<int> indices = StorageManager::getInstance().loadIndexJson(taskId);
        
        // 过滤出待翻译的文献
        std::vector<int> pendingIndices;
        for (int index : indices) {
            LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
            if (data.status != "completed") {
                pendingIndices.push_back(index);
            }
        }
        
        if (pendingIndices.empty()) {
            config.status = "completed";
            StorageManager::getInstance().saveTaskConfig(config);
            rebuildTranslatedHtml(taskId);
            return;
        }
        
        // 共享状态
        std::atomic<int> completedCount(config.completedCount);
        std::atomic<int> failedCount(config.failedCount);
        std::atomic<int> consecutiveFailures(0);
        std::atomic<bool> shouldStop(false);
        std::mutex progressMutex;
        
        // 共享任务队列
        std::queue<int> taskQueue;
        std::mutex queueMutex;
        for (int idx : pendingIndices) {
            taskQueue.push(idx);
        }
        
        int maxConsecutiveFailures = ConfigManager::getInstance().loadSystemConfig().consecutiveFailureThreshold;
        
        // 创建工作函数 - 每个worker从共享队列中取任务
        auto continuousWorker = [&](const ModelConfig& workerModel) {
            Translator translator(workerModel);
            std::string modelName = workerModel.name.empty() ? workerModel.modelId : workerModel.name;
            
            while (!shouldStop.load()) {
                // 从队列中取一个任务
                int index = -1;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (taskQueue.empty()) {
                        return;  // 队列空了，退出
                    }
                    index = taskQueue.front();
                    taskQueue.pop();
                }
                
                // 检查是否被暂停
                TaskConfig currentConfig = StorageManager::getInstance().loadTaskConfig(taskId);
                if (currentConfig.status == "paused") {
                    // 把任务放回队列
                    std::lock_guard<std::mutex> lock(queueMutex);
                    taskQueue.push(index);
                    shouldStop.store(true);
                    return;
                }
                
                LiteratureData data = StorageManager::getInstance().loadLiteratureData(taskId, index);
                
                if (data.status == "completed") {
                    continue;
                }
                
                data.status = "translating";
                data.translatedByModel = modelName;
                StorageManager::getInstance().saveLiteratureData(taskId, index, data);
                
                bool success = true;
                
                // 翻译标题
                if (config.translateTitle && !data.originalTitle.empty()) {
                    TranslationResult result = translator.translate(data.originalTitle, "标题");
                    if (result.success) {
                        data.translatedTitle = result.translatedText;
                    } else {
                        success = false;
                        data.errorMessage = "Title translation failed: " + result.errorMessage;
                    }
                }
                
                // 翻译摘要
                if (success && config.translateAbstract && !data.originalAbstract.empty()) {
                    TranslationResult result = translator.translate(data.originalAbstract, "摘要");
                    if (result.success) {
                        data.translatedAbstract = result.translatedText;
                    } else {
                        success = false;
                        data.errorMessage = "Abstract translation failed: " + result.errorMessage;
                    }
                }
                
                if (success) {
                    data.status = "completed";
                    data.errorMessage = "";
                    completedCount.fetch_add(1);
                    consecutiveFailures.store(0);
                } else {
                    data.status = "failed";
                    failedCount.fetch_add(1);
                    int failures = consecutiveFailures.fetch_add(1) + 1;
                    if (failures >= maxConsecutiveFailures) {
                        shouldStop.store(true);
                    }
                }
                
                StorageManager::getInstance().saveLiteratureData(taskId, index, data);
                
                // 更新任务进度
                {
                    std::lock_guard<std::mutex> lock(progressMutex);
                    TaskConfig latestConfig = StorageManager::getInstance().loadTaskConfig(taskId);
                    if (latestConfig.status == "paused") {
                        shouldStop.store(true);
                    } else {
                        latestConfig.completedCount = completedCount.load();
                        latestConfig.failedCount = failedCount.load();
                        
                        auto now = std::chrono::system_clock::now();
                        auto time = std::chrono::system_clock::to_time_t(now);
                        std::tm tm = *std::gmtime(&time);
                        std::ostringstream oss;
                        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                        latestConfig.updatedAt = oss.str();
                        
                        StorageManager::getInstance().saveTaskConfig(latestConfig);
                    }
                }
            }
        };
        
        // 为每个模型的每个线程创建worker
        std::vector<std::future<void>> futures;
        for (const auto& mwt : config.modelConfigs) {
            for (int t = 0; t < mwt.threads; t++) {
                futures.push_back(std::async(std::launch::async, continuousWorker, mwt.model));
            }
        }
        
        Logger::getInstance().info("Started " + std::to_string(futures.size()) + " continuous workers for task: " + taskId);
        
        // 等待所有worker完成
        for (auto& future : futures) {
            future.wait();
        }
        
        // 更新最终状态
        config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.completedCount = completedCount.load();
        config.failedCount = failedCount.load();
        
        if (config.status != "paused") {
            if (shouldStop.load() && consecutiveFailures.load() >= maxConsecutiveFailures) {
                config.status = "paused";
                Logger::getInstance().error("Too many consecutive failures, pausing task: " + taskId);
            } else if (config.completedCount + config.failedCount >= config.totalCount) {
                config.status = "completed";
                rebuildTranslatedHtml(taskId);
            }
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        config.updatedAt = oss.str();
        
        StorageManager::getInstance().saveTaskConfig(config);
        
        Logger::getInstance().info("Continuous translation completed: " + taskId);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to translate task (continuous): " + std::string(e.what()));
        
        TaskConfig config = StorageManager::getInstance().loadTaskConfig(taskId);
        config.status = "failed";
        StorageManager::getInstance().saveTaskConfig(config);
    }
}

// 模型并发控制
bool TaskQueue::canStartTask(const std::string& modelId) {
    std::lock_guard<std::mutex> lock(modelMutex_);
    
    int maxPerModel = ConfigManager::getInstance().loadSystemConfig().maxConcurrentTasksPerModel;
    
    auto it = modelRunningTasks_.find(modelId);
    if (it == modelRunningTasks_.end()) {
        return true;  // 该模型没有正在运行的任务
    }
    
    return it->second < maxPerModel;
}

void TaskQueue::onTaskStarted(const std::string& taskId, const std::string& modelId) {
    std::lock_guard<std::mutex> lock(modelMutex_);
    
    modelRunningTasks_[modelId]++;
    runningTaskModels_[taskId] = modelId;
    
    Logger::getInstance().info("Task started: " + taskId + " (model: " + modelId + 
                               ", running: " + std::to_string(modelRunningTasks_[modelId]) + ")");
}

void TaskQueue::onTaskFinished(const std::string& taskId, const std::string& modelId) {
    // 注意：调用者需要持有 modelMutex_
    
    auto it = modelRunningTasks_.find(modelId);
    if (it != modelRunningTasks_.end()) {
        it->second--;
        if (it->second <= 0) {
            modelRunningTasks_.erase(it);
        }
    }
    
    runningTaskModels_.erase(taskId);
    
    Logger::getInstance().info("Task finished: " + taskId + " (model: " + modelId + ")");
}

int TaskQueue::getTotalRunningTasks() {
    std::lock_guard<std::mutex> lock(modelMutex_);
    int total = 0;
    for (const auto& pair : modelRunningTasks_) {
        total += pair.second;
    }
    return total;
}
