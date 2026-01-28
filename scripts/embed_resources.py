#!/usr/bin/env python3
"""
将 web 目录下的所有文件转换为 C++ 嵌入式资源
生成 embedded_resources.h 和 embedded_resources.cpp
"""

import os
import sys
import mimetypes

def get_mime_type(filepath):
    """获取文件的 MIME 类型"""
    mime_type, _ = mimetypes.guess_type(filepath)
    if mime_type:
        return mime_type
    
    # 自定义扩展名映射
    ext = os.path.splitext(filepath)[1].lower()
    custom_types = {
        '.html': 'text/html',
        '.htm': 'text/html',
        '.css': 'text/css',
        '.js': 'application/javascript',
        '.json': 'application/json',
        '.png': 'image/png',
        '.jpg': 'image/jpeg',
        '.jpeg': 'image/jpeg',
        '.gif': 'image/gif',
        '.svg': 'image/svg+xml',
        '.ico': 'image/x-icon',
        '.woff': 'font/woff',
        '.woff2': 'font/woff2',
        '.ttf': 'font/ttf',
        '.eot': 'application/vnd.ms-fontobject',
    }
    return custom_types.get(ext, 'application/octet-stream')

def sanitize_name(path):
    """将文件路径转换为有效的 C++ 变量名"""
    name = path.replace('/', '_').replace('.', '_').replace('-', '_')
    return 'res_' + name

def collect_files(web_dir):
    """收集 web 目录下的所有文件"""
    files = []
    for root, dirs, filenames in os.walk(web_dir):
        # 跳过隐藏目录
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        
        for filename in filenames:
            if filename.startswith('.'):
                continue
            
            filepath = os.path.join(root, filename)
            # 相对于 web 目录的路径
            relpath = os.path.relpath(filepath, web_dir)
            # URL 路径（使用正斜杠）
            urlpath = '/' + relpath.replace('\\', '/')
            
            files.append({
                'filepath': filepath,
                'relpath': relpath,
                'urlpath': urlpath,
                'varname': sanitize_name(relpath),
                'mime': get_mime_type(filepath)
            })
    
    return files

def generate_header(files, output_dir):
    """生成头文件"""
    header = '''// 自动生成的嵌入式资源文件 - 请勿手动编辑
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
'''
    
    header_path = os.path.join(output_dir, 'embedded_resources.h')
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write(header)
    
    print(f"生成头文件: {header_path}")

