#include "html_parser.h"
#include "logger.h"
#include <regex>
#include <algorithm>

bool HTMLParser::validate(const std::string& htmlContent) {
    // 检查是否包含WoS标记
    if (htmlContent.find("Clarivate Web of Science") != std::string::npos ||
        htmlContent.find("Web of Science") != std::string::npos) {
        return true;
    }
    
    // 检查是否包含Record标记
    std::regex recordPattern(R"(Record\s+\d+\s+of\s+\d+)");
    return std::regex_search(htmlContent, recordPattern);
}

std::vector<Literature> HTMLParser::parse(const std::string& htmlContent) {
    std::vector<Literature> literatures;
    
    try {
        // 使用<hr>作为记录分隔符来分割HTML
        // WoS HTML格式: <table>记录1</table><hr><table>记录2</table><hr>...
        std::vector<std::string> tables;
        size_t pos = 0;
        
        // 找到第一个<hr>后的内容（跳过头部信息）
        size_t firstHr = htmlContent.find("<hr>");
        if (firstHr == std::string::npos) {
            Logger::getInstance().error("No <hr> separator found in HTML");
            return literatures;
        }
        
        pos = firstHr + 4; // 跳过第一个<hr>
        
        // 按<hr>分割记录
        while (pos < htmlContent.length()) {
            // 找到下一个<hr>或文档结尾
            size_t nextHr = htmlContent.find("<hr>", pos);
            size_t recordEnd = (nextHr != std::string::npos) ? nextHr : htmlContent.length();
            
            // 提取这段内容
            std::string recordContent = htmlContent.substr(pos, recordEnd - pos);
            
            // 检查是否包含Record标记（确保是有效的文献记录）
            if (recordContent.find("Record ") != std::string::npos && 
                recordContent.find(" of ") != std::string::npos) {
                tables.push_back(recordContent);
            }
            
            if (nextHr == std::string::npos) break;
            pos = nextHr + 4; // 跳过<hr>
        }
        
        Logger::getInstance().info("Found " + std::to_string(tables.size()) + " literature tables");
        
        // 解析每个table
        for (const auto& table : tables) {
            Literature lit;
            lit.originalHtml = table;
            
            // 提取Record编号
            std::regex recordPattern(R"(Record\s+(\d+)\s+of\s+(\d+))");
            std::smatch match;
            if (std::regex_search(table, match, recordPattern)) {
                lit.recordNumber = std::stoi(match[1]);
                lit.totalRecords = std::stoi(match[2]);
            }
            
            // 提取Title - 格式: <b>Title:</b><value>...</value>
            lit.title = extractValue(table, "Title:");
            lit.title = decodeHtmlEntities(lit.title);
            
            // 提取Abstract - 格式: <b>Abstract:</b>文本内容</td>
            lit.abstract = extractTextAfterBold(table, "Abstract:");
            lit.abstract = decodeHtmlEntities(lit.abstract);
            
            // 提取Author(s) - 格式: <b>Author(s):</b>文本内容</td>
            lit.authors = extractTextAfterBold(table, "Author(s):");
            lit.authors = decodeHtmlEntities(lit.authors);
            
            // 提取Source行的多个字段 - 所有字段都在同一个<td>中
            // 格式: <b>Source:</b> <value>...</value>&nbsp;&nbsp;<b>Volume:</b> <value>...</value>...
            lit.source = extractValue(table, "Source:");
            lit.source = decodeHtmlEntities(lit.source);
            
            lit.volume = extractValue(table, "Volume:");
            lit.issue = extractValue(table, "Issue:");
            lit.pages = extractValue(table, "Pages:");
            lit.doi = extractValue(table, "DOI:");
            lit.earlyAccessDate = extractValue(table, "Early Access Date:");
            lit.publishedDate = extractValue(table, "Published Date:");
            
            // 提取Accession Number - 格式: <b>Accession Number:</b><value>...</value>
            lit.accessionNumber = extractValue(table, "Accession Number:");
            
            // 提取ISSN - 格式: <b>ISSN:</b><value>...</value>
            lit.issn = extractValue(table, "ISSN:");
            lit.eissn = extractValue(table, "eISSN:");
            
            literatures.push_back(lit);
        }
        
        Logger::getInstance().info("Parsed " + std::to_string(literatures.size()) + " literatures");
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to parse HTML: " + std::string(e.what()));
    }
    
    return literatures;
}

int HTMLParser::countRecords(const std::string& htmlContent) {
    // 从第一个table提取总记录数
    std::regex countPattern(R"((\d+)\s+record\(s\)\s+printed)");
    std::smatch match;
    if (std::regex_search(htmlContent, match, countPattern)) {
        return std::stoi(match[1]);
    }
    
    // 或从第一个文献的Record标记提取
    std::regex recordPattern(R"(Record\s+\d+\s+of\s+(\d+))");
    if (std::regex_search(htmlContent, match, recordPattern)) {
        return std::stoi(match[1]);
    }
    
    return 0;
}

