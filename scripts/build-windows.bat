@echo off
REM WoS Translator Windows 构建脚本
REM 需要: Visual Studio 2019/2022, CMake, vcpkg

setlocal enabledelayedexpansion

echo === WoS Translator Windows 构建脚本 ===
echo.

REM 检查 CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到 CMake，请安装 CMake 并添加到 PATH
    echo 下载地址: https://cmake.org/download/
    exit /b 1
)

REM 检查 vcpkg
if not defined VCPKG_ROOT (
    echo 警告: VCPKG_ROOT 环境变量未设置
    echo 尝试使用默认路径 C:\vcpkg
    set VCPKG_ROOT=C:\vcpkg
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo 错误: 未找到 vcpkg，请先安装 vcpkg
    echo.
    echo 安装步骤:
    echo   1. git clone https://github.com/microsoft/vcpkg C:\vcpkg
    echo   2. cd C:\vcpkg
    echo   3. bootstrap-vcpkg.bat
    echo   4. 设置环境变量 VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

REM 解析参数
set BUILD_TYPE=Release
set CLEAN=0
set EMBED_RESOURCES=0

:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if /i "%~1"=="--clean" (
    set CLEAN=1
    shift
    goto :parse_args
)
if /i "%~1"=="--embed" (
    set EMBED_RESOURCES=1
    shift
    goto :parse_args
)
echo 未知参数: %~1
echo 用法: build-windows.bat [--debug] [--clean] [--embed]
exit /b 1

:done_args

echo 构建类型: %BUILD_TYPE%
echo 嵌入资源: %EMBED_RESOURCES%
echo.

REM 安装依赖
echo 检查/安装依赖库...
"%VCPKG_ROOT%\vcpkg.exe" install curl:x64-windows openssl:x64-windows --triplet x64-windows
if %errorlevel% neq 0 (
    echo 错误: 依赖安装失败
    exit /b 1
)

REM 清理
if %CLEAN%==1 (
    echo 清理构建目录...
    if exist build-windows rmdir /s /q build-windows
)

REM 创建构建目录
if not exist build-windows mkdir build-windows
cd build-windows

REM 配置
echo 配置项目...
set CMAKE_OPTS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%
set CMAKE_OPTS=%CMAKE_OPTS% -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
set CMAKE_OPTS=%CMAKE_OPTS% -DVCPKG_TARGET_TRIPLET=x64-windows

if %EMBED_RESOURCES%==1 (
    set CMAKE_OPTS=%CMAKE_OPTS% -DEMBED_RESOURCES=ON
)

cmake .. %CMAKE_OPTS% -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo 错误: CMake 配置失败
    cd ..
    exit /b 1
)

REM 编译
echo 编译项目...
cmake --build . --config %BUILD_TYPE% --parallel
if %errorlevel% neq 0 (
    echo 错误: 编译失败
    cd ..
    exit /b 1
)

cd ..

echo.
echo === 构建完成 ===
echo 可执行文件: build-windows\%BUILD_TYPE%\wos-translator.exe
echo.

if %EMBED_RESOURCES%==1 (
    echo 已嵌入 web 资源，可执行文件为单文件部署模式
) else (
    echo 运行时需要 web 目录
)

echo.
echo 运行程序:
echo   build-windows\%BUILD_TYPE%\wos-translator.exe
echo.

endlocal
