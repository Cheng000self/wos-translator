cmake_minimum_required(VERSION 3.10)
project(wos-translator VERSION 1.0.0 LANGUAGES CXX)

# 设置C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认构建类型为Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 平台检测
if(WIN32)
    message(STATUS "Building for Windows")
    # Windows 编译选项
    if(MSVC)
        add_compile_options(/utf-8)      # UTF-8 编码
        add_compile_options(/MP)         # 多处理器编译
        add_compile_options(/wd4996)     # 禁用 deprecated 警告
        add_compile_options(/wd4267)     # 禁用 size_t 转换警告
        add_compile_options(/wd4244)     # 禁用类型转换警告
        add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
        add_definitions(-DWIN32_LEAN_AND_MEAN)
        add_definitions(-DNOMINMAX)
    endif()
    set(PLATFORM_LIBS ws2_32)
else()
    message(STATUS "Building for Linux/Unix")
    # Linux/Unix 编译选项
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    # 针对本地编译使用 -march=native，交叉编译时由工具链文件指定
    if(NOT CMAKE_CROSSCOMPILING)
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(PLATFORM_LIBS "")
endif()

# 针对ARM的优化选项（交叉编译时不使用 -mcpu=native）
if(NOT WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        if(NOT CMAKE_CROSSCOMPILING)
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=native")
        endif()
        message(STATUS "Building for ARM architecture")
    endif()
endif()

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/nlohmann)

# 查找libcurl
if(CMAKE_CROSSCOMPILING AND DEFINED CURL_LIBRARY)
    # 交叉编译时使用工具链文件中指定的路径
    set(CURL_LIBRARIES ${CURL_LIBRARY})
    message(STATUS "Using cross-compile CURL: ${CURL_LIBRARY}")
elseif(STATIC_LINK)
    # 静态链接时使用自编译的精简版 libcurl
    set(STATIC_CURL_LIB "${CMAKE_SOURCE_DIR}/static-libs/lib/libcurl.a")
    if(EXISTS ${STATIC_CURL_LIB})
        set(CURL_LIBRARIES ${STATIC_CURL_LIB})
        set(CURL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/static-libs/include")
        message(STATUS "Using static CURL: ${STATIC_CURL_LIB}")
    else()
        message(FATAL_ERROR "Static libcurl not found. Run ./scripts/build-curl-static.sh first")
    endif()
else()
    find_package(CURL REQUIRED)
endif()
include_directories(${CURL_INCLUDE_DIR})

# 查找OpenSSL（用于SHA-256哈希）
if(CMAKE_CROSSCOMPILING AND DEFINED OPENSSL_SSL_LIBRARY)
    # 交叉编译时使用工具链文件中指定的路径
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    message(STATUS "Using cross-compile OpenSSL: ${OPENSSL_SSL_LIBRARY}")
elseif(STATIC_LINK)
    # 静态链接时查找静态库
    find_library(SSL_STATIC_LIB libssl.a PATHS /usr/lib/x86_64-linux-gnu /usr/lib)
    find_library(CRYPTO_STATIC_LIB libcrypto.a PATHS /usr/lib/x86_64-linux-gnu /usr/lib)
    if(SSL_STATIC_LIB AND CRYPTO_STATIC_LIB)
        set(OPENSSL_LIBRARIES ${SSL_STATIC_LIB} ${CRYPTO_STATIC_LIB})
        message(STATUS "Using static OpenSSL: ${SSL_STATIC_LIB}")
    else()
        message(FATAL_ERROR "Static OpenSSL not found. Install libssl-dev")
    endif()
else()
    find_package(OpenSSL REQUIRED)
endif()
include_directories(${OPENSSL_INCLUDE_DIR})

# 查找线程库
find_package(Threads REQUIRED)

# 嵌入式资源选项
option(EMBED_RESOURCES "Embed web resources into executable" OFF)

# 静态链接选项
option(STATIC_LINK "Build with static linking" OFF)

# 源文件
set(SOURCES
    src/main.cpp
    src/logger.cpp
    src/config_manager.cpp
    src/html_parser.cpp
    src/translator.cpp
    src/storage_manager.cpp
    src/task_queue.cpp
    src/web_server.cpp
    src/exporter.cpp
)

# 如果启用嵌入式资源
if(EMBED_RESOURCES)
    message(STATUS "Embedding web resources into executable")
    
    # 生成嵌入式资源文件
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    
    set(EMBED_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/embed_resources.py")
    set(WEB_DIR "${CMAKE_SOURCE_DIR}/web")
    set(EMBED_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/src")
    set(EMBED_HEADER "${EMBED_OUTPUT_DIR}/embedded_resources.h")
    set(EMBED_SOURCE "${EMBED_OUTPUT_DIR}/embedded_resources.cpp")
    
    # 收集所有web文件作为依赖
    file(GLOB_RECURSE WEB_FILES "${WEB_DIR}/*")
    
    # 添加自定义命令生成嵌入式资源
    add_custom_command(
        OUTPUT ${EMBED_HEADER} ${EMBED_SOURCE}
        COMMAND ${Python3_EXECUTABLE} ${EMBED_SCRIPT} ${WEB_DIR} ${EMBED_OUTPUT_DIR}
        DEPENDS ${EMBED_SCRIPT} ${WEB_FILES}
        COMMENT "Generating embedded resources..."
    )
    
    # 添加嵌入式资源源文件
    list(APPEND SOURCES ${EMBED_SOURCE})
    
    # 添加编译定义
    add_definitions(-DEMBED_RESOURCES)
else()
    message(STATUS "Web resources will be loaded from filesystem")
    
    # 生成空的嵌入式资源文件（避免编译错误）
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/embed_resources.py 
                    ${CMAKE_SOURCE_DIR}/web ${CMAKE_SOURCE_DIR}/src --empty
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
endif()

# 可执行文件
add_executable(wos-translator ${SOURCES})

# 静态链接设置
if(STATIC_LINK AND NOT CMAKE_CROSSCOMPILING AND NOT WIN32)
    message(STATUS "Building with static linking")
    # 设置静态链接标志
    set_target_properties(wos-translator PROPERTIES LINK_FLAGS "-static")
    # 静态链接需要额外的库
    target_link_libraries(wos-translator
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        Threads::Threads
        z       # zlib
        dl      # dlopen
    )
elseif(WIN32)
    # Windows 链接库
    target_link_libraries(wos-translator
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        Threads::Threads
        ${PLATFORM_LIBS}
    )
else()
    # 链接库
    target_link_libraries(wos-translator
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        Threads::Threads
    )
endif()

# 交叉编译时添加静态链接所需的额外库
if(CMAKE_CROSSCOMPILING AND DEFINED CURL_EXTRA_LIBS)
    target_link_libraries(wos-translator ${CURL_EXTRA_LIBS})
endif()

# 链接时优化（LTO）
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_property(TARGET wos-translator PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link-time optimization enabled")
    else()
        message(STATUS "Link-time optimization not supported: ${ipo_error}")
    endif()
endif()

# 安装规则
install(TARGETS wos-translator DESTINATION bin)
install(DIRECTORY web/ DESTINATION share/wos-translator/web)
install(DIRECTORY DESTINATION var/wos-translator/data)
install(DIRECTORY DESTINATION var/wos-translator/config)
install(DIRECTORY DESTINATION var/wos-translator/logs)

# 打印构建信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "C++ flags (Debug): ${CMAKE_CXX_FLAGS_DEBUG}")
