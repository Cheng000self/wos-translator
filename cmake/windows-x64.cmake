# Windows x64 编译配置
# 用于 Visual Studio 2019/2022

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Windows 特定编译选项
if(MSVC)
    # 使用 UTF-8 编码
    add_compile_options(/utf-8)
    
    # 禁用一些警告
    add_compile_options(/wd4996)  # 禁用 deprecated 警告
    add_compile_options(/wd4267)  # 禁用 size_t 转换警告
    add_compile_options(/wd4244)  # 禁用类型转换警告
    
    # 启用多处理器编译
    add_compile_options(/MP)
    
    # 定义 Windows 版本
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# 链接 Winsock
if(WIN32)
    set(PLATFORM_LIBS ws2_32)
endif()
