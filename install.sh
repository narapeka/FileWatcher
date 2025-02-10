#!/bin/bash

# 检查是否为 root 用户
if [[ $EUID -ne 0 ]]; then
    echo "该脚本必须以 root 权限运行，请使用 sudo 或以 root 用户身份运行"
    exit 1
fi

# 提示用户
echo "安装脚本正在执行..."
echo "确保您已将编译好的二进制文件 (filewatcher) 放在当前目录中，并准备好配置文件。"

# 定义依赖包
DEPS=("libcurl4" "libjson-c-dev")

# 检测包管理器并安装依赖
echo "检测系统包管理器并安装依赖包..."

if command -v apt-get &> /dev/null; then
    echo "检测到 apt-get，正在安装依赖..."
    sudo apt-get update
    sudo apt-get install -y ${DEPS[@]}
elif command -v yum &> /dev/null; then
    echo "检测到 yum，正在安装依赖..."
    sudo yum install -y ${DEPS[@]}
elif command -v dnf &> /dev/null; then
    echo "检测到 dnf，正在安装依赖..."
    sudo dnf install -y ${DEPS[@]}
elif command -v zypper &> /dev/null; then
    echo "检测到 zypper，正在安装依赖..."
    sudo zypper install -y ${DEPS[@]}
elif command -v pacman &> /dev/null; then
    echo "检测到 pacman，正在安装依赖..."
    sudo pacman -S --noconfirm ${DEPS[@]}
else
    echo "未能识别您的包管理器，请手动安装以下依赖：${DEPS[@]}"
    exit 1
fi

# 确保 filewatcher 可执行
if [ ! -f "filewatcher" ]; then
    echo "错误：未能找到 filewatcher 可执行文件，请确保该文件存在。"
    exit 1
fi

# 将 filewatcher 移动到 /usr/local/bin 目录下
echo "复制程序文件到 /usr/local/bin ..."
sudo cp filewatcher /usr/local/bin/
sudo chmod +x /usr/local/bin/filewatcher

# 检查 config.json 文件是否存在
if [ ! -f "config.json" ]; then
    echo "错误：未能找到 config.json 配置文件，请确保该文件存在。"
    exit 1
fi

# 将 config.json 复制到 /etc/filewatcher/ 目录下
echo "复制配置文件到 /etc/filewatcher/ ..."
sudo mkdir -p /etc/filewatcher
sudo cp config.json /etc/filewatcher/

echo "创建日志文件/var/log/filewatcher.log..."
sudo touch /var/log/filewatcher.log

# 创建 systemd 服务文件
SERVICE_FILE="/etc/systemd/system/filewatcher.service"

echo "正在创建 systemd 服务：$SERVICE_FILE..."

sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=FileWatcher 服务
After=network.target

[Service]
ExecStart=/usr/local/bin/filewatcher /etc/filewatcher/config.json
WorkingDirectory=/etc/filewatcher
Restart=always
User=root
Group=root
StandardOutput=append:/var/log/filewatcher.log
StandardError=append:/var/log/filewatcher.log
SyslogIdentifier=filewatcher

[Install]
WantedBy=multi-user.target
EOF

# 重载 systemd 配置
echo "正在重载 systemd 配置..."
sudo systemctl daemon-reload

# 启动并启用服务
echo "正在启用 filewatcher 服务..."
sudo systemctl start filewatcher
sudo systemctl enable filewatcher

# 提示用户服务状态检查命令
echo "FileWatcher 服务已成功安装并启动！"
# 完成提示
echo "可以使用以下命令来管理服务："
echo "  启动服务：sudo systemctl start filewatcher"
echo "  查看服务状态：sudo systemctl status filewatcher"
echo "  停止服务：sudo systemctl stop filewatcher"
echo "  禁用开机启动：sudo systemctl disable filewatcher"
echo "若遇到问题："
echo "  查看实时日志：sudo journalctl -u filewatcher -f"
echo "  查看所有日志：cat /var/log/filewatcher.log"

