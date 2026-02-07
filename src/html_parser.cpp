#include "html_parser.h"
#include "logger.h"
#include <regex>
#include <algorithm>

// Chinese UTF-8 string constants for field names
static const std::string ZH_DI = "\xe7\xac\xac";
static const std::string ZH_TIAO = "\xe6\x9d\xa1";
static const std::string ZH_TIAO_GONG = "\xe6\x9d\xa1\xef\xbc\x8c\xe5\x85\xb1";
static const std::string ZH_BIAOTI = "\xe6\xa0\x87\xe9\xa2\x98:";
static const std::string ZH_ZHAIYAO = "\xe6\x91\x98\xe8\xa6\x81:";
static const std::string ZH_ZUOZHE = "\xe4\xbd\x9c\xe8\x80\x85:";
static const std::string ZH_LAIYUAN = "\xe6\x9d\xa5\xe6\xba\x90\xe5\x87\xba\xe7\x89\x88\xe7\x89\xa9:";
static const std::string ZH_JUAN = "\xe5\x8d\xb7:";
static const std::string ZH_WENXIANHAO = "\xe6\x96\x87\xe7\x8c\xae\xe5\x8f\xb7:";
static const std::string ZH_QI = "\xe6\x9c\x9f:";
static const std::string ZH_YE = "\xe9\xa1\xb5:";
static const std::string ZH_RUCANG = "\xe5\x85\xa5\xe8\x97\x8f\xe5\x8f\xb7:";

static bool isChineseWos(const std::string& htmlContent) {
    if (htmlContent.find(ZH_DI) != std::string::npos &&
        htmlContent.find(ZH_TIAO_GONG) != std::string::npos) {
        return true;
    }
    std::string zhBiaotiTag = "<b>" + ZH_BIAOTI + "</b>";
    if (htmlContent.find(zhBiaotiTag) != std::string::npos) {
        return true;
    }
    return false;
}

bool HTMLParser::validate(const std::string& htmlContent) {
    if (htmlContent.find("Clarivate Web of Science") != std::string::npos ||
        htmlContent.find("Web of Science") != std::string::npos) {
        return true;
    }
    std::regex recordPatternEn(R"(Record\s+\d+\s+of\s+\d+)");
    if (std::regex_search(htmlContent, recordPatternEn)) {
        return true;
    }
    if (isChineseWos(htmlContent)) {
        return true;
    }
    return false;
}