def generate_source(files, output_dir):
    """生成源文件"""
    source_lines = [
        '// 自动生成的嵌入式资源文件 - 请勿手动编辑',
        '// 由 scripts/embed_resources.py 生成',
        '',
        '#include "embedded_resources.h"',
        '#include <vector>',
        '',
        'namespace {',
        ''
    ]
    
    # 生成每个文件的字节数组
    for f in files:
        with open(f['filepath'], 'rb') as fp:
            data = fp.read()
        
        source_lines.append(f"// {f['relpath']} ({len(data)} bytes)")
        source_lines.append(f"const uint8_t {f['varname']}[] = {{")
        
        # 每行 16 个字节
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
            source_lines.append(f"    {hex_str},")
        
        source_lines.append("};")
        source_lines.append("")
    
    # 生成资源映射表
    source_lines.append("// 资源映射表")
    source_lines.append("struct ResourceEntry {")
    source_lines.append("    const char* path;")
    source_lines.append("    const uint8_t* data;")
    source_lines.append("    size_t size;")
    source_lines.append("    const char* mimeType;")
    source_lines.append("};")
    source_lines.append("")
    source_lines.append(f"const ResourceEntry resourceTable[] = {{")
    
    for f in files:
        with open(f['filepath'], 'rb') as fp:
            size = len(fp.read())
        source_lines.append(f'    {{"{f["urlpath"]}", {f["varname"]}, {size}, "{f["mime"]}"}},')
    
    source_lines.append("};")
    source_lines.append("")
    source_lines.append(f"const size_t resourceCount = {len(files)};")
    source_lines.append("")
    source_lines.append("} // anonymous namespace")
    source_lines.append("")
    
    # 生成查找函数
    source_lines.append("const EmbeddedResource* getEmbeddedResource(const std::string& path) {")
    source_lines.append("    static std::unordered_map<std::string, EmbeddedResource> cache;")
    source_lines.append("    static bool initialized = false;")
    source_lines.append("    ")
    source_lines.append("    if (!initialized) {")
    source_lines.append("        for (size_t i = 0; i < resourceCount; ++i) {")
    source_lines.append("            cache[resourceTable[i].path] = {")
    source_lines.append("                resourceTable[i].data,")
    source_lines.append("                resourceTable[i].size,")
    source_lines.append("                resourceTable[i].mimeType")
    source_lines.append("            };")
    source_lines.append("        }")
    source_lines.append("        initialized = true;")
    source_lines.append("    }")
    source_lines.append("    ")
    source_lines.append("    auto it = cache.find(path);")
    source_lines.append("    if (it != cache.end()) {")
    source_lines.append("        return &it->second;")
    source_lines.append("    }")
    source_lines.append("    ")
    source_lines.append("    // 尝试添加 index.html")
    source_lines.append("    if (path.back() == '/') {")
    source_lines.append("        it = cache.find(path + \"index.html\");")
    source_lines.append("        if (it != cache.end()) {")
    source_lines.append("            return &it->second;")
    source_lines.append("        }")
    source_lines.append("    }")
    source_lines.append("    ")
    source_lines.append("    return nullptr;")
    source_lines.append("}")
    source_lines.append("")
    source_lines.append("bool hasEmbeddedResources() {")
    source_lines.append("    return resourceCount > 0;")
    source_lines.append("}")
    source_lines.append("")
    source_lines.append("std::vector<std::string> getEmbeddedResourcePaths() {")
    source_lines.append("    std::vector<std::string> paths;")
    source_lines.append("    for (size_t i = 0; i < resourceCount; ++i) {")
    source_lines.append("        paths.push_back(resourceTable[i].path);")
    source_lines.append("    }")
    source_lines.append("    return paths;")
    source_lines.append("}")
    
    source_path = os.path.join(output_dir, 'embedded_resources.cpp')
    with open(source_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(source_lines))
    
    print(f"生成源文件: {source_path}")

def generate_empty_resources(output_dir):
    """生成空的资源文件（用于非嵌入式构建）"""
    header = '''// 自动生成的嵌入式资源文件 - 请勿手动编辑
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
'''
    
    source = '''// 自动生成的嵌入式资源文件 - 请勿手动编辑
// 空资源文件（非嵌入式构建）

#include "embedded_resources.h"

const EmbeddedResource* getEmbeddedResource(const std::string&) {
    return nullptr;
}

bool hasEmbeddedResources() {
    return false;
}

std::vector<std::string> getEmbeddedResourcePaths() {
    return {};
}
'''
    
    with open(os.path.join(output_dir, 'embedded_resources.h'), 'w') as f:
        f.write(header)
    
    with open(os.path.join(output_dir, 'embedded_resources.cpp'), 'w') as f:
        f.write(source)
    
    print("生成空资源文件（非嵌入式构建）")

def main():
    if len(sys.argv) < 3:
        print(f"用法: {sys.argv[0]} <web目录> <输出目录> [--empty]")
        print(f"示例: {sys.argv[0]} web src")
        print(f"      {sys.argv[0]} web src --empty  # 生成空资源文件")
        sys.exit(1)
    
    web_dir = sys.argv[1]
    output_dir = sys.argv[2]
    empty_mode = '--empty' in sys.argv
    
    if empty_mode:
        generate_empty_resources(output_dir)
        return
    
    if not os.path.isdir(web_dir):
        print(f"错误: web 目录不存在: {web_dir}")
        sys.exit(1)
    
    os.makedirs(output_dir, exist_ok=True)
    
    print(f"扫描 web 目录: {web_dir}")
    files = collect_files(web_dir)
    
    if not files:
        print("警告: 没有找到任何文件")
        generate_empty_resources(output_dir)
        return
    
    print(f"找到 {len(files)} 个文件:")
    total_size = 0
    for f in files:
        size = os.path.getsize(f['filepath'])
        total_size += size
        print(f"  {f['urlpath']} ({size} bytes) [{f['mime']}]")
    
    print(f"总大小: {total_size / 1024:.1f} KB")
    print()
    
    generate_header(files, output_dir)
    generate_source(files, output_dir)
    
    print()
    print("资源嵌入完成!")

if __name__ == '__main__':
    main()
