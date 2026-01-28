#include "storage_manager.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define platform_mkdir(path, mode) _mkdir(path)
    #define platform_stat _stat
    #define platform_stat_struct struct _stat
    #define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #define platform_mkdir mkdir
    #define platform_stat stat
    #define platform_stat_struct struct stat
#endif

// 跨平台文件系统操作
#if __cplusplus >= 201703L
    #include <filesystem>
    namespace fs = std::filesystem;
    #define USE_STD_FILESYSTEM 1
#else
    #define USE_STD_FILESYSTEM 0
#endif

StorageManager& StorageManager::getInstance() {
    static StorageManager instance;
    return instance;
}

std::string StorageManager::getTaskPath(const std::string& taskId) {
    return "data/" + taskId;
}

bool StorageManager::createTaskDirectory(const std::string& taskId) {
    try {
        std::string path = getTaskPath(taskId);
        
        // 创建日期目录
        size_t slashPos = taskId.find('/');
        if (slashPos != std::string::npos) {
            std::string dateDir = "data/" + taskId.substr(0, slashPos);
            platform_mkdir(dateDir.c_str(), 0755);
        }
        
        // 创建任务目录
        if (platform_mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            Logger::getInstance().error("Failed to create task directory: " + path);
            return false;
        }
        
        // 创建list子目录
        std::string listDir = path + "/list";
        if (platform_mkdir(listDir.c_str(), 0755) != 0 && errno != EEXIST) {
            Logger::getInstance().error("Failed to create list directory: " + listDir);
            return false;
        }
        
        Logger::getInstance().info("Created task directory: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception creating task directory: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::saveTaskConfig(const TaskConfig& config) {
    try {
        std::string path = getTaskPath(config.taskId) + "/config.json";
        
        json j;
        j["taskId"] = config.taskId;
        j["taskName"] = config.taskName;
        j["fileName"] = config.fileName;
        j["fileNames"] = config.fileNames;
        j["translateTitle"] = config.translateTitle;
        j["translateAbstract"] = config.translateAbstract;
        
        // 保存模型配置
        json modelJson;
        modelJson["url"] = config.modelConfig.url;
        modelJson["apiKey"] = config.modelConfig.apiKey;
        modelJson["modelId"] = config.modelConfig.modelId;
        modelJson["temperature"] = config.modelConfig.temperature;
        modelJson["systemPrompt"] = config.modelConfig.systemPrompt;
        j["modelConfig"] = modelJson;
        
        j["totalCount"] = config.totalCount;
        j["completedCount"] = config.completedCount;
        j["failedCount"] = config.failedCount;
        j["status"] = config.status;
        j["createdAt"] = config.createdAt;
        j["updatedAt"] = config.updatedAt;
        j["deleted"] = config.deleted;
        
        std::ofstream file(path);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open config file for writing: " + path);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save task config: " + std::string(e.what()));
        return false;
    }
}

TaskConfig StorageManager::loadTaskConfig(const std::string& taskId) {
    TaskConfig config;
    
    try {
        std::string path = getTaskPath(taskId) + "/config.json";
        std::ifstream file(path);
        
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open config file: " + path);
            return config;
        }
        
        json j;
        file >> j;
        
        config.taskId = j.value("taskId", "");
        config.taskName = j.value("taskName", "");
        config.fileName = j.value("fileName", "");
        
        // 加载多文件名
        if (j.contains("fileNames") && j["fileNames"].is_array()) {
            config.fileNames = j["fileNames"].get<std::vector<std::string>>();
        }
        
        config.translateTitle = j.value("translateTitle", true);
        config.translateAbstract = j.value("translateAbstract", true);
        
        if (j.contains("modelConfig")) {
            json modelJson = j["modelConfig"];
            config.modelConfig.url = modelJson.value("url", "");
            config.modelConfig.apiKey = modelJson.value("apiKey", "");
            config.modelConfig.modelId = modelJson.value("modelId", "");
            config.modelConfig.temperature = modelJson.value("temperature", 0.3f);
            config.modelConfig.systemPrompt = modelJson.value("systemPrompt", "");
        }
        
        config.totalCount = j.value("totalCount", 0);
        config.completedCount = j.value("completedCount", 0);
        config.failedCount = j.value("failedCount", 0);
        config.status = j.value("status", "pending");
        config.createdAt = j.value("createdAt", "");
        config.updatedAt = j.value("updatedAt", "");
        config.deleted = j.value("deleted", false);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load task config: " + std::string(e.what()));
    }
    
    return config;
}

