# WoS Translator 部署文档

本文档详细介绍如何部署和配置 WoS Translator。

## 目录

1. [系统要求](#系统要求)
2. [部署方式](#部署方式)
3. [配置说明](#配置说明)
4. [反向代理配置](#反向代理配置)
5. [性能优化](#性能优化)
6. [监控和维护](#监控和维护)
7. [故障排查](#故障排查)
8. [卸载](#卸载)

---

## 系统要求

### 硬件要求

| 项目 | 最低配置 | 推荐配置 |
|------|----------|----------|
| CPU | 单核 ARM Cortex-A7 / x86_64 | 双核或更高 |
| 内存 | 128MB RAM | 256MB+ RAM |
| 存储 | 50MB（不含数据） | 1GB+（含数据） |
| 网络 | 可访问 AI API | 稳定网络连接 |

### 软件要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Linux (Ubuntu, Debian, CentOS, OpenWrt 等) |
| 运行时库 | libcurl, OpenSSL, pthread |

### 依赖库安装

**Ubuntu/Debian:**
```bash
sudo apt-get install -y libcurl4 libssl1.1
```

**CentOS/RHEL:**
```bash
sudo yum install -y libcurl openssl
```

**OpenWrt:**
```bash
opkg update
opkg install libcurl libssl
```

---

## 部署方式

### 方式一：直接运行（开发/测试）

最简单的部署方式，适合开发测试：

```bash
# 1. 编译（如果还没编译）
./scripts/build.sh

# 2. 复制可执行文件到项目根目录
cp build/wos-translator .

# 3. 启动服务
./scripts/start.sh

# 或直接运行
./wos-translator
```

服务启动后：
- 访问地址: `http://localhost:8080`
- 默认密码: `admin123`
- 日志文件: `logs/app.log`

### 方式二：systemd 服务（生产环境推荐）

适合 Ubuntu、Debian、CentOS 等使用 systemd 的系统：

```bash
# 1. 编译
./scripts/build.sh

# 2. 复制可执行文件
cp build/wos-translator .

# 3. 以 root 权限运行安装脚本
sudo ./scripts/install.sh

# 4. 启动服务
sudo systemctl start wos-translator

# 5. 设置开机自启
sudo systemctl enable wos-translator

# 6. 查看状态
sudo systemctl status wos-translator

# 7. 查看日志
sudo journalctl -u wos-translator -f
```

安装后的文件位置：
- 可执行文件: `/opt/wos-translator/wos-translator`
- Web 文件: `/opt/wos-translator/web/`
- 配置文件: `/opt/wos-translator/config/`
- 数据目录: `/opt/wos-translator/data/`
- 日志目录: `/opt/wos-translator/logs/`

#### systemd 服务文件

`/etc/systemd/system/wos-translator.service`:

```ini
[Unit]
Description=WoS Translator Service
After=network.target

[Service]
Type=simple
User=www-data
Group=www-data
WorkingDirectory=/opt/wos-translator
ExecStart=/opt/wos-translator/wos-translator
Restart=always
RestartSec=5

# 安全设置
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/wos-translator/config /opt/wos-translator/data /opt/wos-translator/logs

[Install]
WantedBy=multi-user.target
```

### 方式三：OpenWrt 部署

适合路由器等嵌入式设备：

#### 1. 交叉编译

在开发机上：
```bash
# ARM32 设备
./scripts/build.sh --arm

# ARM64 设备
./scripts/build.sh --arm64
```

#### 2. 复制文件到设备

```bash
# 复制可执行文件
scp build/wos-translator root@openwrt:/usr/bin/

# 复制 web 文件
scp -r web root@openwrt:/usr/share/wos-translator/
```

#### 3. 在 OpenWrt 上创建目录

```bash
ssh root@openwrt
mkdir -p /etc/wos-translator
mkdir -p /var/lib/wos-translator/data
mkdir -p /var/log/wos-translator
```

#### 4. 创建启动脚本

创建 `/etc/init.d/wos-translator`:

```bash
#!/bin/sh /etc/rc.common

START=99
STOP=10

USE_PROCD=1
PROG=/usr/bin/wos-translator
WORKDIR=/var/lib/wos-translator

start_service() {
    procd_open_instance
    procd_set_param command $PROG
    procd_set_param respawn
    procd_set_param stdout 1
    procd_set_param stderr 1
    procd_set_param env HOME=$WORKDIR
    procd_close_instance
}

stop_service() {
    killall wos-translator
}
```

#### 5. 启动服务

```bash
chmod +x /etc/init.d/wos-translator
/etc/init.d/wos-translator enable
/etc/init.d/wos-translator start
```

### 方式四：Docker 部署（可选）

如果需要 Docker 部署，可以创建 Dockerfile：

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY wos-translator /app/
COPY web /app/web/

RUN mkdir -p config data logs

EXPOSE 8080

CMD ["./wos-translator"]
```

构建和运行：
```bash
docker build -t wos-translator .
docker run -d -p 8080:8080 -v $(pwd)/data:/app/data wos-translator
```

---

## 配置说明

### 系统配置

配置文件: `config/system.json`

首次运行时自动创建，包含以下参数：

```json
{
  "serverPort": 8080,                    // 服务器端口
  "maxUploadFiles": 1,                   // 每个任务最大上传文件数
  "maxTasks": 50,                        // 最大任务数
  "maxConcurrentTasks": 10,              // 最大并发任务数（总体）
  "maxConcurrentTasksPerModel": 1,       // 同一模型最大并发任务数
  "maxTranslationThreads": 1,            // 任务内翻译线程数
  "maxRetries": 3,                       // 翻译重试次数
  "consecutiveFailureThreshold": 5,      // 连续失败阈值
  "logLevel": "info",                    // 日志级别
  "sessionTimeoutMinutes": 30,           // 会话超时时间
  "maxLoginAttempts": 3,                 // 最大登录尝试次数
  "lockoutDurationMinutes": 5            // 登录锁定时长
}
```

#### 参数说明

| 参数 | 说明 | 建议值 |
|------|------|--------|
| `serverPort` | HTTP 服务端口 | 8080 |
| `maxUploadFiles` | 单任务最大文件数 | 1-10 |
| `maxTasks` | 系统最大任务数 | 50-100 |
| `maxConcurrentTasks` | 总并发任务数 | 1-10（根据内存调整） |
| `maxConcurrentTasksPerModel` | 同模型并发数 | 1-3 |
| `maxTranslationThreads` | 任务内线程数 | 1-4 |
| `maxRetries` | API 重试次数 | 3 |
| `consecutiveFailureThreshold` | 连续失败暂停阈值 | 5 |
| `logLevel` | 日志级别 | info |
| `sessionTimeoutMinutes` | 会话超时 | 30 |
| `maxLoginAttempts` | 登录尝试次数 | 3 |
| `lockoutDurationMinutes` | 锁定时长 | 5 |

### 模型配置

配置文件: `config/models.json`

通过 Web 界面配置，或直接编辑文件：

```json
[
  {
    "id": "gpt-3.5",
    "name": "GPT-3.5 Turbo",
    "url": "https://api.openai.com/v1/chat/completions",
    "apiKey": "sk-your-api-key",
    "modelId": "gpt-3.5-turbo",
    "temperature": 0.3,
    "systemPrompt": "你是一个专业的学术文献翻译助手..."
  }
]
```

#### 支持的 API 类型

| 类型 | URL 格式 | 说明 |
|------|----------|------|
| OpenAI | `https://api.openai.com/v1/chat/completions` | 官方 API |
| Azure OpenAI | `https://xxx.openai.azure.com/...` | Azure 托管 |
| Ollama | `http://localhost:11434/v1/chat/completions` | 本地模型 |
| 其他兼容 API | 各服务商提供的 URL | 需兼容 OpenAI 格式 |

### 日志配置

日志文件: `logs/app.log`

日志级别（从低到高）：
- `debug`: 调试信息
- `info`: 一般信息
- `warning`: 警告信息
- `error`: 错误信息

日志轮转：
- 单文件最大: 10MB
- 保留备份数: 5

---

## 反向代理配置

### Nginx 配置

推荐使用 Nginx 作为反向代理，提供 HTTPS 支持：

```nginx
server {
    listen 80;
    server_name your-domain.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name your-domain.com;

    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;

    # 客户端最大上传大小
    client_max_body_size 50M;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # WebSocket 支持（如果需要）
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        
        # 超时设置
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }
}
```

### Apache 配置

```apache
<VirtualHost *:443>
    ServerName your-domain.com
    
    SSLEngine on
    SSLCertificateFile /path/to/cert.pem
    SSLCertificateKeyFile /path/to/key.pem
    
    ProxyPreserveHost On
    ProxyPass / http://127.0.0.1:8080/
    ProxyPassReverse / http://127.0.0.1:8080/
    
    <Proxy *>
        Order deny,allow
        Allow from all
    </Proxy>
</VirtualHost>
```

---

## 性能优化

### 内存优化

对于低内存设备（< 256MB）：

```json
{
  "maxConcurrentTasks": 1,
  "maxConcurrentTasksPerModel": 1,
  "maxTranslationThreads": 1,
  "maxTasks": 20
}
```

### 并发优化

对于高性能服务器：

```json
{
  "maxConcurrentTasks": 10,
  "maxConcurrentTasksPerModel": 3,
  "maxTranslationThreads": 4,
  "maxTasks": 100
}
```

### 网络优化

```json
{
  "maxRetries": 5,
  "consecutiveFailureThreshold": 10
}
```

---

## 监控和维护

### 日志监控

```bash
# 实时查看日志
tail -f logs/app.log

# 查看错误日志
grep -i error logs/app.log

# 查看最近的警告和错误
grep -E "(WARNING|ERROR)" logs/app.log | tail -50
```

### 磁盘空间

```bash
# 查看数据目录大小
du -sh data/

# 查看各任务大小
du -sh data/*/*
```

### 清理旧数据

通过 Web 界面的"系统设置"页面：
1. 查看存储使用情况
2. 清理已删除的任务

或手动清理：
```bash
# 删除 30 天前的任务
find data/ -type d -mtime +30 -exec rm -rf {} \;
```

### 备份

```bash
# 备份配置和数据
tar -czvf backup-$(date +%Y%m%d).tar.gz config/ data/

# 恢复
tar -xzvf backup-20240101.tar.gz
```

---

## 故障排查

### 服务无法启动

1. **检查端口占用**
   ```bash
   netstat -tlnp | grep 8080
   # 或
   ss -tlnp | grep 8080
   ```

2. **检查日志**
   ```bash
   tail -100 logs/app.log
   ```

3. **检查权限**
   ```bash
   ls -la config/ data/ logs/
   # 确保有读写权限
   ```

4. **检查依赖库**
   ```bash
   ldd ./wos-translator
   # 确保所有库都能找到
   ```

### 翻译失败

1. **检查网络连接**
   ```bash
   curl -I https://api.openai.com
   ```

2. **验证 API 密钥**
   - 在模型管理页面测试连接

3. **检查 API 配额**
   - 登录 API 提供商控制台查看

4. **查看详细错误**
   ```bash
   grep -i "translation failed" logs/app.log
   ```

### 内存不足

1. **减少并发**
   ```json
   {
     "maxConcurrentTasks": 1,
     "maxTranslationThreads": 1
   }
   ```

2. **清理数据**
   - 删除旧任务
   - 清理已删除任务

3. **监控内存**
   ```bash
   ps aux | grep wos-translator
   free -h
   ```

### 性能问题

1. **检查 CPU 使用**
   ```bash
   top -p $(pgrep wos-translator)
   ```

2. **检查磁盘 I/O**
   ```bash
   iostat -x 1
   ```

3. **优化配置**
   - 调整并发参数
   - 考虑使用 SSD

---

## 卸载

### 直接运行方式

```bash
# 停止服务
pkill wos-translator

# 删除文件
rm -rf wos-translator config data logs web
```

### systemd 服务方式

```bash
# 停止并禁用服务
sudo systemctl stop wos-translator
sudo systemctl disable wos-translator

# 删除服务文件
sudo rm /etc/systemd/system/wos-translator.service
sudo systemctl daemon-reload

# 删除安装目录
sudo rm -rf /opt/wos-translator
```

### OpenWrt

```bash
# 停止并禁用服务
/etc/init.d/wos-translator stop
/etc/init.d/wos-translator disable

# 删除文件
rm /etc/init.d/wos-translator
rm /usr/bin/wos-translator
rm -rf /usr/share/wos-translator
rm -rf /etc/wos-translator
rm -rf /var/lib/wos-translator
rm -rf /var/log/wos-translator
```

---

## 安全建议

1. **修改默认密码**: 首次登录后立即修改
2. **使用 HTTPS**: 通过 Nginx 等反向代理提供 HTTPS
3. **限制访问**: 配置防火墙，只允许信任的 IP 访问
4. **定期备份**: 备份 config 和 data 目录
5. **更新依赖**: 定期更新系统和依赖库
6. **监控日志**: 定期检查日志，发现异常及时处理

---

## 技术支持

如有问题，请查看：
- 日志文件: `logs/app.log`
- 项目文档: [README.md](README.md)
- 编译教程: [BUILD.md](BUILD.md)
- 设计文档: `.kiro/specs/wos-translation-system/design.md`
