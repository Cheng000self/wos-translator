// 自动生成的嵌入式资源文件 - 请勿手动编辑
// 由 scripts/embed_resources.py 生成

#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct EmbeddedResource {
    const uint8_t* data;
    size_t size;
    const char* mimeType;
};

// 获取嵌入式资源
const EmbeddedResource* getEmbeddedResource(const std::string& path);

// 检查是否有嵌入式资源
bool hasEmbeddedResources();

// 获取所有嵌入式资源路径
std::vector<std::string> getEmbeddedResourcePaths();

#endif // EMBEDDED_RESOURCES_H
