#!/bin/bash
# WoS Translator 构建脚本

set -e

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# 解析参数
BUILD_TYPE="Release"
CLEAN=false
TOOLCHAIN=""
BUILD_DIR="build"
ARCH_NAME="x86_64"
EMBED_RESOURCES=false
STATIC_LINK=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --arm)
            TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake"
            BUILD_DIR="build-arm"
            ARCH_NAME="ARM32"
            shift
            ;;
        --arm64)
            TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake"
            BUILD_DIR="build-arm64"
            ARCH_NAME="ARM64"
            shift
            ;;
        --embed)
            EMBED_RESOURCES=true
            shift
            ;;
        --static)
            STATIC_LINK=true
            shift
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: $0 [--debug] [--clean] [--arm] [--arm64] [--embed] [--static]"
            exit 1
            ;;
    esac
done

echo "=== WoS Translator 构建脚本 ==="
echo "构建类型: $BUILD_TYPE"
echo "目标架构: $ARCH_NAME"
echo "构建目录: $BUILD_DIR"
echo "嵌入资源: $EMBED_RESOURCES"
echo "静态链接: $STATIC_LINK"

# 清理
if [ "$CLEAN" = true ]; then
    echo "清理构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" > /dev/null

# 配置
echo "配置项目..."
CMAKE_OPTS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE $TOOLCHAIN"
if [ "$EMBED_RESOURCES" = true ]; then
    CMAKE_OPTS="$CMAKE_OPTS -DEMBED_RESOURCES=ON"
fi
if [ "$STATIC_LINK" = true ]; then
    CMAKE_OPTS="$CMAKE_OPTS -DSTATIC_LINK=ON"
fi
cmake .. $CMAKE_OPTS

# 编译
echo "编译项目..."
make -j$(nproc)

popd > /dev/null

echo ""
echo "=== 构建完成 ==="
echo "可执行文件: $BUILD_DIR/wos-translator"
echo ""
echo "验证可执行文件架构:"
file "$BUILD_DIR/wos-translator"
echo ""
if [ "$EMBED_RESOURCES" = true ]; then
    echo "已嵌入 web 资源，可执行文件为单文件部署模式"
    echo "无需复制 web 目录"
    echo ""
fi
if [ "$ARCH_NAME" = "x86_64" ]; then
    echo "运行程序:"
    echo "  ./$BUILD_DIR/wos-translator"
    echo ""
    echo "或使用启动脚本:"
    echo "  cp $BUILD_DIR/wos-translator . && ./scripts/start.sh"
else
    echo "部署到目标设备:"
    if [ "$EMBED_RESOURCES" = true ]; then
        echo "  scp $BUILD_DIR/wos-translator user@target:/path/to/install/"
        echo "  # 单文件部署，无需复制 web 目录"
    else
        echo "  scp $BUILD_DIR/wos-translator user@target:/path/to/install/"
        echo "  scp -r web user@target:/path/to/install/"
    fi
fi
