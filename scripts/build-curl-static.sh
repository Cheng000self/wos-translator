#!/bin/bash
# 编译精简版静态 libcurl（仅 HTTPS 支持）
# 用于 x86_64 静态链接构建

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CURL_VERSION="8.18.0"
CURL_DIR="$PROJECT_DIR/curl-$CURL_VERSION"
BUILD_DIR="$PROJECT_DIR/curl-build-static"
INSTALL_DIR="$PROJECT_DIR/static-libs"

echo "=== 编译精简版静态 libcurl (x86_64) ==="
echo "curl 版本: $CURL_VERSION"
echo "源码目录: $CURL_DIR"
echo "构建目录: $BUILD_DIR"
echo "安装目录: $INSTALL_DIR"

# 检查 curl 源码
if [ ! -d "$CURL_DIR" ]; then
    echo "错误: curl 源码目录不存在: $CURL_DIR"
    echo "请下载 curl 源码: https://curl.se/download.html"
    exit 1
fi

# 清理旧的构建
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_DIR"

cd "$BUILD_DIR"

echo ""
echo "配置 curl..."
# 使用 cmake 配置，禁用不需要的功能
cmake "$CURL_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_CURL_EXE=OFF \
    -DCURL_USE_OPENSSL=ON \
    -DCURL_USE_LIBSSH=OFF \
    -DCURL_USE_LIBSSH2=OFF \
    -DCURL_USE_GSSAPI=OFF \
    -DCURL_USE_LIBPSL=OFF \
    -DCURL_DISABLE_LDAP=ON \
    -DCURL_DISABLE_LDAPS=ON \
    -DCURL_DISABLE_RTSP=ON \
    -DCURL_DISABLE_DICT=ON \
    -DCURL_DISABLE_TELNET=ON \
    -DCURL_DISABLE_TFTP=ON \
    -DCURL_DISABLE_POP3=ON \
    -DCURL_DISABLE_IMAP=ON \
    -DCURL_DISABLE_SMTP=ON \
    -DCURL_DISABLE_GOPHER=ON \
    -DCURL_DISABLE_SMB=ON \
    -DCURL_DISABLE_MQTT=ON \
    -DUSE_NGHTTP2=OFF \
    -DUSE_LIBIDN2=OFF \
    -DCURL_BROTLI=OFF \
    -DCURL_ZSTD=OFF \
    -DENABLE_UNIX_SOCKETS=OFF

echo ""
echo "编译 curl..."
make -j$(nproc)

echo ""
echo "安装 curl..."
make install

echo ""
echo "=== 编译完成 ==="
echo "静态库: $INSTALL_DIR/lib/libcurl.a"
ls -lh "$INSTALL_DIR/lib/libcurl.a"
