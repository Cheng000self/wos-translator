#!/bin/bash
# WoS Translator 启动脚本

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "脚本目录：$SCRIPT_DIR"
echo "项目目录：$PROJECT_DIR"

# 切换到项目目录
cd "$PROJECT_DIR" || exit 1

# 创建必要的目录
mkdir -p config data logs web

# 检查可执行文件是否存在
if [ ! -f "./wos-translator" ]; then
    echo "错误: 找不到可执行文件 wos-translator"
    echo "请先编译项目: mkdir build && cd build && cmake .. && make"
    exit 1
fi

# 检查配置文件
if [ ! -f "config/system.json" ]; then
    echo "首次运行，将创建默认配置..."
fi

# 启动服务
echo "启动 WoS Translator..."
echo "日志文件: logs/app.log"
echo "按 Ctrl+C 停止服务"
echo ""

./wos-translator