std::string HTMLParser::extractValue(const std::string& html, const std::string& fieldName) {
    // 查找<b>fieldName</b>后的<value>标签
    // 格式: <b>Field:</b> <value>...</value> 或 <b>Field:</b>\n<value>...</value>
    
    // 首先找到<b>fieldName</b>的位置
    std::string searchStr = "<b>" + fieldName + "</b>";
    size_t startPos = html.find(searchStr);
    if (startPos == std::string::npos) {
        return "";
    }
    
    // 跳过<b>fieldName</b>
    startPos += searchStr.length();
    
    // 找到<value>标签
    size_t valueStart = html.find("<value>", startPos);
    if (valueStart == std::string::npos || valueStart > startPos + 50) {
        // <value>标签应该在<b>标签后不远处
        return "";
    }
    
    valueStart += 7; // 跳过"<value>"
    
    // 找到</value>标签
    size_t valueEnd = html.find("</value>", valueStart);
    if (valueEnd == std::string::npos) {
        return "";
    }
    
    // 提取值
    std::string value = html.substr(valueStart, valueEnd - valueStart);
    
    // 去除前后空白
    value.erase(0, value.find_first_not_of(" \t\n\r"));
    if (!value.empty()) {
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
    }
    
    return value;
}

std::string HTMLParser::extractTextAfterBold(const std::string& html, const std::string& boldText) {
    // 查找<b>boldText</b>后的文本，直到</td>
    // 格式: <td><b>Author(s):</b>文本内容</td>
    // 或: <td><b>Abstract:</b>文本内容</td>
    
    // 首先找到<b>boldText</b>的位置
    std::string searchStr = "<b>" + boldText + "</b>";
    size_t startPos = html.find(searchStr);
    if (startPos == std::string::npos) {
        return "";
    }
    
    // 跳过<b>boldText</b>
    startPos += searchStr.length();
    
    // 找到</td>的位置
    size_t endPos = html.find("</td>", startPos);
    if (endPos == std::string::npos) {
        return "";
    }
    
    // 提取中间的文本
    std::string text = html.substr(startPos, endPos - startPos);
    
    // 移除内部的HTML标签（但保留文本）
    std::regex tagPattern("<[^>]+>");
    text = std::regex_replace(text, tagPattern, "");
    
    // 去除前后空白
    text.erase(0, text.find_first_not_of(" \t\n\r"));
    if (!text.empty()) {
        text.erase(text.find_last_not_of(" \t\n\r") + 1);
    }
    
    // 将多个空格替换为单个空格
    std::regex spacePattern("\\s+");
    text = std::regex_replace(text, spacePattern, " ");
    
    return text;
}

std::string HTMLParser::decodeHtmlEntities(const std::string& text) {
    std::string result = text;
    // 基本HTML实体解码
    size_t pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
        result.replace(pos, 5, "&");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&lt;", pos)) != std::string::npos) {
        result.replace(pos, 4, "<");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&gt;", pos)) != std::string::npos) {
        result.replace(pos, 4, ">");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&quot;", pos)) != std::string::npos) {
        result.replace(pos, 6, "\"");
        pos += 1;
    }
    return result;
}

json Literature::toJson() const {
    json j;
    j["recordNumber"] = recordNumber;
    j["totalRecords"] = totalRecords;
    j["title"] = title;
    j["abstract"] = abstract;
    j["authors"] = authors;
    j["source"] = source;
    j["volume"] = volume;
    j["issue"] = issue;
    j["pages"] = pages;
    j["doi"] = doi;
    j["earlyAccessDate"] = earlyAccessDate;
    j["publishedDate"] = publishedDate;
    j["accessionNumber"] = accessionNumber;
    j["issn"] = issn;
    j["eissn"] = eissn;
    j["originalHtml"] = originalHtml;
    return j;
}

Literature Literature::fromJson(const json& j) {
    Literature lit;
    lit.recordNumber = j.value("recordNumber", 0);
    lit.totalRecords = j.value("totalRecords", 0);
    lit.title = j.value("title", "");
    lit.abstract = j.value("abstract", "");
    lit.authors = j.value("authors", "");
    lit.source = j.value("source", "");
    lit.volume = j.value("volume", "");
    lit.issue = j.value("issue", "");
    lit.pages = j.value("pages", "");
    lit.doi = j.value("doi", "");
    lit.earlyAccessDate = j.value("earlyAccessDate", "");
    lit.publishedDate = j.value("publishedDate", "");
    lit.accessionNumber = j.value("accessionNumber", "");
    lit.issn = j.value("issn", "");
    lit.eissn = j.value("eissn", "");
    lit.originalHtml = j.value("originalHtml", "");
    return lit;
}
