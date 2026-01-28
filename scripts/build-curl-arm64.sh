#!/bin/bash
# 为 ARM64 交叉编译精简版 libcurl 静态库
# 只启用 HTTPS 支持，禁用其他所有可选功能

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CURL_SRC="$PROJECT_DIR/curl-8.18.0"
SYSROOT="$PROJECT_DIR/arm64-sysroot"
BUILD_DIR="$PROJECT_DIR/curl-build-arm64"
INSTALL_DIR="$SYSROOT/usr"

echo "=== 编译 ARM64 精简版 libcurl ==="
echo "源码目录: $CURL_SRC"
echo "Sysroot: $SYSROOT"
echo "安装目录: $INSTALL_DIR"

# 清理并创建构建目录
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

pushd "$BUILD_DIR" > /dev/null

# 使用 CMake 配置 curl
# 禁用所有不需要的功能，只保留 HTTPS
cmake "$CURL_SRC" \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_SYSROOT="$SYSROOT" \
    -DCMAKE_FIND_ROOT_PATH="$SYSROOT" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_C_FLAGS="-march=armv8-a" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_CURL_EXE=OFF \
    -DBUILD_TESTING=OFF \
    -DCURL_USE_OPENSSL=ON \
    -DOPENSSL_ROOT_DIR="$SYSROOT/usr" \
    -DOPENSSL_INCLUDE_DIR="$SYSROOT/usr/include" \
    -DOPENSSL_CRYPTO_LIBRARY="$SYSROOT/usr/lib/aarch64-linux-gnu/libcrypto.a" \
    -DOPENSSL_SSL_LIBRARY="$SYSROOT/usr/lib/aarch64-linux-gnu/libssl.a" \
    -DZLIB_INCLUDE_DIR="$SYSROOT/usr/include" \
    -DZLIB_LIBRARY="$SYSROOT/lib/aarch64-linux-gnu/libz.so.1.2.11" \
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
    -DUSE_LIBIDN2=OFF \
    -DCURL_USE_LIBSSH2=OFF \
    -DCURL_USE_LIBSSH=OFF \
    -DCURL_USE_LIBPSL=OFF \
    -DCURL_BROTLI=OFF \
    -DCURL_ZSTD=OFF \
    -DUSE_NGHTTP2=OFF \
    -DUSE_NGHTTP3=OFF \
    -DUSE_NGTCP2=OFF \
    -DUSE_QUICHE=OFF \
    -DCURL_USE_GSSAPI=OFF \
    -DENABLE_UNIX_SOCKETS=OFF

echo "编译 libcurl..."
make -j$(nproc)

echo "安装 libcurl..."
make install

popd > /dev/null

echo ""
echo "=== libcurl 编译完成 ==="
echo "静态库: $INSTALL_DIR/lib/libcurl.a"
echo "头文件: $INSTALL_DIR/include/curl/"
