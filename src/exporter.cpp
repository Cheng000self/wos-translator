#include "exporter.h"
#include "logger.h"
#include "nlohmann/json.hpp"
#include <sstream>

using json = nlohmann::json;

// 占位符实现，将在任务8中完成
std::string Exporter::exportLiteratures(
    const std::vector<LiteratureData>& literatures,
    ExportFormat format,
    const std::string& originalFileName) {
    
    switch (format) {
        case ExportFormat::TXT:
            return exportToTxt(literatures);
        case ExportFormat::JSON:
            return exportToJson(literatures);
        case ExportFormat::CSV:
            return exportToCsv(literatures);
        case ExportFormat::HTML:
            return exportToHtml(literatures);
        default:
            return "";
    }
}

std::string Exporter::exportToTxt(const std::vector<LiteratureData>& literatures) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < literatures.size(); i++) {
        const auto& lit = literatures[i];
        
        oss << "========================================\n";
        oss << "文献 " << lit.recordNumber << " / " << lit.totalRecords << "\n";
        oss << "========================================\n\n";
        
        // 标题
        if (!lit.originalTitle.empty()) {
            oss << "标题（原文）：\n" << lit.originalTitle << "\n\n";
        }
        if (!lit.translatedTitle.empty()) {
            oss << "标题（译文）：\n" << lit.translatedTitle << "\n\n";
        }
        
        // 作者
        if (!lit.authors.empty()) {
            oss << "作者：\n" << lit.authors << "\n\n";
        }
        
        // 来源
        if (!lit.source.empty()) {
            oss << "来源：" << lit.source;
            if (!lit.volume.empty()) oss << ", 卷: " << lit.volume;
            if (!lit.issue.empty()) oss << ", 期: " << lit.issue;
            if (!lit.pages.empty()) oss << ", 页: " << lit.pages;
            oss << "\n\n";
        }
        
        // DOI
        if (!lit.doi.empty()) {
            oss << "DOI：" << lit.doi << "\n\n";
        }
        
        // 日期
        if (!lit.publishedDate.empty()) {
            oss << "发表日期：" << lit.publishedDate << "\n";
        }
        if (!lit.earlyAccessDate.empty()) {
            oss << "早期访问日期：" << lit.earlyAccessDate << "\n";
        }
        if (!lit.publishedDate.empty() || !lit.earlyAccessDate.empty()) {
            oss << "\n";
        }
        
        // 摘要
        if (!lit.originalAbstract.empty()) {
            oss << "摘要（原文）：\n" << lit.originalAbstract << "\n\n";
        }
        if (!lit.translatedAbstract.empty()) {
            oss << "摘要（译文）：\n" << lit.translatedAbstract << "\n\n";
        }
        
        // 登录号
        if (!lit.accessionNumber.empty()) {
            oss << "WoS登录号：" << lit.accessionNumber << "\n\n";
        }
        
        // ISSN
        if (!lit.issn.empty()) {
            oss << "ISSN：" << lit.issn << "\n";
        }
        if (!lit.eissn.empty()) {
            oss << "eISSN：" << lit.eissn << "\n";
        }
        
        if (i < literatures.size() - 1) {
            oss << "\n\n";
        }
    }
    
    return oss.str();
}

std::string Exporter::exportToJson(const std::vector<LiteratureData>& literatures) {
    json result = json::array();
    
    for (const auto& lit : literatures) {
        json litJson;
        litJson["recordNumber"] = lit.recordNumber;
        litJson["totalRecords"] = lit.totalRecords;
        litJson["originalTitle"] = lit.originalTitle;
        litJson["originalAbstract"] = lit.originalAbstract;
        litJson["translatedTitle"] = lit.translatedTitle;
        litJson["translatedAbstract"] = lit.translatedAbstract;
        litJson["authors"] = lit.authors;
        litJson["source"] = lit.source;
        litJson["volume"] = lit.volume;
        litJson["issue"] = lit.issue;
        litJson["pages"] = lit.pages;
        litJson["doi"] = lit.doi;
        litJson["earlyAccessDate"] = lit.earlyAccessDate;
        litJson["publishedDate"] = lit.publishedDate;
        litJson["accessionNumber"] = lit.accessionNumber;
        litJson["issn"] = lit.issn;
        litJson["eissn"] = lit.eissn;
        litJson["status"] = lit.status;
        
        result.push_back(litJson);
    }
    
    return result.dump(2);  // 格式化输出，缩进2个空格
}

std::string Exporter::exportToCsv(const std::vector<LiteratureData>& literatures) {
    std::ostringstream oss;
    
    // CSV表头
    oss << "Record Number,Total Records,Original Title,Translated Title,"
        << "Original Abstract,Translated Abstract,Authors,Source,Volume,Issue,Pages,"
        << "DOI,Early Access Date,Published Date,Accession Number,ISSN,eISSN,Status\n";
    
    // 数据行
    for (const auto& lit : literatures) {
        oss << lit.recordNumber << ","
            << lit.totalRecords << ","
            << escapeCsv(lit.originalTitle) << ","
            << escapeCsv(lit.translatedTitle) << ","
            << escapeCsv(lit.originalAbstract) << ","
            << escapeCsv(lit.translatedAbstract) << ","
            << escapeCsv(lit.authors) << ","
            << escapeCsv(lit.source) << ","
            << escapeCsv(lit.volume) << ","
            << escapeCsv(lit.issue) << ","
            << escapeCsv(lit.pages) << ","
            << escapeCsv(lit.doi) << ","
            << escapeCsv(lit.earlyAccessDate) << ","
            << escapeCsv(lit.publishedDate) << ","
            << escapeCsv(lit.accessionNumber) << ","
            << escapeCsv(lit.issn) << ","
            << escapeCsv(lit.eissn) << ","
            << escapeCsv(lit.status) << "\n";
    }
    
    return oss.str();
}

