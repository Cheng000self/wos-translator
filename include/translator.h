#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <string>
#include "config_manager.h"

struct TranslationResult {
    bool success;
    std::string translatedText;
    std::string errorMessage;
    int retryCount;
};

struct TestConnectionResult {
    bool success;
    std::string errorMessage;
    int httpCode;
};

class Translator {
public:
    Translator(const ModelConfig& config);
    
    TranslationResult translate(const std::string& text, const std::string& context);
    TestConnectionResult testConnection();
    void setConfig(const ModelConfig& config);
    
private:
    TranslationResult translateWithRetry(const std::string& text, 
                                        const std::string& context,
                                        int maxRetries);
    
    ModelConfig config_;
};

#endif // TRANSLATOR_H
