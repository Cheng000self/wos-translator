# ARM64 (AArch64) Linux交叉编译工具链配置
# 使用方法: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 指定交叉编译器
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 获取项目根目录（工具链文件所在目录的父目录）
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# 指定目标系统根目录（sysroot）
set(CMAKE_SYSROOT "${PROJECT_ROOT}/arm64-sysroot")
set(CMAKE_FIND_ROOT_PATH "${PROJECT_ROOT}/arm64-sysroot")

# 搜索程序的路径（使用主机系统的程序）
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# 搜索库和头文件的路径（只在sysroot中搜索）
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ARM64优化选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")

# 静态链接选项 - 生成不依赖 glibc 的可执行文件
# 使用 -static 完全静态链接，适用于 OpenWrt/musl 等非 glibc 系统
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

# 设置库搜索路径
set(CMAKE_LIBRARY_PATH 
    "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu"
    "${CMAKE_SYSROOT}/lib/aarch64-linux-gnu"
)

# 设置头文件搜索路径
set(CMAKE_INCLUDE_PATH
    "${CMAKE_SYSROOT}/usr/include"
    "${CMAKE_SYSROOT}/usr/include/aarch64-linux-gnu"
)

# 使用自编译的精简版静态 libcurl
set(CURL_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include")
set(CURL_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libcurl.a")

# 手动设置 OpenSSL 路径（静态库）
set(OPENSSL_ROOT_DIR "${CMAKE_SYSROOT}/usr")
set(OPENSSL_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include")
set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libcrypto.a")
set(OPENSSL_SSL_LIBRARY "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libssl.a")

# 设置 zlib 路径
set(ZLIB_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include")
set(ZLIB_LIBRARY "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libz.a")

# 静态链接需要的额外库（精简版curl只需要ssl, crypto, z）
set(CURL_EXTRA_LIBS "ssl;crypto;z;dl;pthread")
