#ifndef EXPORTER_H
#define EXPORTER_H

#include <string>
#include <vector>
#include "storage_manager.h"

enum class ExportFormat {
    TXT,
    JSON,
    CSV,
    HTML
};

class Exporter {
public:
    static std::string exportLiteratures(
        const std::vector<LiteratureData>& literatures,
        ExportFormat format,
        const std::string& originalFileName);
    
private:
    static std::string exportToTxt(const std::vector<LiteratureData>& literatures);
    static std::string exportToJson(const std::vector<LiteratureData>& literatures);
    static std::string exportToCsv(const std::vector<LiteratureData>& literatures);
    static std::string exportToHtml(const std::vector<LiteratureData>& literatures);
    
    static std::string escapeHtml(const std::string& text);
    static std::string escapeCsv(const std::string& text);
};

#endif // EXPORTER_H