std::vector<Literature> HTMLParser::parse(const std::string& htmlContent) {
    std::vector<Literature> literatures;
    try {
        bool isChinese = isChineseWos(htmlContent);
        if (isChinese) {
            Logger::getInstance().info("Detected Chinese WoS HTML format");
        } else {
            Logger::getInstance().info("Detected English WoS HTML format");
        }
        std::vector<std::string> tables;
        size_t pos = 0;
        size_t firstHr = htmlContent.find("<hr>");
        if (firstHr == std::string::npos) {
            Logger::getInstance().error("No <hr> separator found in HTML");
            return literatures;
        }
        pos = firstHr + 4;
        while (pos < htmlContent.length()) {
            size_t nextHr = htmlContent.find("<hr>", pos);
            size_t recordEnd = (nextHr != std::string::npos) ? nextHr : htmlContent.length();
            std::string recordContent = htmlContent.substr(pos, recordEnd - pos);
            bool hasRecord = false;
            if (isChinese) {
                hasRecord = (recordContent.find(ZH_DI) != std::string::npos &&
                             recordContent.find(ZH_TIAO) != std::string::npos);
            } else {
                hasRecord = (recordContent.find("Record ") != std::string::npos &&
                             recordContent.find(" of ") != std::string::npos);
            }
            if (hasRecord) {
                tables.push_back(recordContent);
            }
            if (nextHr == std::string::npos) break;
            pos = nextHr + 4;
        }
        Logger::getInstance().info("Found " + std::to_string(tables.size()) + " literature tables");
        for (const auto& table : tables) {
            Literature lit;
            lit.originalHtml = table;
            if (isChinese) {
                std::string zhPattern = ZH_DI + "\\s*(\\d+)\\s*" + ZH_TIAO_GONG + "\\s*(\\d+)\\s*" + ZH_TIAO;
                std::regex recordPattern(zhPattern);
                std::smatch match;
                if (std::regex_search(table, match, recordPattern)) {
                    lit.recordNumber = std::stoi(match[1]);
                    lit.totalRecords = std::stoi(match[2]);
                }
            } else {
                std::regex recordPattern(R"(Record\s+(\d+)\s+of\s+(\d+))");
                std::smatch match;
                if (std::regex_search(table, match, recordPattern)) {
                    lit.recordNumber = std::stoi(match[1]);
                    lit.totalRecords = std::stoi(match[2]);
                }
            }
            if (isChinese) {
                lit.title = extractValue(table, ZH_BIAOTI);
                lit.title = decodeHtmlEntities(lit.title);
                lit.abstract = extractTextAfterBold(table, ZH_ZHAIYAO);
                lit.abstract = decodeHtmlEntities(lit.abstract);
                lit.authors = extractTextAfterBold(table, ZH_ZUOZHE);
                lit.authors = decodeHtmlEntities(lit.authors);
                lit.source = extractValue(table, ZH_LAIYUAN);
                lit.source = decodeHtmlEntities(lit.source);
                lit.volume = extractValue(table, ZH_JUAN);
                lit.issue = extractValue(table, ZH_WENXIANHAO);
                if (lit.issue.empty()) lit.issue = extractValue(table, ZH_QI);
                lit.pages = extractValue(table, ZH_YE);
                if (lit.pages.empty()) lit.pages = extractValue(table, "Pages:");
                lit.doi = extractValue(table, "DOI:");
                lit.earlyAccessDate = extractValue(table, "Early Access Date:");
                lit.publishedDate = extractValue(table, "Published Date:");
                lit.accessionNumber = extractValue(table, ZH_RUCANG);
                lit.issn = extractValue(table, "ISSN:");
                lit.eissn = extractValue(table, "eISSN:");
            } else {
                lit.title = extractValue(table, "Title:");
                lit.title = decodeHtmlEntities(lit.title);
                lit.abstract = extractTextAfterBold(table, "Abstract:");
                lit.abstract = decodeHtmlEntities(lit.abstract);
                lit.authors = extractTextAfterBold(table, "Author(s):");
                lit.authors = decodeHtmlEntities(lit.authors);
                lit.source = extractValue(table, "Source:");
                lit.source = decodeHtmlEntities(lit.source);
                lit.volume = extractValue(table, "Volume:");
                lit.issue = extractValue(table, "Issue:");
                lit.pages = extractValue(table, "Pages:");
                lit.doi = extractValue(table, "DOI:");
                lit.earlyAccessDate = extractValue(table, "Early Access Date:");
                lit.publishedDate = extractValue(table, "Published Date:");
                lit.accessionNumber = extractValue(table, "Accession Number:");
                lit.issn = extractValue(table, "ISSN:");
                lit.eissn = extractValue(table, "eISSN:");
            }
            literatures.push_back(lit);
        }
        Logger::getInstance().info("Parsed " + std::to_string(literatures.size()) + " literatures");
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to parse HTML: " + std::string(e.what()));
    }
    return literatures;
}

int HTMLParser::countRecords(const std::string& htmlContent) {
    bool isChinese = isChineseWos(htmlContent);
    if (isChinese) {
        std::string zhPattern = ZH_DI + "\\s*\\d+\\s*" + ZH_TIAO_GONG + "\\s*(\\d+)\\s*" + ZH_TIAO;
        std::regex recordPattern(zhPattern);
        std::smatch match;
        if (std::regex_search(htmlContent, match, recordPattern)) {
            return std::stoi(match[1]);
        }
    }
    std::regex countPattern(R"((\d+)\s+record\(s\)\s+printed)");
    std::smatch match;
    if (std::regex_search(htmlContent, match, countPattern)) {
        return std::stoi(match[1]);
    }
    std::regex recordPattern(R"(Record\s+\d+\s+of\s+(\d+))");
    if (std::regex_search(htmlContent, match, recordPattern)) {
        return std::stoi(match[1]);
    }
    return 0;
}

