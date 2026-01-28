# WoS Translator - Web of Science 文献翻译系统

一个用于翻译 Web of Science 导出的学术文献 HTML 文件的轻量级系统。采用 C++ 后端和 HTML + Tailwind CSS 前端，专为低资源环境优化设计。

本项目是完全使用kiro的Claude Opus 4.5模型构建，Credits使用量约为500
编译引用:
[openai-cpp](https://github.com/olrea/openai-cpp)
[libcurl](https://curl.se/libcurl/)
前端skills:
[UI UX Pro Max](https://ui.cod.ndjp.net/)
注意：README.md 文件也是使用Claude Opus 4.5模型构建。

## 功能特性

### 核心功能
- **HTML 解析**: 自动解析 Web of Science 导出的 HTML 文件，提取文献标题、摘要、作者等信息
- **AI 翻译**: 支持 OpenAI API 及兼容接口（如 Ollama、本地模型），进行英文到中文的高质量翻译
- **任务管理**: 支持多任务并发处理，实时进度追踪，暂停/恢复/删除任务
- **多格式导出**: 支持 TXT、JSON、CSV、HTML 四种导出格式
- **HTML 转 JSON**: 无需翻译，直接将 WoS HTML 转换为结构化 JSON 数据

### 高级功能
- **多文件上传**: 单个任务支持上传多个 HTML 文件（可配置）
- **任务命名**: 支持自定义任务名称（支持中文），方便管理
- **并发控制**: 
  - 总体并发任务数限制
  - 同一模型并发任务数限制（不同模型可并行）
  - 任务内多线程翻译
- **软删除机制**: 删除任务时文件保留，可在设置中永久清理
- **存储管理**: 查看数据文件夹占用空间，清理已删除任务

### 安全功能
- SHA-256 密码哈希 + 盐值
- 会话管理（可配置超时时间）
- 防暴力破解（登录失败锁定）
- 输入验证和清理
- API 密钥文件权限保护

### 系统特性
- **轻量高效**: 内存占用 < 50MB，适合嵌入式设备
- **响应式界面**: 现代化 Web 界面，支持移动设备
- **日志管理**: 自动轮转，保留备份
- **错误处理**: 自动重试机制，连续失败自动暂停

## 系统要求

### 最低配置
| 项目 | 要求 |
|------|------|
| CPU | 单核 ARM Cortex-A7 或 x86_64 |
| 内存 | 128MB RAM（推荐 256MB+） |
| 存储 | 50MB 可用空间（不含数据） |
| 系统 | Linux (OpenWrt, Ubuntu, Debian 等) |

### 编译依赖
- C++17 编译器 (GCC 7+, Clang 5+)
- CMake 3.10+
- libcurl (>= 7.0)
- OpenSSL (>= 1.1.0)
- pthread

## 快速开始

### 1. 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libssl-dev
```

**CentOS/RHEL:**
```bash
sudo yum install -y gcc-c++ cmake libcurl-devel openssl-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake curl openssl
```

### 2. 编译

```bash
# 克隆项目
git clone <repository-url>
cd wos-translator

# 编译（x86/x86_64）
./scripts/build.sh

# 复制可执行文件到项目根目录
cp build/wos-translator .
```

### 3. 运行

```bash
# 启动服务
./scripts/start.sh
```

### 4. 访问

打开浏览器访问: `http://localhost:8080`

**默认密码**: `admin123`（请立即修改！）

## 项目结构

```
wos-translator/
├── src/                      # C++ 源代码
│   ├── main.cpp             # 主程序入口
│   ├── html_parser.cpp      # HTML 解析器
│   ├── translator.cpp       # 翻译引擎
│   ├── task_queue.cpp       # 任务队列管理
│   ├── web_server.cpp       # Web 服务器
│   ├── storage_manager.cpp  # 存储管理
│   ├── config_manager.cpp   # 配置管理
│   ├── exporter.cpp         # 导出功能
│   └── logger.cpp           # 日志系统
├── include/                  # 头文件
├── web/                      # 前端文件
│   ├── index.html           # 主页
│   ├── translate.html       # 翻译页面
│   ├── queue.html           # 任务队列
│   ├── task.html            # 任务详情
│   ├── models.html          # 模型管理
│   ├── convert.html         # HTML 转 JSON
│   ├── settings.html        # 系统设置
│   ├── donate.html          # 捐赠页面
│   └── assets/              # 静态资源 (JS/CSS)
├── scripts/                  # 部署脚本
│   ├── build.sh             # 构建脚本
│   ├── start.sh             # 启动脚本
│   ├── install.sh           # 安装脚本
│   └── wos-translator.service # systemd 服务文件
├── cmake/                    # CMake 工具链文件
│   ├── arm-linux-gnueabihf.cmake   # ARM32 交叉编译
│   └── aarch64-linux-gnu.cmake     # ARM64 交叉编译
├── nlohmann/                 # JSON 库（header-only）
├── config/                   # 配置文件（运行时创建）
├── data/                     # 数据目录（运行时创建）
├── logs/                     # 日志文件（运行时创建）
├── CMakeLists.txt           # CMake 配置
├── README.md                # 项目说明
├── DEPLOYMENT.md            # 部署文档
└── BUILD.md                 # 编译教程
```

## 配置说明

### 系统配置 (config/system.json)

首次运行时自动创建，可通过 Web 界面或直接编辑文件修改：

```json
{
  "serverPort": 8080,                    // 服务器端口
  "maxUploadFiles": 1,                   // 每个任务最大上传文件数
  "maxTasks": 50,                        // 最大任务数
  "maxConcurrentTasks": 10,              // 最大并发任务数（总体）
  "maxConcurrentTasksPerModel": 1,       // 同一模型最大并发任务数
  "maxTranslationThreads": 1,            // 任务内翻译线程数
  "maxRetries": 3,                       // 翻译重试次数
  "consecutiveFailureThreshold": 5,      // 连续失败阈值（达到后自动暂停）
  "logLevel": "info",                    // 日志级别: debug/info/warning/error
  "sessionTimeoutMinutes": 30,           // 会话超时时间（分钟）
  "maxLoginAttempts": 3,                 // 最大登录尝试次数
  "lockoutDurationMinutes": 5            // 登录锁定时长（分钟）
}
```

### 模型配置 (config/models.json)

通过 Web 界面的"模型管理"页面配置，支持多个模型：

```json
[
  {
    "id": "gpt-3.5",
    "name": "GPT-3.5 Turbo",
    "url": "https://api.openai.com/v1/chat/completions",
    "apiKey": "sk-your-api-key",
    "modelId": "gpt-3.5-turbo",
    "temperature": 0.3,
    "systemPrompt": "你是一个专业的学术文献翻译助手，请将以下英文翻译为中文，保持学术性和准确性。只返回翻译结果，不要添加任何解释。"
  },
  {
    "id": "ollama-local",
    "name": "Ollama 本地模型",
    "url": "http://localhost:11434/v1/chat/completions",
    "apiKey": "ollama",
    "modelId": "qwen2.5:7b",
    "temperature": 0.3,
    "systemPrompt": "..."
  }
]
```

## 使用指南

### 1. 翻译文献

1. 从 Web of Science 导出 HTML 格式的文献记录
2. 访问"翻译"页面
3. （可选）输入任务名称
4. 上传 HTML 文件
5. 选择翻译选项（标题/摘要）
6. 选择 AI 模型
7. 点击"创建翻译任务"

### 2. 管理任务

- **队列页面**: 查看所有任务，支持列表/网格视图
- **任务详情**: 查看翻译进度，浏览原文和译文
- **操作**: 暂停/恢复/删除任务
- **导出**: 任务完成后可导出为多种格式

### 3. 模型管理

- 添加/编辑/删除 AI 模型配置
- 测试模型连接
- 支持 OpenAI API 及兼容接口

### 4. 系统设置

- 修改管理员密码
- 调整系统参数
- 查看存储使用情况
- 清理已删除任务

## 部署方式

### 方式一：直接运行

```bash
./scripts/start.sh
```

### 方式二：systemd 服务

```bash
sudo ./scripts/install.sh
sudo systemctl start wos-translator
sudo systemctl enable wos-translator
```

### 方式三：OpenWrt 部署

详见 [DEPLOYMENT.md](DEPLOYMENT.md) 和 [BUILD.md](BUILD.md)

## 故障排查

### 服务无法启动

```bash
# 检查端口占用
netstat -tlnp | grep 8080

# 查看日志
tail -f logs/app.log

# 检查权限
ls -la config/ data/ logs/
```

### 翻译失败

1. 检查网络连接
2. 验证 API 密钥是否正确
3. 检查 API 配额是否用尽
4. 查看日志了解详细错误

### 内存不足

1. 减少 `maxConcurrentTasks`
2. 减少 `maxTranslationThreads`
3. 清理旧任务数据

## 安全建议

1. **立即修改默认密码**
2. 使用 HTTPS（通过 Nginx 反向代理）
3. 配置防火墙限制访问
4. 定期备份 config 和 data 目录
5. 定期更新系统和依赖

## 许可证

MIT License

## 相关文档

- [编译教程](BUILD.md) - 详细的编译说明（x86/ARM）
- [部署文档](DEPLOYMENT.md) - 部署和配置指南
- [设计文档](.kiro/specs/wos-translation-system/design.md) - 系统设计说明

---

**注意**: 本项目仅供学术研究使用，请遵守相关 API 服务条款。