bool StorageManager::saveOriginalHtml(const std::string& taskId, const std::string& content) {
    try {
        std::string path = getTaskPath(taskId) + "/original.html";
        std::ofstream file(path);
        
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open original.html for writing: " + path);
            return false;
        }
        
        file << content;
        file.close();
        
        Logger::getInstance().info("Saved original HTML: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save original HTML: " + std::string(e.what()));
        return false;
    }
}

std::string StorageManager::loadOriginalHtml(const std::string& taskId) {
    try {
        std::string path = getTaskPath(taskId) + "/original.html";
        std::ifstream file(path);
        
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open original.html: " + path);
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load original HTML: " + std::string(e.what()));
        return "";
    }
}

bool StorageManager::saveLiteratureData(const std::string& taskId, int index, const LiteratureData& data) {
    try {
        std::string path = getTaskPath(taskId) + "/list/" + std::to_string(index) + ".json";
        
        json j;
        j["index"] = data.index;
        j["recordNumber"] = data.recordNumber;
        j["totalRecords"] = data.totalRecords;
        j["sourceFileName"] = data.sourceFileName;
        j["sourceFileIndex"] = data.sourceFileIndex;
        j["indexInFile"] = data.indexInFile;
        j["originalTitle"] = data.originalTitle;
        j["originalAbstract"] = data.originalAbstract;
        j["translatedTitle"] = data.translatedTitle;
        j["translatedAbstract"] = data.translatedAbstract;
        j["authors"] = data.authors;
        j["source"] = data.source;
        j["volume"] = data.volume;
        j["issue"] = data.issue;
        j["pages"] = data.pages;
        j["doi"] = data.doi;
        j["earlyAccessDate"] = data.earlyAccessDate;
        j["publishedDate"] = data.publishedDate;
        j["accessionNumber"] = data.accessionNumber;
        j["issn"] = data.issn;
        j["eissn"] = data.eissn;
        j["status"] = data.status;
        j["errorMessage"] = data.errorMessage;
        
        std::ofstream file(path);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open literature file for writing: " + path);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save literature data: " + std::string(e.what()));
        return false;
    }
}

LiteratureData StorageManager::loadLiteratureData(const std::string& taskId, int index) {
    LiteratureData data;
    data.sourceFileIndex = 1;
    data.indexInFile = 0;
    
    try {
        std::string path = getTaskPath(taskId) + "/list/" + std::to_string(index) + ".json";
        std::ifstream file(path);
        
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open literature file: " + path);
            return data;
        }
        
        json j;
        file >> j;
        
        data.index = j.value("index", 0);
        data.recordNumber = j.value("recordNumber", 0);
        data.totalRecords = j.value("totalRecords", 0);
        data.sourceFileName = j.value("sourceFileName", "");
        data.sourceFileIndex = j.value("sourceFileIndex", 1);
        data.indexInFile = j.value("indexInFile", 0);
        data.originalTitle = j.value("originalTitle", "");
        data.originalAbstract = j.value("originalAbstract", "");
        data.translatedTitle = j.value("translatedTitle", "");
        data.translatedAbstract = j.value("translatedAbstract", "");
        data.authors = j.value("authors", "");
        data.source = j.value("source", "");
        data.volume = j.value("volume", "");
        data.issue = j.value("issue", "");
        data.pages = j.value("pages", "");
        data.doi = j.value("doi", "");
        data.earlyAccessDate = j.value("earlyAccessDate", "");
        data.publishedDate = j.value("publishedDate", "");
        data.accessionNumber = j.value("accessionNumber", "");
        data.issn = j.value("issn", "");
        data.eissn = j.value("eissn", "");
        data.status = j.value("status", "pending");
        data.errorMessage = j.value("errorMessage", "");
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load literature data: " + std::string(e.what()));
    }
    
    return data;
}

