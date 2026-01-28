#!/bin/bash
# WoS Translator 安装脚本

set -e

# 检查是否以root权限运行
if [ "$EUID" -ne 0 ]; then 
    echo "请使用root权限运行此脚本"
    exit 1
fi

# 配置
INSTALL_DIR="/opt/wos-translator"
SERVICE_FILE="/etc/systemd/system/wos-translator.service"

echo "=== WoS Translator 安装脚本 ==="
echo ""

# 检查可执行文件
if [ ! -f "./wos-translator" ]; then
    echo "错误: 找不到可执行文件 wos-translator"
    echo "请先编译项目"
    exit 1
fi

# 创建安装目录
echo "创建安装目录: $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR/config"
mkdir -p "$INSTALL_DIR/data"
mkdir -p "$INSTALL_DIR/logs"
mkdir -p "$INSTALL_DIR/web"

# 复制文件
echo "复制文件..."
cp wos-translator "$INSTALL_DIR/"
cp -r web/* "$INSTALL_DIR/web/"

# 设置权限
echo "设置权限..."
chown -R www-data:www-data "$INSTALL_DIR"
chmod 755 "$INSTALL_DIR/wos-translator"
chmod 700 "$INSTALL_DIR/config"
chmod 755 "$INSTALL_DIR/data"
chmod 755 "$INSTALL_DIR/logs"
chmod 755 "$INSTALL_DIR/web"

# 安装systemd服务（可选）
if [ -f "scripts/wos-translator.service" ]; then
    echo "安装systemd服务..."
    cp scripts/wos-translator.service "$SERVICE_FILE"
    systemctl daemon-reload
    echo ""
    echo "systemd服务已安装。使用以下命令管理服务:"
    echo "  启动服务: systemctl start wos-translator"
    echo "  停止服务: systemctl stop wos-translator"
    echo "  开机自启: systemctl enable wos-translator"
    echo "  查看状态: systemctl status wos-translator"
fi

echo ""
echo "=== 安装完成 ==="
echo "安装目录: $INSTALL_DIR"
echo ""
echo "首次运行时，默认管理员密码为: admin123"
echo "请在首次登录后立即修改密码！"
echo ""
echo "访问地址: http://localhost:8080"