std::string HTMLParser::extractValue(const std::string& html, const std::string& fieldName) {
    std::string searchStr = "<b>" + fieldName + "</b>";
    size_t startPos = html.find(searchStr);
    if (startPos == std::string::npos) return "";
    startPos += searchStr.length();
    size_t valueStart = html.find("<value>", startPos);
    if (valueStart == std::string::npos || valueStart > startPos + 50) return "";
    valueStart += 7;
    size_t valueEnd = html.find("</value>", valueStart);
    if (valueEnd == std::string::npos) return "";
    std::string value = html.substr(valueStart, valueEnd - valueStart);
    value.erase(0, value.find_first_not_of(" \t\n\r"));
    if (!value.empty()) value.erase(value.find_last_not_of(" \t\n\r") + 1);
    return value;
}

std::string HTMLParser::extractTextAfterBold(const std::string& html, const std::string& boldText) {
    std::string searchStr = "<b>" + boldText + "</b>";
    size_t startPos = html.find(searchStr);
    if (startPos == std::string::npos) return "";
    startPos += searchStr.length();
    size_t endPos = html.find("</td>", startPos);
    if (endPos == std::string::npos) return "";
    std::string text = html.substr(startPos, endPos - startPos);
    std::regex tagPattern("<[^>]+>");
    text = std::regex_replace(text, tagPattern, "");
    text.erase(0, text.find_first_not_of(" \t\n\r"));
    if (!text.empty()) text.erase(text.find_last_not_of(" \t\n\r") + 1);
    std::regex spacePattern("\\s+");
    text = std::regex_replace(text, spacePattern, " ");
    return text;
}

std::string HTMLParser::decodeHtmlEntities(const std::string& text) {
    std::string result = text;
    size_t pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) { result.replace(pos, 5, "&"); pos += 1; }
    pos = 0;
    while ((pos = result.find("&lt;", pos)) != std::string::npos) { result.replace(pos, 4, "<"); pos += 1; }
    pos = 0;
    while ((pos = result.find("&gt;", pos)) != std::string::npos) { result.replace(pos, 4, ">"); pos += 1; }
    pos = 0;
    while ((pos = result.find("&quot;", pos)) != std::string::npos) { result.replace(pos, 6, "\""); pos += 1; }
    return result;
}

json Literature::toJson() const {
    json j;
    j["recordNumber"] = recordNumber; j["totalRecords"] = totalRecords;
    j["title"] = title; j["abstract"] = abstract; j["authors"] = authors;
    j["source"] = source; j["volume"] = volume; j["issue"] = issue;
    j["pages"] = pages; j["doi"] = doi;
    j["earlyAccessDate"] = earlyAccessDate; j["publishedDate"] = publishedDate;
    j["accessionNumber"] = accessionNumber; j["issn"] = issn; j["eissn"] = eissn;
    j["originalHtml"] = originalHtml;
    return j;
}

Literature Literature::fromJson(const json& j) {
    Literature lit;
    lit.recordNumber = j.value("recordNumber", 0); lit.totalRecords = j.value("totalRecords", 0);
    lit.title = j.value("title", ""); lit.abstract = j.value("abstract", "");
    lit.authors = j.value("authors", ""); lit.source = j.value("source", "");
    lit.volume = j.value("volume", ""); lit.issue = j.value("issue", "");
    lit.pages = j.value("pages", ""); lit.doi = j.value("doi", "");
    lit.earlyAccessDate = j.value("earlyAccessDate", "");
    lit.publishedDate = j.value("publishedDate", "");
    lit.accessionNumber = j.value("accessionNumber", "");
    lit.issn = j.value("issn", ""); lit.eissn = j.value("eissn", "");
    lit.originalHtml = j.value("originalHtml", "");
    return lit;
}
