# WoS Translator

Web of Science 学术文献批量翻译系统。C++17 单进程多线程后端 + 原生 HTML/JS/Tailwind 前端，支持 x86_64 和 ARM64 静态编译，单文件部署。

> 本项目使用 [Kiro](https://kiro.dev) 的 Claude Opus 4.5 模型辅助构建。

## 功能概览

### 文献解析与翻译
- 解析 Web of Science 导出的 HTML 文件（支持英文和中文版 WoS 页面）
- 自动提取标题、摘要、作者、DOI、来源、ISSN 等结构化字段
- 通过 OpenAI 兼容 API 进行英译中翻译
- 单任务支持上传多个 HTML 文件，合并处理

### 多模型调度
- 单任务可配置多个翻译模型，每个模型独立设置并发线程数
- 连续调度（work-stealing）：模型 A 翻译完分配的文献后自动领取剩余待翻译项
- 支持 OpenAI、小米 MiMo、MiniMAX 等模型模板，也支持任意 OpenAI 兼容 API（Ollama 等）
- 模型级别并发控制，避免单一 API 过载

### 任务管理
- 实时进度追踪，支持暂停/恢复
- 翻译失败项可重试（保留原模型或换模型）
- 整个任务可重置，重新选择模型全量重翻
- 日历视图按日期浏览任务，标注每日任务数量
- 全局搜索：按任务名、文件名、ID、模型名检索
- 列表/网格两种布局，软删除 + 永久清理

### 文献浏览与导出
- 列表视图 + 详情视图，支持键盘翻页（PgUp/PgDn）
- 可选显示英文原文，字体大小可调
- 导出格式：HTML、JSON、CSV、TXT、PDF（打印式，每页一篇文献）
- 全选/清空选择，按需导出

### 工具
- HTML 转 JSON：无需翻译，直接将 WoS HTML 转为结构化数据

### 安全
- SHA-256 + 盐值密码哈希
- 会话管理（可配置超时）
- 登录失败锁定（防暴力破解）

## 系统要求

| 项目 | 最低要求 |
|------|----------|
| CPU | 单核 ARM Cortex-A53 或 x86_64 |
| 内存 | 128MB（推荐 256MB+） |
| 存储 | 50MB（不含数据） |
| 系统 | Linux（Ubuntu、Debian、CentOS、OpenWrt 等） |

## 快速开始

### 方式一：下载预编译版本

从 [Releases](../../releases) 页面下载对应架构的可执行文件，直接运行：

```bash
chmod +x wos-translator
./wos-translator
```

程序启动后访问 `http://localhost:8080`，默认密码 `admin123`（请立即修改）。

### 方式二：从源码编译

```bash
# 安装依赖（Ubuntu/Debian）
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libssl-dev

# 编译（静态链接 + 嵌入资源 = 单文件部署）
bash scripts/build.sh --static --embed --clean

# 运行
./build/wos-translator
```

详细编译说明见 [BUILD.md](BUILD.md)。

## 使用流程

### 1. 配置模型

访问「模型」页面，添加翻译模型。支持三种模板：

| 模板 | 说明 | URL 示例 |
|------|------|----------|
| OpenAI | OpenAI 及兼容 API | `https://api.openai.com/v1` |
| 小米 MiMo | 小米大模型 | `https://api.xiaomimimo.com/v1/chat/completions` |
| MiniMAX | MiniMAX 大模型 | `https://api.minimaxi.com/v1/chat/completions` |

也可选择「自定义」手动填写任意 OpenAI 兼容 API 地址（如 Ollama `http://localhost:11434/v1`）。

### 2. 创建翻译任务

1. 从 Web of Science 导出 HTML 格式的检索结果
2. 访问「翻译」页面，上传 HTML 文件
3. 选择翻译选项（标题/摘要）和模型（可多选，分别设置线程数）
4. 点击「创建翻译任务」

### 3. 查看结果

- 「队列」页面查看所有任务进度
- 点击任务进入详情页，浏览翻译结果
- 选择文献后导出为所需格式

## 项目结构

```
wos-translator/
├── src/                          # C++ 后端源码
│   ├── main.cpp                  # 程序入口
│   ├── web_server.cpp            # HTTP 服务器 + REST API
│   ├── html_parser.cpp           # WoS HTML 解析器
│   ├── translator.cpp            # AI 翻译引擎
│   ├── task_queue.cpp            # 任务调度器（多模型连续调度）
│   ├── storage_manager.cpp       # 文件存储管理
│   ├── config_manager.cpp        # 配置管理
│   ├── exporter.cpp              # 多格式导出
│   └── logger.cpp                # 日志系统
├── include/                      # 头文件
├── web/                          # 前端（原生 HTML + Tailwind CSS + Vanilla JS）
│   ├── index.html                # 主页
│   ├── translate.html            # 创建翻译任务
│   ├── queue.html                # 任务队列（日历 + 搜索）
│   ├── task.html                 # 任务详情（浏览 + 导出 + 重试）
│   ├── models.html               # 模型管理
│   ├── convert.html              # HTML 转 JSON 工具
│   ├── settings.html             # 系统设置
│   └── assets/js/                # JavaScript 模块
├── scripts/                      # 构建和部署脚本
│   ├── build.sh                  # 统一构建脚本
│   ├── build-curl-static.sh      # x86 静态 libcurl 编译
│   ├── build-curl-arm64.sh       # ARM64 静态 libcurl 编译
│   ├── embed_resources.py        # Web 资源嵌入工具
│   └── start.sh                  # 启动脚本
├── cmake/                        # 交叉编译工具链
│   ├── aarch64-linux-gnu.cmake   # ARM64 工具链
│   ├── arm-linux-gnueabihf.cmake # ARM32 工具链
│   └── windows-x64.cmake         # Windows 工具链
├── nlohmann/                     # JSON 库 
└── openai.hpp					  # openai 库
```

## 配置说明

### 系统配置（config/system.json）

首次运行自动创建，也可通过 Web 界面的「设置」页面修改：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `serverPort` | 8080 | HTTP 服务端口 |
| `maxUploadFiles` | 1 | 单任务最大上传文件数 |
| `maxConcurrentTasks` | 10 | 最大并发任务数 |
| `maxConcurrentTasksPerModel` | 5 | 同一模型最大并发任务数 |
| `maxModelsPerTask` | 5 | 单任务最多使用模型数 |
| `maxRetries` | 3 | API 调用重试次数 |
| `consecutiveFailureThreshold` | 5 | 连续失败自动暂停阈值 |
| `sessionTimeoutMinutes` | 30 | 登录会话超时（分钟） |

### 模型配置（config/models.json）

通过 Web 界面的「模型」页面管理，支持：
- 多模型配置，每个模型独立的 URL、API Key、温度等参数
- 模型模板快速配置（OpenAI / 小米 / MiniMAX）
- 连接测试
- 可选系统提示词和思考模式

## 部署

### 单文件部署（推荐）

使用 `--static --embed` 编译后，可执行文件包含所有 Web 资源，无外部依赖：

```bash
# 复制到目标机器
scp build/wos-translator user@server:/opt/

# 运行（自动创建 config/、data/、logs/ 目录）
./wos-translator
```

### systemd 服务

```bash
sudo ./scripts/install.sh
sudo systemctl enable --now wos-translator
```

### OpenWrt

```bash
scp build-arm64/wos-translator root@router:/usr/bin/
# 使用 scripts/install-openwrt.sh 安装 init.d 服务
```

### Nginx 反向代理（HTTPS）

```nginx
server {
    listen 443 ssl;
    server_name your-domain.com;
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    client_max_body_size 50M;
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

更多部署方式见 [DEPLOYMENT.md](DEPLOYMENT.md)。

## 技术栈

| 组件 | 技术 |
|------|------|
| 后端 | C++17，单进程多线程 |
| HTTP 服务器 | 自实现（POSIX socket） |
| HTTP 客户端 | libcurl |
| 加密 | OpenSSL（SHA-256） |
| JSON | [nlohmann/json](https://github.com/nlohmann/json)（header-only） |
| 前端 | HTML + [Tailwind CSS](https://tailwindcss.com)（CDN）+ Vanilla JavaScript |
| 构建 | CMake 3.10+ |

## 引用

- [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++
- [libcurl](https://curl.se/libcurl/) - HTTP 客户端库
- [openai-cpp](https://github.com/olrea/openai-cpp) - API 调用参考

## 许可证

MIT License
