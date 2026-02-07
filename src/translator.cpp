#include "translator.h"
#include "logger.h"
#include "openai.hpp"
#include <thread>
#include <chrono>
#include <curl/curl.h>

Translator::Translator(const ModelConfig& config) : config_(config) {
}

TranslationResult Translator::translate(const std::string& text, const std::string& context) {
    return translateWithRetry(text, context, ConfigManager::getInstance().loadSystemConfig().maxRetries);
}

TestConnectionResult Translator::testConnection() {
    TestConnectionResult result;
    result.success = false;
    result.httpCode = 0;
    
    try {
        Logger::getInstance().info("Testing API connection to: " + config_.url);
        
        // 使用 curl 直接测试连接，设置超时
        CURL* curl = curl_easy_init();
        if (!curl) {
            result.errorMessage = "Failed to initialize curl";
            Logger::getInstance().error(result.errorMessage);
            return result;
        }
        
        // 构建请求
        std::string url = config_.url;
        
        // 根据autoAppendPath决定是否追加路径
        if (config_.autoAppendPath) {
            if (url.back() != '/') url += "/";
            url += "chat/completions";
        }
        
        // 构建请求体
        nlohmann::json requestJson;
        requestJson["model"] = config_.modelId;
        requestJson["messages"] = nlohmann::json::array({
            {{"role", "user"}, {"content", "Hi"}}
        });
        requestJson["max_tokens"] = 5;
        
        // 厂商特定参数
        if (config_.provider == "xiaomi") {
            requestJson["thinking"] = {{"type", config_.enableThinking ? "enabled" : "disabled"}};
        } else if (config_.provider == "minimax") {
            requestJson["reasoning_split"] = true;
        }
        
        std::string requestBody = requestJson.dump();
        
        std::string responseData;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        // Xiaomi使用api-key头，其他使用Authorization Bearer
        if (config_.provider == "xiaomi") {
            headers = curl_slist_append(headers, ("api-key: " + config_.apiKey).c_str());
        } else {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + config_.apiKey).c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);  // 15秒超时
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);  // 10秒连接超时
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, std::string* data) -> size_t {
            data->append(ptr, size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        
        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        result.httpCode = static_cast<int>(httpCode);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            result.errorMessage = curl_easy_strerror(res);
            Logger::getInstance().warning("API connection test failed: " + result.errorMessage);
            return result;
        }
        
        // 检查HTTP状态码
        if (httpCode >= 200 && httpCode < 300) {
            result.success = true;
            Logger::getInstance().info("API connection test successful (HTTP " + std::to_string(httpCode) + ")");
            return result;
        } else if (httpCode == 401) {
            result.errorMessage = "Invalid API key (HTTP 401)";
        } else if (httpCode == 404) {
            result.errorMessage = "Model not found or invalid endpoint (HTTP 404)";
        } else if (httpCode == 429) {
            result.errorMessage = "Rate limit exceeded (HTTP 429)";
        } else {
            // 尝试解析错误响应
            try {
                auto errJson = nlohmann::json::parse(responseData);
                if (errJson.contains("error") && errJson["error"].contains("message")) {
                    result.errorMessage = errJson["error"]["message"].get<std::string>();
                } else {
                    result.errorMessage = "HTTP " + std::to_string(httpCode);
                }
            } catch (...) {
                result.errorMessage = "HTTP " + std::to_string(httpCode) + ": " + responseData.substr(0, 100);
            }
        }
        
        Logger::getInstance().warning("API connection test failed: " + result.errorMessage);
        return result;
        
    } catch (const std::exception& e) {
        result.errorMessage = std::string(e.what());
        Logger::getInstance().error("API connection test exception: " + result.errorMessage);
        return result;
    }
}

void Translator::setConfig(const ModelConfig& config) {
    config_ = config;
}

