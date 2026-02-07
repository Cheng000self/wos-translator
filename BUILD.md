# 编译指南

本文档介绍 WoS Translator 在各平台上的编译方法。

## 编译依赖

| 依赖 | 最低版本 | 说明 |
|------|----------|------|
| GCC/G++ | 7.0+ | C++17 支持 |
| CMake | 3.10+ | 构建系统 |
| libcurl | 7.0+ | HTTP 客户端 |
| OpenSSL | 1.1.0+ | SHA-256 加密 |
| Python3 | 3.6+ | 资源嵌入脚本（`--embed` 时需要） |

## 构建脚本

项目提供统一构建脚本 `scripts/build.sh`，支持以下选项：

```bash
bash scripts/build.sh [选项]

选项：
  --clean     清理后重新编译
  --debug     Debug 构建（含调试信息）
  --embed     嵌入 Web 资源到可执行文件（单文件部署）
  --static    静态链接（无外部库依赖）
  --arm       ARM32 交叉编译
  --arm64     ARM64 交叉编译
```

常用组合：

```bash
# 开发调试（动态链接，需要 web/ 目录）
bash scripts/build.sh --debug --clean

# x86_64 生产构建（静态 + 嵌入 = 单文件）
bash scripts/build.sh --static --embed --clean

# ARM64 生产构建
bash scripts/build.sh --arm64 --embed --clean
```

## x86_64 编译

### 安装依赖

Ubuntu/Debian：
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libssl-dev python3
```

CentOS/RHEL：
```bash
sudo yum install -y gcc-c++ cmake libcurl-devel openssl-devel python3
```

Arch Linux：
```bash
sudo pacman -S base-devel cmake curl openssl python
```

### 动态链接编译

```bash
bash scripts/build.sh --embed --clean
```

产物：`build/wos-translator`（约 6.5MB），运行时需要系统安装 libcurl 和 OpenSSL。

### 静态链接编译

静态编译需要先构建精简版 libcurl 静态库（仅保留 HTTPS 支持，去除 nghttp2、libssh 等不必要依赖）：

```bash
# 1. 下载 curl 源码（项目已包含 curl-8.18.0/）
#    如果没有，手动下载：
#    wget https://curl.se/download/curl-8.18.0.tar.gz && tar xzf curl-8.18.0.tar.gz

# 2. 编译精简版静态 libcurl
bash scripts/build-curl-static.sh

# 3. 静态编译项目
bash scripts/build.sh --static --embed --clean
```

产物：`build/wos-translator`（约 9MB），完全静态链接，可在任意 x86_64 Linux 上运行。

验证：
```bash
file build/wos-translator
# ELF 64-bit LSB executable, x86-64, ... statically linked ...

ldd build/wos-translator
# not a dynamic executable
```

## ARM64 交叉编译

适用于 Raspberry Pi 3/4/5（64位）、ARM64 路由器等设备。

### 安装交叉编译工具链

```bash
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### 准备 sysroot

项目已包含 `arm64-sysroot/` 目录，内含 ARM64 版 glibc、zlib、OpenSSL 头文件和库。如需重建：

```bash
mkdir -p arm64-sysroot/debs

# 下载 ARM64 依赖包
apt-get download libc6:arm64 libc6-dev:arm64 linux-libc-dev:arm64
apt-get download libgcc-s1:arm64 libstdc++6:arm64 libstdc++-11-dev:arm64
apt-get download zlib1g:arm64 zlib1g-dev:arm64
apt-get download libssl3:arm64 libssl-dev:arm64

# 解压
for deb in *.deb; do dpkg-deb -x "$deb" arm64-sysroot/; done

# 编译 ARM64 版静态 libcurl
bash scripts/build-curl-arm64.sh
```

### 编译

```bash
bash scripts/build.sh --arm64 --embed --clean
```

产物：`build-arm64/wos-translator`（约 8.6MB），静态链接。

验证：
```bash
file build-arm64/wos-translator
# ELF 64-bit LSB executable, ARM aarch64, ... statically linked ...
```

### 部署到目标设备

```bash
scp build-arm64/wos-translator root@device:/usr/bin/
ssh root@device "./usr/bin/wos-translator"
```

## ARM32 交叉编译

适用于 Raspberry Pi 1/2/Zero、ARMv7 路由器等设备。

```bash
# 安装工具链
sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# 安装 ARM32 版依赖库
sudo dpkg --add-architecture armhf
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev:armhf libssl-dev:armhf

# 编译
bash scripts/build.sh --arm --embed --clean
```

## 嵌入式资源说明

使用 `--embed` 选项时，`scripts/embed_resources.py` 会将 `web/` 目录下所有文件转换为 C++ 字节数组，编译进可执行文件。运行时无需 `web/` 目录。

工作流程：
1. 扫描 `web/` 目录，收集所有 HTML、JS、CSS、图片等文件
2. 生成 `src/embedded_resources.h` 和 `src/embedded_resources.cpp`
3. 编译时链接到可执行文件
4. 运行时 HTTP 服务器优先从嵌入资源响应，找不到时回退到文件系统

不使用 `--embed` 时，程序从 `web/` 目录读取前端文件，适合开发阶段频繁修改前端。

## 手动编译（不使用构建脚本）

```bash
mkdir -p build
cd build

# 基础编译
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 带嵌入资源
cmake .. -DCMAKE_BUILD_TYPE=Release -DEMBED_RESOURCES=ON
make -j$(nproc)

# 静态链接 + 嵌入资源
cmake .. -DCMAKE_BUILD_TYPE=Release -DEMBED_RESOURCES=ON -DSTATIC_LINK=ON
make -j$(nproc)

# ARM64 交叉编译
cmake .. -DCMAKE_BUILD_TYPE=Release -DEMBED_RESOURCES=ON \
         -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux-gnu.cmake
make -j$(nproc)
```

## 编译产物

| 构建方式 | 产物路径 | 大小 | 外部依赖 |
|----------|----------|------|----------|
| x86_64 动态链接 | `build/wos-translator` | ~6MB | libcurl, OpenSSL |
| x86_64 动态 + 嵌入 | `build/wos-translator` | ~6.5MB | libcurl, OpenSSL |
| x86_64 静态 + 嵌入 | `build/wos-translator` | ~9MB | 无 |
| ARM64 静态 + 嵌入 | `build-arm64/wos-translator` | ~8.6MB | 无 |

运行时自动创建的目录：
- `config/` — 系统配置和模型配置
- `data/` — 任务数据（按日期组织）
- `logs/` — 日志文件

## 常见问题

### CMake 版本过低

Ubuntu 16.04 等旧系统可能 CMake 版本不足 3.10：
```bash
# 使用 pip 安装新版
pip3 install cmake
```

### 找不到 libcurl / OpenSSL

```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libssl-dev

# CentOS
sudo yum install libcurl-devel openssl-devel
```

### 静态链接时的 glibc 警告

静态编译时会出现关于 `getaddrinfo`、`dlopen` 等函数的警告，这是 glibc 的已知限制，不影响程序正常运行。

### 修改前端后可执行文件未更新

嵌入式构建时，修改 `web/` 下的文件后需要重新编译。使用 `--clean` 确保资源重新生成：
```bash
bash scripts/build.sh --static --embed --clean
```