std::string Exporter::exportToHtml(const std::vector<LiteratureData>& literatures) {
    std::ostringstream oss;
    
    // HTML头部
    oss << "<!DOCTYPE html>\n"
        << "<html lang=\"zh-CN\">\n"
        << "<head>\n"
        << "    <meta charset=\"UTF-8\">\n"
        << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "    <title>文献翻译结果</title>\n"
        << "    <style>\n"
        << "        body { font-family: Arial, sans-serif; margin: 20px; line-height: 1.6; }\n"
        << "        .literature { border: 1px solid #ddd; margin-bottom: 20px; padding: 15px; }\n"
        << "        .literature h2 { color: #333; margin-top: 0; }\n"
        << "        .field { margin-bottom: 10px; }\n"
        << "        .field-label { font-weight: bold; color: #666; }\n"
        << "        .original { background-color: #f9f9f9; padding: 10px; margin: 5px 0; }\n"
        << "        .translated { background-color: #e8f4f8; padding: 10px; margin: 5px 0; }\n"
        << "    </style>\n"
        << "</head>\n"
        << "<body>\n"
        << "    <h1>文献翻译结果</h1>\n"
        << "    <p>共 " << literatures.size() << " 篇文献</p>\n\n";
    
    // 文献内容
    for (const auto& lit : literatures) {
        oss << "    <div class=\"literature\">\n"
            << "        <h2>文献 " << lit.recordNumber << " / " << lit.totalRecords << "</h2>\n";
        
        // 标题
        if (!lit.originalTitle.empty() || !lit.translatedTitle.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <div class=\"field-label\">标题：</div>\n";
            if (!lit.originalTitle.empty()) {
                oss << "            <div class=\"original\">原文：" << escapeHtml(lit.originalTitle) << "</div>\n";
            }
            if (!lit.translatedTitle.empty()) {
                oss << "            <div class=\"translated\">译文：" << escapeHtml(lit.translatedTitle) << "</div>\n";
            }
            oss << "        </div>\n";
        }
        
        // 作者
        if (!lit.authors.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">作者：</span>" << escapeHtml(lit.authors) << "\n"
                << "        </div>\n";
        }
        
        // 来源
        if (!lit.source.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">来源：</span>" << escapeHtml(lit.source);
            if (!lit.volume.empty()) oss << ", 卷: " << escapeHtml(lit.volume);
            if (!lit.issue.empty()) oss << ", 期: " << escapeHtml(lit.issue);
            if (!lit.pages.empty()) oss << ", 页: " << escapeHtml(lit.pages);
            oss << "\n        </div>\n";
        }
        
        // DOI
        if (!lit.doi.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">DOI：</span>" << escapeHtml(lit.doi) << "\n"
                << "        </div>\n";
        }
        
        // 日期
        if (!lit.publishedDate.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">发表日期：</span>" << escapeHtml(lit.publishedDate) << "\n"
                << "        </div>\n";
        }
        if (!lit.earlyAccessDate.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">早期访问日期：</span>" << escapeHtml(lit.earlyAccessDate) << "\n"
                << "        </div>\n";
        }
        
        // 摘要
        if (!lit.originalAbstract.empty() || !lit.translatedAbstract.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <div class=\"field-label\">摘要：</div>\n";
            if (!lit.originalAbstract.empty()) {
                oss << "            <div class=\"original\">原文：" << escapeHtml(lit.originalAbstract) << "</div>\n";
            }
            if (!lit.translatedAbstract.empty()) {
                oss << "            <div class=\"translated\">译文：" << escapeHtml(lit.translatedAbstract) << "</div>\n";
            }
            oss << "        </div>\n";
        }
        
        // 登录号
        if (!lit.accessionNumber.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">WoS登录号：</span>" << escapeHtml(lit.accessionNumber) << "\n"
                << "        </div>\n";
        }
        
        // ISSN
        if (!lit.issn.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">ISSN：</span>" << escapeHtml(lit.issn) << "\n"
                << "        </div>\n";
        }
        if (!lit.eissn.empty()) {
            oss << "        <div class=\"field\">\n"
                << "            <span class=\"field-label\">eISSN：</span>" << escapeHtml(lit.eissn) << "\n"
                << "        </div>\n";
        }
        
        oss << "    </div>\n\n";
    }
    
    // HTML尾部
    oss << "</body>\n"
        << "</html>\n";
    
    return oss.str();
}

std::string Exporter::escapeHtml(const std::string& text) {
    std::string result;
    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c; break;
        }
    }
    return result;
}

std::string Exporter::escapeCsv(const std::string& text) {
    if (text.find(',') != std::string::npos || 
        text.find('"') != std::string::npos ||
        text.find('\n') != std::string::npos) {
        std::string result = "\"";
        for (char c : text) {
            if (c == '"') {
                result += "\"\"";
            } else {
                result += c;
            }
        }
        result += "\"";
        return result;
    }
    return text;
}