TranslationResult Translator::translateWithRetry(const std::string& text, 
                                                 const std::string& context,
                                                 int maxRetries) {
    TranslationResult result;
    result.success = false;
    result.retryCount = 0;
    
    for (int attempt = 0; attempt <= maxRetries; attempt++) {
        try {
            result.retryCount = attempt;
            
            Logger::getInstance().info("Translation attempt " + std::to_string(attempt + 1) + 
                                      " for " + context);
            
            // 使用 curl 直接调用 API
            CURL* curl = curl_easy_init();
            if (!curl) {
                result.errorMessage = "Failed to initialize curl";
                Logger::getInstance().error(result.errorMessage);
                continue;
            }
            
            // 构建请求 URL
            std::string url = config_.url;
            
            // 根据autoAppendPath决定是否追加路径
            if (config_.autoAppendPath) {
                if (url.back() != '/') url += "/";
                url += "chat/completions";
            }
            
            // 构建提示词
            std::string systemPrompt = config_.systemPrompt.empty() 
                ? "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。"
                : config_.systemPrompt;
            
            std::string userPrompt = "请将以下" + context + "翻译为中文：\n\n" + text;
            
            // 使用 nlohmann::json 构建请求体（自动处理转义）
            nlohmann::json requestJson;
            requestJson["model"] = config_.modelId;
            requestJson["messages"] = nlohmann::json::array({
                {{"role", "system"}, {"content", systemPrompt}},
                {{"role", "user"}, {"content", userPrompt}}
            });
            requestJson["temperature"] = config_.temperature;
            
            // 厂商特定参数
            if (config_.provider == "xiaomi") {
                requestJson["thinking"] = {{"type", config_.enableThinking ? "enabled" : "disabled"}};
            } else if (config_.provider == "minimax") {
                requestJson["reasoning_split"] = true;
            }
            
            std::string requestBody = requestJson.dump();
            
            std::string responseData;
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            
            // Xiaomi使用api-key头，其他使用Authorization Bearer
            if (config_.provider == "xiaomi") {
                headers = curl_slist_append(headers, ("api-key: " + config_.apiKey).c_str());
            } else {
                headers = curl_slist_append(headers, ("Authorization: Bearer " + config_.apiKey).c_str());
            }
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);  // 60秒超时
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);  // 15秒连接超时
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, std::string* data) -> size_t {
                data->append(ptr, size * nmemb);
                return size * nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            
            CURLcode res = curl_easy_perform(curl);
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            if (res != CURLE_OK) {
                result.errorMessage = curl_easy_strerror(res);
                Logger::getInstance().warning("Translation curl error: " + result.errorMessage);
                
                // 如果还有重试机会，等待后重试
                if (attempt < maxRetries) {
                    int waitSeconds = (1 << attempt) * 2;  // 指数退避：2, 4, 8秒
                    Logger::getInstance().info("Waiting " + std::to_string(waitSeconds) + 
                                             " seconds before retry...");
                    std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
                }
                continue;
            }
            
            // 检查 HTTP 状态码
            if (httpCode < 200 || httpCode >= 300) {
                // 尝试解析错误响应
                try {
                    auto errJson = nlohmann::json::parse(responseData);
                    if (errJson.contains("error") && errJson["error"].contains("message")) {
                        result.errorMessage = errJson["error"]["message"].get<std::string>();
                    } else {
                        result.errorMessage = "HTTP " + std::to_string(httpCode);
                    }
                } catch (...) {
                    result.errorMessage = "HTTP " + std::to_string(httpCode) + ": " + responseData.substr(0, 100);
                }
                
                Logger::getInstance().warning("Translation HTTP error: " + result.errorMessage);
                
                // 判断是否为可重试错误
                bool isRetryable = (httpCode == 429 || httpCode >= 500);
                
                if (isRetryable && attempt < maxRetries) {
                    int waitSeconds = (1 << attempt) * 2;
                    Logger::getInstance().info("Waiting " + std::to_string(waitSeconds) + 
                                             " seconds before retry...");
                    std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
                    continue;
                }
                
                return result;
            }
            
            // 解析响应
            try {
                auto responseJson = nlohmann::json::parse(responseData);
                
                if (responseJson.contains("choices") && responseJson["choices"].is_array() && 
                    !responseJson["choices"].empty()) {
                    auto choice = responseJson["choices"][0];
                    if (choice.contains("message") && choice["message"].contains("content")) {
                        result.translatedText = choice["message"]["content"].get<std::string>();
                        
                        // MiniMAX: 即使使用reasoning_split，也做兜底清理<think>标签
                        if (config_.provider == "minimax") {
                            std::string& text = result.translatedText;
                            size_t thinkStart = text.find("<think>");
                            while (thinkStart != std::string::npos) {
                                size_t thinkEnd = text.find("</think>", thinkStart);
                                if (thinkEnd != std::string::npos) {
                                    text.erase(thinkStart, thinkEnd - thinkStart + 8);
                                } else {
                                    // 没有闭合标签，删除到末尾
                                    text.erase(thinkStart);
                                    break;
                                }
                                thinkStart = text.find("<think>");
                            }
                            // 去除首尾空白
                            size_t start = text.find_first_not_of(" \t\n\r");
                            size_t end = text.find_last_not_of(" \t\n\r");
                            if (start != std::string::npos && end != std::string::npos) {
                                text = text.substr(start, end - start + 1);
                            }
                        }
                        
                        result.success = true;
                        
                        Logger::getInstance().info("Translation successful for " + context);
                        return result;
                    }
                }
                
                result.errorMessage = "Invalid API response format";
                Logger::getInstance().warning("Translation response format error: " + responseData.substr(0, 200));
                
            } catch (const std::exception& e) {
                result.errorMessage = "Failed to parse response: " + std::string(e.what());
                Logger::getInstance().warning(result.errorMessage);
            }
            
        } catch (const std::exception& e) {
            result.errorMessage = std::string(e.what());
            Logger::getInstance().warning("Translation attempt " + std::to_string(attempt + 1) + 
                                        " exception: " + result.errorMessage);
            
            // 如果还有重试机会，等待后重试
            if (attempt < maxRetries) {
                int waitSeconds = (1 << attempt) * 2;
                Logger::getInstance().info("Waiting " + std::to_string(waitSeconds) + 
                                         " seconds before retry...");
                std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
            }
        }
    }
    
    Logger::getInstance().error("Translation failed after " + std::to_string(maxRetries + 1) + 
                               " attempts: " + result.errorMessage);
    return result;
}