bool StorageManager::saveTranslatedHtml(const std::string& taskId, const std::string& content) {
    try {
        std::string path = getTaskPath(taskId) + "/translated.html";
        std::ofstream file(path);
        
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open translated.html for writing: " + path);
            return false;
        }
        
        file << content;
        file.close();
        
        Logger::getInstance().info("Saved translated HTML: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save translated HTML: " + std::string(e.what()));
        return false;
    }
}

std::string StorageManager::loadTranslatedHtml(const std::string& taskId) {
    try {
        std::string path = getTaskPath(taskId) + "/translated.html";
        std::ifstream file(path);
        
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load translated HTML: " + std::string(e.what()));
        return "";
    }
}

bool StorageManager::saveIndexJson(const std::string& taskId, const std::vector<int>& indices) {
    try {
        std::string path = getTaskPath(taskId) + "/index.json";
        
        json j = json::array();
        for (int index : indices) {
            j.push_back(index);
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open index.json for writing: " + path);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save index.json: " + std::string(e.what()));
        return false;
    }
}

std::vector<int> StorageManager::loadIndexJson(const std::string& taskId) {
    std::vector<int> indices;
    
    try {
        std::string path = getTaskPath(taskId) + "/index.json";
        std::ifstream file(path);
        
        if (!file.is_open()) {
            return indices;
        }
        
        json j;
        file >> j;
        
        for (const auto& item : j) {
            indices.push_back(item.get<int>());
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to load index.json: " + std::string(e.what()));
    }
    
    return indices;
}

bool StorageManager::deleteTask(const std::string& taskId) {
    try {
        std::string path = getTaskPath(taskId);
        
#if USE_STD_FILESYSTEM
        // 使用 C++17 filesystem
        std::error_code ec;
        fs::remove_all(path, ec);
        if (!ec) {
            Logger::getInstance().info("Deleted task: " + taskId);
            return true;
        } else {
            Logger::getInstance().error("Failed to delete task: " + taskId + " - " + ec.message());
            return false;
        }
#elif defined(_WIN32)
        // Windows: 使用 system 命令
        std::string command = "rmdir /s /q \"" + path + "\"";
        int result = system(command.c_str());
        if (result == 0) {
            Logger::getInstance().info("Deleted task: " + taskId);
            return true;
        } else {
            Logger::getInstance().error("Failed to delete task: " + taskId);
            return false;
        }
#else
        // Linux: 使用 rm -rf
        std::string command = "rm -rf \"" + path + "\"";
        int result = system(command.c_str());
        if (result == 0) {
            Logger::getInstance().info("Deleted task: " + taskId);
            return true;
        } else {
            Logger::getInstance().error("Failed to delete task: " + taskId);
            return false;
        }
#endif
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception deleting task: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::softDeleteTask(const std::string& taskId) {
    try {
        TaskConfig config = loadTaskConfig(taskId);
        if (config.taskId.empty()) {
            Logger::getInstance().error("Task not found for soft delete: " + taskId);
            return false;
        }
        
        config.deleted = true;
        
        // 更新时间
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        config.updatedAt = oss.str();
        
        if (saveTaskConfig(config)) {
            Logger::getInstance().info("Soft deleted task: " + taskId);
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Exception soft deleting task: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::permanentDeleteTask(const std::string& taskId) {
    return deleteTask(taskId);
}

std::vector<std::string> StorageManager::listDeletedTasks() {
    std::vector<std::string> deletedTasks;
    
    try {
#if USE_STD_FILESYSTEM
        // 使用 C++17 filesystem
        if (!fs::exists("data")) return deletedTasks;
        
        for (const auto& dateEntry : fs::directory_iterator("data")) {
            if (!dateEntry.is_directory()) continue;
            std::string dateName = dateEntry.path().filename().string();
            if (dateName[0] == '.') continue;
            
            for (const auto& taskEntry : fs::directory_iterator(dateEntry.path())) {
                if (!taskEntry.is_directory()) continue;
                std::string taskName = taskEntry.path().filename().string();
                if (taskName[0] == '.') continue;
                
                std::string taskId = dateName + "/" + taskName;
                TaskConfig config = loadTaskConfig(taskId);
                if (!config.taskId.empty() && config.deleted) {
                    deletedTasks.push_back(taskId);
                }
            }
        }
#elif defined(_WIN32)
        // Windows: 使用 FindFirstFile/FindNextFile
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA("data\\*", &findData);
        if (hFind == INVALID_HANDLE_VALUE) return deletedTasks;
        
        do {
            if (findData.cFileName[0] == '.') continue;
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            
            std::string datePath = std::string("data\\") + findData.cFileName;
            std::string datePattern = datePath + "\\*";
            
            WIN32_FIND_DATAA taskFindData;
            HANDLE hTaskFind = FindFirstFileA(datePattern.c_str(), &taskFindData);
            if (hTaskFind != INVALID_HANDLE_VALUE) {
                do {
                    if (taskFindData.cFileName[0] == '.') continue;
                    if (!(taskFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    
                    std::string taskId = std::string(findData.cFileName) + "/" + taskFindData.cFileName;
                    TaskConfig config = loadTaskConfig(taskId);
                    if (!config.taskId.empty() && config.deleted) {
                        deletedTasks.push_back(taskId);
                    }
                } while (FindNextFileA(hTaskFind, &taskFindData));
                FindClose(hTaskFind);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
#else
        // Linux: 使用 opendir/readdir
        DIR* dir = opendir("data");
        if (!dir) {
            return deletedTasks;
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
                TaskConfig config = loadTaskConfig(taskId);
                if (!config.taskId.empty() && config.deleted) {
                    deletedTasks.push_back(taskId);
                }
            }
            closedir(dateDir);
        }
        closedir(dir);
#endif
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to list deleted tasks: " + std::string(e.what()));
    }
    
    return deletedTasks;
}

int StorageManager::permanentDeleteAllDeleted() {
    std::vector<std::string> deletedTasks = listDeletedTasks();
    int count = 0;
    
    for (const auto& taskId : deletedTasks) {
        if (permanentDeleteTask(taskId)) {
            count++;
        }
    }
    
    Logger::getInstance().info("Permanently deleted " + std::to_string(count) + " tasks");
    return count;
}

// 递归计算目录大小
static uint64_t calculateDirSize(const std::string& path) {
    uint64_t size = 0;
    
#if USE_STD_FILESYSTEM
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                size += entry.file_size();
            }
        }
    } catch (...) {
        // 忽略错误
    }
#elif defined(_WIN32)
    WIN32_FIND_DATAA findData;
    std::string pattern = path + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    
    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "\\" + findData.cFileName;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            size += calculateDirSize(fullPath);
        } else {
            LARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            size += fileSize.QuadPart;
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        platform_stat_struct st;
        
        if (platform_stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                size += calculateDirSize(fullPath);
            } else {
                size += st.st_size;
            }
        }
    }
    
    closedir(dir);
#endif
    
    return size;
}

uint64_t StorageManager::getStorageUsage() {
    return calculateDirSize("data");
}

std::string StorageManager::formatStorageSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}
