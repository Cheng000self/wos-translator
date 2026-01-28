#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct Literature {
    // 基本信息
    int recordNumber;
    int totalRecords;
    
    // 核心字段（需要翻译）
    std::string title;
    std::string abstract;
    
    // 扩展字段（不翻译，但需要保存）
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
    
    // 原始HTML（用于翻译后重建）
    std::string originalHtml;
    
    // JSON序列化
    json toJson() const;
    static Literature fromJson(const json& j);
};

class HTMLParser {
public:
    bool validate(const std::string& htmlContent);
    std::vector<Literature> parse(const std::string& htmlContent);
    int countRecords(const std::string& htmlContent);
    
private:
    std::string extractValue(const std::string& html, const std::string& fieldName);
    std::string extractTextAfterBold(const std::string& html, const std::string& boldText);
    std::string decodeHtmlEntities(const std::string& text);
};

#endif // HTML_PARSER_H
