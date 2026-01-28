# WoS Translator 编译教程

本文档详细介绍如何在不同平台上编译 WoS Translator。

## 目录

1. [编译环境准备](#编译环境准备)
2. [x86/x86_64 本地编译](#x86x86_64-本地编译)
3. [Windows x64 编译](#windows-x64-编译)
4. [ARM32 交叉编译](#arm32-交叉编译)
5. [ARM64 交叉编译](#arm64-交叉编译)
6. [嵌入式资源（单文件部署）](#嵌入式资源单文件部署)
7. [静态链接编译](#静态链接编译)
8. [编译选项说明](#编译选项说明)
9. [常见问题](#常见问题)

---

## 编译环境准备

### 基础工具

所有平台都需要以下基础工具：

| 工具 | 最低版本 | 说明 |
|------|----------|------|
| GCC/G++ | 7.0+ | C++17 支持 |
| CMake | 3.10+ | 构建系统 |
| Make | 4.0+ | 编译工具 |

### 依赖库

| 库 | 版本 | 说明 |
|----|------|------|
| libcurl | 7.0+ | HTTP 客户端 |
| OpenSSL | 1.1.0+ | 加密库（SHA-256） |
| pthread | - | 线程库（系统自带） |

---

## x86/x86_64 本地编译

### Ubuntu/Debian

```bash
# 1. 安装编译工具
sudo apt-get update
sudo apt-get install -y build-essential cmake

# 2. 安装依赖库
sudo apt-get install -y libcurl4-openssl-dev libssl-dev

# 3. 验证安装
gcc --version      # 应显示 7.0 或更高版本
cmake --version    # 应显示 3.10 或更高版本
```

### CentOS/RHEL 7

```bash
# 1. 安装开发工具
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake3

# 2. 安装依赖库
sudo yum install -y libcurl-devel openssl-devel

# 3. 如果 cmake 版本过低，使用 cmake3
sudo ln -sf /usr/bin/cmake3 /usr/bin/cmake
```

### CentOS/RHEL 8+

```bash
# 1. 安装开发工具
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y cmake

# 2. 安装依赖库
sudo dnf install -y libcurl-devel openssl-devel
```

### Arch Linux

```bash
# 安装所有依赖
sudo pacman -S base-devel cmake curl openssl
```

### Fedora

```bash
# 安装所有依赖
sudo dnf install -y gcc-c++ cmake libcurl-devel openssl-devel
```

### 编译步骤

```bash
# 方式一：使用构建脚本（推荐）
./scripts/build.sh

# 方式二：手动编译
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 编译完成后，可执行文件位于 build/wos-translator
```

### 验证编译结果

```bash
# 检查可执行文件
file build/wos-translator
# 输出示例: build/wos-translator: ELF 64-bit LSB executable, x86-64, ...

# 检查依赖库
ldd build/wos-translator
# 应显示 libcurl.so, libssl.so, libcrypto.so, libpthread.so 等

# 测试运行
./build/wos-translator --help 2>/dev/null || echo "程序可以运行"
```

---

## Windows x64 编译

适用于 Windows 10/11 x64 系统。

### 前置要求

| 工具 | 版本 | 说明 |
|------|------|------|
| Visual Studio | 2019/2022 | C++ 桌面开发工作负载 |
| CMake | 3.10+ | 构建系统 |
| vcpkg | 最新版 | 包管理器 |
| Python | 3.8+ | 嵌入资源脚本（可选） |

### 安装步骤

#### 1. 安装 Visual Studio

下载并安装 [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)，选择"使用 C++ 的桌面开发"工作负载。

#### 2. 安装 CMake

下载并安装 [CMake](https://cmake.org/download/)，安装时选择"Add CMake to the system PATH"。

#### 3. 安装 vcpkg

```powershell
# 克隆 vcpkg
git clone https://github.com/microsoft/vcpkg C:\vcpkg
cd C:\vcpkg

# 初始化 vcpkg
.\bootstrap-vcpkg.bat

# 设置环境变量（管理员权限）
setx VCPKG_ROOT "C:\vcpkg" /M
```

#### 4. 安装依赖库

```powershell
# 安装 curl 和 openssl
C:\vcpkg\vcpkg.exe install curl:x64-windows openssl:x64-windows
```

### 编译步骤

#### 方式一：使用构建脚本（推荐）

```batch
# 打开 "Developer Command Prompt for VS 2022"
cd path\to\wos-translator
scripts\build-windows.bat

# 带嵌入资源
scripts\build-windows.bat --embed

# Debug 构建
scripts\build-windows.bat --debug
```

#### 方式二：手动编译

```powershell
# 创建构建目录
mkdir build-windows
cd build-windows

# 配置（使用 vcpkg）
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows

# 编译
cmake --build . --config Release --parallel
```

### 验证编译结果

```powershell
# 检查可执行文件
dir build-windows\Release\wos-translator.exe

# 运行程序
build-windows\Release\wos-translator.exe
```

### 运行程序

```powershell
# 复制可执行文件到项目根目录
copy build-windows\Release\wos-translator.exe .

# 运行（需要 web 目录，除非使用 --embed 编译）
.\wos-translator.exe
```

访问 http://localhost:8080 即可使用。

### 注意事项

1. **DLL 依赖**: 动态链接版本需要 vcpkg 安装的 DLL 文件，建议将 `C:\vcpkg\installed\x64-windows\bin` 添加到 PATH
2. **防火墙**: 首次运行时 Windows 防火墙可能会提示，需要允许网络访问
3. **编码问题**: 确保终端使用 UTF-8 编码（`chcp 65001`）
4. **嵌入资源**: 使用 `--embed` 选项可避免 DLL 依赖问题

---

## ARM32 交叉编译

适用于 32 位 ARM 设备，如：
- Raspberry Pi 1/2/Zero
- 部分 OpenWrt 路由器
- 其他 ARMv7 设备

### 安装交叉编译工具链

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# 验证安装
arm-linux-gnueabihf-gcc --version
```

**Arch Linux:**
```bash
sudo pacman -S arm-linux-gnueabihf-gcc
```

### 安装 ARM 版依赖库

有两种方式获取 ARM 版依赖库：

#### 方式一：使用 multiarch（推荐）

```bash
# 添加 armhf 架构支持
sudo dpkg --add-architecture armhf
sudo apt-get update

# 安装 ARM 版依赖库
sudo apt-get install -y libcurl4-openssl-dev:armhf libssl-dev:armhf
```

#### 方式二：从目标设备获取

```bash
# 在目标 ARM 设备上安装依赖
opkg update
opkg install libcurl libssl

# 将库文件复制到编译机
scp root@arm-device:/usr/lib/libcurl* /path/to/sysroot/usr/lib/
scp root@arm-device:/usr/lib/libssl* /path/to/sysroot/usr/lib/
scp root@arm-device:/usr/lib/libcrypto* /path/to/sysroot/usr/lib/
scp -r root@arm-device:/usr/include/curl /path/to/sysroot/usr/include/
scp -r root@arm-device:/usr/include/openssl /path/to/sysroot/usr/include/
```

### 编译

```bash
# 方式一：使用构建脚本
./scripts/build.sh --arm

# 方式二：手动编译
mkdir -p build-arm
cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-linux-gnueabihf.cmake \
         -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 工具链配置文件说明

`cmake/arm-linux-gnueabihf.cmake` 内容：

```cmake
# ARM Linux 交叉编译工具链配置
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 指定交叉编译器
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# 如果使用 sysroot，取消注释并修改路径
# set(CMAKE_SYSROOT /path/to/arm/sysroot)

# 搜索路径设置
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ARM 优化选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
```

### 验证编译结果

```bash
# 检查可执行文件架构
file build-arm/wos-translator
# 输出示例: build-arm/wos-translator: ELF 32-bit LSB executable, ARM, EABI5, ...

# 检查依赖库（需要 ARM 版 ldd 或 qemu）
arm-linux-gnueabihf-readelf -d build-arm/wos-translator | grep NEEDED
```

---

## ARM64 交叉编译

适用于 64 位 ARM 设备，如：
- Raspberry Pi 3/4/5（64位系统）
- 部分高端 OpenWrt 路由器
- 其他 AArch64 设备

### 安装交叉编译工具链

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 验证安装
aarch64-linux-gnu-gcc --version
```

**Arch Linux:**
```bash
sudo pacman -S aarch64-linux-gnu-gcc
```

### 安装 ARM64 版依赖库

有三种方式获取 ARM64 版依赖库：

#### 方式一：使用 multiarch（可能有依赖冲突）

```bash
# 添加 arm64 架构支持
sudo dpkg --add-architecture arm64

# 创建 arm64 源配置
sudo tee /etc/apt/sources.list.d/arm64.list << 'EOF'
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse
EOF

sudo apt-get update

# 安装 ARM64 版依赖库
sudo apt-get install -y libcurl4-openssl-dev:arm64 libssl-dev:arm64
```

> **注意**: 如果遇到 i386 依赖冲突，请使用方式二。

#### 方式二：使用 sysroot（推荐，避免依赖冲突）

本项目已包含完整的 sysroot 构建支持：

```bash
# 1. 创建 sysroot 目录
mkdir -p arm64-sysroot/debs

# 2. 下载 ARM64 依赖包
apt-get download libc6:arm64 libc6-dev:arm64 linux-libc-dev:arm64
apt-get download libgcc-s1:arm64 libstdc++6:arm64 libstdc++-11-dev:arm64
apt-get download zlib1g:arm64 zlib1g-dev:arm64
apt-get download libssl3:arm64 libssl-dev:arm64

# 3. 解压到 sysroot
for deb in *.deb; do dpkg-deb -x "$deb" arm64-sysroot/; done

# 4. 编译精简版 libcurl（避免复杂依赖）
./scripts/build-curl-arm64.sh

# 5. 编译项目
./scripts/build.sh --arm64
```

#### 方式三：从目标设备获取

```bash
# 在目标 ARM64 设备上安装依赖
opkg update
opkg install libcurl libssl

# 将库文件复制到编译机
scp root@arm64-device:/usr/lib/libcurl* arm64-sysroot/usr/lib/
scp root@arm64-device:/usr/lib/libssl* arm64-sysroot/usr/lib/
scp root@arm64-device:/usr/lib/libcrypto* arm64-sysroot/usr/lib/
scp -r root@arm64-device:/usr/include/curl arm64-sysroot/usr/include/
scp -r root@arm64-device:/usr/include/openssl arm64-sysroot/usr/include/
```

### 编译

```bash
# 方式一：使用构建脚本（推荐）
./scripts/build.sh --arm64

# 方式二：手动编译
mkdir -p build-arm64
cd build-arm64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux-gnu.cmake \
         -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 工具链配置文件说明

`cmake/aarch64-linux-gnu.cmake` 已配置为使用项目内的 sysroot：

```cmake
# ARM64 (AArch64) Linux 交叉编译工具链配置
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 指定交叉编译器
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 使用项目内的 sysroot
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_SYSROOT "${PROJECT_ROOT}/arm64-sysroot")
set(CMAKE_FIND_ROOT_PATH "${PROJECT_ROOT}/arm64-sysroot")

# 搜索路径设置
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ARM64 优化选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
```

### 验证编译结果

```bash
# 检查可执行文件架构
file build-arm64/wos-translator
# 输出示例: build-arm64/wos-translator: ELF 64-bit LSB executable, ARM aarch64, ...

# 检查依赖库
aarch64-linux-gnu-objdump -p build-arm64/wos-translator | grep NEEDED
# 应只显示基本系统库: libz.so.1, libstdc++.so.6, libgcc_s.so.1, libc.so.6

# 检查文件大小
ls -lh build-arm64/wos-translator
# 约 6-7 MB
```

---

## 嵌入式资源（单文件部署）

默认情况下，程序运行时需要 `web/` 目录存放前端文件。通过嵌入式资源功能，可以将所有 web 文件打包到可执行文件中，实现单文件部署。

### 优势

- **单文件部署**: 只需复制一个可执行文件到目标设备
- **简化运维**: 无需管理 web 目录，减少文件丢失风险
- **适合嵌入式设备**: 特别适合存储空间有限的 OpenWrt 路由器

### 编译方法

```bash
# 方式一：使用构建脚本（推荐）
./scripts/build.sh --embed              # x86_64 嵌入式构建
./scripts/build.sh --arm64 --embed      # ARM64 嵌入式构建
./scripts/build.sh --arm --embed        # ARM32 嵌入式构建

# 方式二：手动编译
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DEMBED_RESOURCES=ON
make -j$(nproc)
```

### 工作原理

1. **资源扫描**: `scripts/embed_resources.py` 脚本扫描 `web/` 目录下所有文件
2. **代码生成**: 将文件内容转换为 C++ 字节数组，生成 `embedded_resources.h` 和 `embedded_resources.cpp`
3. **编译链接**: 资源代码与主程序一起编译，嵌入到可执行文件中
4. **运行时查找**: 程序优先从嵌入式资源查找文件，找不到时再从文件系统读取

### 支持的文件类型

| 类型 | 扩展名 | MIME 类型 |
|------|--------|-----------|
| HTML | .html, .htm | text/html |
| CSS | .css | text/css |
| JavaScript | .js | application/javascript |
| JSON | .json | application/json |
| 图片 | .png, .jpg, .gif, .svg, .ico | image/* |
| 字体 | .woff, .woff2, .ttf, .eot | font/* |

### 部署示例

```bash
# 编译嵌入式版本
./scripts/build.sh --arm64 --embed

# 部署到目标设备（只需一个文件）
scp build-arm64/wos-translator root@router:/usr/bin/

# 在目标设备上运行
ssh root@router
cd /usr/bin
./wos-translator
# 程序会自动创建 config/, data/, logs/ 目录
```

### 文件大小对比

| 构建方式 | 可执行文件 | web 目录 | 总大小 |
|----------|------------|----------|--------|
| 普通构建 | ~6 MB | ~500 KB | ~6.5 MB |
| 嵌入式构建 | ~6.5 MB | 不需要 | ~6.5 MB |

### 注意事项

1. **构建依赖**: 需要 Python 3 来运行资源嵌入脚本
2. **更新 web 文件**: 修改 web 文件后需要重新编译
3. **调试模式**: 开发时建议使用普通构建，方便修改前端文件
4. **混合模式**: 嵌入式版本仍会检查文件系统，可用于覆盖嵌入的资源

### 手动生成资源文件

如果需要手动生成嵌入式资源文件：

```bash
# 生成嵌入式资源
python3 scripts/embed_resources.py web src

# 生成空资源文件（用于普通构建）
python3 scripts/embed_resources.py web src --empty
```

---

## 静态链接编译

静态链接可以将所有依赖库打包到可执行文件中，使程序可以在没有安装依赖库的系统上运行。

### 优势

- **无依赖部署**: 目标系统无需安装 libcurl、OpenSSL 等库
- **跨发行版兼容**: 可在不同 Linux 发行版间移植
- **适合容器/嵌入式**: 特别适合 Docker 容器或嵌入式系统

### x86_64 静态编译

由于系统自带的 libcurl 静态库依赖过多（nghttp2、libssh、ldap、gssapi 等），需要先编译精简版 libcurl：

```bash
# 1. 下载 curl 源码（如果还没有）
wget https://curl.se/download/curl-8.18.0.tar.gz
tar xzf curl-8.18.0.tar.gz

# 2. 编译精简版静态 libcurl（仅 HTTPS 支持）
./scripts/build-curl-static.sh

# 3. 静态编译项目
./scripts/build.sh --static --embed --clean
```

编译完成后，可执行文件约 8-9 MB，完全静态链接。

### ARM64 静态编译

ARM64 交叉编译默认已启用静态链接（在 `cmake/aarch64-linux-gnu.cmake` 中配置）：

```bash
# ARM64 静态编译（已默认启用）
./scripts/build.sh --arm64 --embed --clean
```

### 验证静态链接

```bash
# 检查是否为静态链接
file build/wos-translator
# 输出应包含 "statically linked"

# 检查动态库依赖（静态链接应无输出或只有 linux-vdso）
ldd build/wos-translator
# 输出: not a dynamic executable
```

### 文件大小对比

| 构建方式 | 可执行文件大小 |
|----------|----------------|
| 动态链接 | ~6 MB |
| 动态链接 + 嵌入资源 | ~6.5 MB |
| 静态链接 + 嵌入资源 | ~8.9 MB |
| ARM64 静态链接 + 嵌入资源 | ~8.6 MB |

### 注意事项

1. **glibc 警告**: 静态链接时可能出现关于 `getaddrinfo`、`dlopen` 等函数的警告，这是正常的，程序仍可正常运行
2. **DNS 解析**: 静态链接的程序在某些系统上可能需要 `/etc/resolv.conf` 和 `/etc/nsswitch.conf` 文件
3. **编译时间**: 静态链接编译时间较长，因为需要链接更多代码
4. **调试困难**: 静态链接的程序调试较困难，开发时建议使用动态链接

---

## 编译选项说明

### 构建脚本选项

```bash
./scripts/build.sh [选项]

选项:
  --debug     Debug 构建（包含调试信息，无优化）
  --clean     清理后重新编译
  --arm       ARM32 交叉编译
  --arm64     ARM64 交叉编译
  --embed     嵌入 web 资源（单文件部署）
  --static    静态链接（需要先运行 build-curl-static.sh）
```

### CMake 选项

```bash
cmake .. [选项]

常用选项:
  -DCMAKE_BUILD_TYPE=Release    # Release 构建（默认）
  -DCMAKE_BUILD_TYPE=Debug      # Debug 构建
  -DCMAKE_TOOLCHAIN_FILE=...    # 指定工具链文件
  -DCMAKE_INSTALL_PREFIX=...    # 指定安装路径
  -DEMBED_RESOURCES=ON          # 嵌入 web 资源
  -DSTATIC_LINK=ON              # 静态链接
```

### 编译优化

Release 构建默认启用以下优化：
- `-O3`: 最高级别优化
- `-DNDEBUG`: 禁用断言
- `-march=native`: 针对本机 CPU 优化（本地编译）
- LTO (Link-Time Optimization): 链接时优化

Debug 构建：
- `-g`: 包含调试信息
- `-O0`: 无优化
- `-DDEBUG`: 启用调试宏

---

## 常见问题

### 1. CMake 版本过低

**问题**: `CMake 3.10 or higher is required`

**解决方案**:

Ubuntu 18.04+:
```bash
sudo apt-get install cmake
```

Ubuntu 16.04 或更早版本:
```bash
# 添加 Kitware APT 仓库
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main'
sudo apt-get update
sudo apt-get install cmake
```

### 2. 找不到 libcurl

**问题**: `Could NOT find CURL`

**解决方案**:
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# CentOS/RHEL
sudo yum install libcurl-devel

# 交叉编译时，确保安装了对应架构的库
sudo apt-get install libcurl4-openssl-dev:armhf  # ARM32
sudo apt-get install libcurl4-openssl-dev:arm64  # ARM64
```

### 3. 找不到 OpenSSL

**问题**: `Could NOT find OpenSSL`

**解决方案**:
```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# CentOS/RHEL
sudo yum install openssl-devel
```

### 4. 交叉编译找不到库

**问题**: 交叉编译时找不到 ARM 版本的库

**解决方案**:

方式一：使用 multiarch
```bash
sudo dpkg --add-architecture armhf  # 或 arm64
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev:armhf libssl-dev:armhf
```

方式二：使用 sysroot
```bash
# 编辑工具链文件，添加 sysroot 路径
# cmake/arm-linux-gnueabihf.cmake
set(CMAKE_SYSROOT /path/to/arm/sysroot)
set(CMAKE_FIND_ROOT_PATH /path/to/arm/sysroot)
```

### 5. C++17 不支持

**问题**: `error: 'filesystem' is not a namespace-name`

**解决方案**:
```bash
# 升级 GCC 到 7.0 或更高版本
sudo apt-get install gcc-7 g++-7
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 70
```

### 6. 链接错误

**问题**: `undefined reference to 'curl_easy_init'`

**解决方案**:
```bash
# 确保链接了 curl 库
# 检查 CMakeLists.txt 中是否有:
target_link_libraries(wos-translator ${CURL_LIBRARIES})
```

### 7. 运行时找不到库

**问题**: `error while loading shared libraries: libcurl.so.4`

**解决方案**:
```bash
# 方式一：安装运行时库
sudo apt-get install libcurl4

# 方式二：设置 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/libs:$LD_LIBRARY_PATH

# 方式三：静态链接（需要修改 CMakeLists.txt）
```

---

## 部署到目标设备

### 复制文件

```bash
# 复制可执行文件
scp build/wos-translator user@target:/path/to/install/

# 复制 web 文件
scp -r web user@target:/path/to/install/

# 在目标设备上创建必要目录
ssh user@target "mkdir -p /path/to/install/{config,data,logs}"
```

### 运行

```bash
# 在目标设备上
cd /path/to/install
./wos-translator
```

### 设置开机自启

参考 [DEPLOYMENT.md](DEPLOYMENT.md) 中的 systemd 服务配置或 OpenWrt init.d 脚本。

---

## 编译产物

成功编译后，会生成以下文件：

| 文件 | 说明 |
|------|------|
| `build/wos-translator` | 可执行文件 |

运行时需要的文件：

| 目录/文件 | 说明 |
|-----------|------|
| `web/` | 前端文件 |
| `config/` | 配置文件（自动创建） |
| `data/` | 数据目录（自动创建） |
| `logs/` | 日志目录（自动创建） |

---

## 参考资料

- [CMake 官方文档](https://cmake.org/documentation/)
- [GCC 交叉编译指南](https://gcc.gnu.org/onlinedocs/gcc/Cross-Compiler.html)
- [libcurl 官方文档](https://curl.se/libcurl/)
- [OpenSSL 官方文档](https://www.openssl.org/docs/)
