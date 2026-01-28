// 自动生成的嵌入式资源文件 - 请勿手动编辑
// 空资源文件（非嵌入式构建）

#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H

#include <string>
#include <vector>
#include <cstdint>

struct EmbeddedResource {
    const uint8_t* data;
    size_t size;
    const char* mimeType;
};

const EmbeddedResource* getEmbeddedResource(const std::string& path);
bool hasEmbeddedResources();
std::vector<std::string> getEmbeddedResourcePaths();

#endif // EMBEDDED_RESOURCES_H
