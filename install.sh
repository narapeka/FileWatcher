#!/bin/bash

# 检查是否为 root 用户
if [[ $EUID -ne 0 ]]; then
    echo "该脚本必须以 root 权限运行，请使用 sudo 或以 root 用户身份运行"
    exit 1
fi

# 提示用户
echo "安装脚本正在执行..."

# 用户确认
read -p "确认已经修改好 config.json 配置文件 (y/n)? " -n 1 -r
echo    # 这个空行用于换行

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
fi

# 清理旧版本
echo "清理旧版本程序..."
echo "清理系统服务..."
# Check if the filewatcher service exists before stopping and disabling
if systemctl is-active --quiet filewatcher; then
    sudo systemctl stop filewatcher
    sudo systemctl disable filewatcher
    sudo rm /etc/systemd/system/filewatcher.service
fi

# Check if the fwcontrol service exists before stopping and disabling
if systemctl is-active --quiet fwcontrol; then
    sudo systemctl stop fwcontrol
    sudo systemctl disable fwcontrol
    sudo rm /etc/systemd/system/fwcontrol.service
fi

echo "清理程序文件..."
# Only remove filewatcher and fwcontrol files if they exist
[ -f /usr/local/bin/filewatcher ] && sudo rm /usr/local/bin/filewatcher
[ -f /usr/local/bin/fwcontrol.py ] && sudo rm /usr/local/bin/fwcontrol.py
[ -d /etc/filewatcher ] && sudo rm -rf /etc/filewatcher

echo "清理日志文件..."
# Only remove log files if they exist
[ -f /var/log/filewatcher.log ] && sudo rm /var/log/filewatcher.log
[ -f /var/log/fwcontrol.log ] && sudo rm /var/log/fwcontrol.log

# 定义依赖包
DEPS=("libcurl4" "libjson-c-dev" "logrotate")

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

# 确保 fwcontrol.py 存在
if [ ! -f "fwcontrol.py" ]; then
    echo "错误：未能找到 fwcontrol.py 文件，请确保该文件存在。"
    exit 1
fi

# 将 filewatcher 移动到 /usr/local/bin 目录下
echo "复制程序文件到 /usr/local/bin ..."
sudo cp filewatcher /usr/local/bin/
sudo chmod +x /usr/local/bin/filewatcher
sudo cp fwcontrol.py /usr/local/bin/
sudo chmod +x /usr/local/bin/fwcontrol.py

# 检查 config.json 文件是否存在
if [ ! -f "config.json" ]; then
    echo "错误：未能找到 config.json 配置文件，请确保该文件存在。"
    exit 1
fi

# 将 config.json 复制到 /etc/filewatcher/ 目录下
echo "复制配置文件到 /etc/filewatcher/ ..."
sudo mkdir -p /etc/filewatcher
sudo cp config.json /etc/filewatcher/

echo "创建日志文件/var/log/..."
sudo touch /var/log/filewatcher.log
sudo touch /var/log/fwcontrol.log
sudo chmod 644 /var/log/filewatcher.log
sudo chmod 644 /var/log/fwcontrol.log

# 创建 logrotate 配置文件
LOGROTATE_FILE="/etc/logrotate.d/filewatcher"
echo "正在创建 logrotate 配置文件：$LOGROTATE_FILE..."
sudo bash -c "cat > $LOGROTATE_FILE" <<EOF
/var/log/filewatcher.log /var/log/fwcontrol.log {
    daily
    rotate 3
    compress
    missingok
    notifempty
    size 1M
    create 644 root root
}
EOF
sudo chmod 644 $LOGROTATE_FILE

# 创建 filewatcher systemd 服务文件
SERVICE_FILE="/etc/systemd/system/filewatcher.service"

echo "正在创建 filewatcher systemd 服务：$SERVICE_FILE..."

sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=FileWatcher 服务
After=network.target

[Service]
ExecStart=/usr/local/bin/filewatcher /etc/filewatcher/config.json
WorkingDirectory=/etc/filewatcher
Restart=always
StartLimitIntervalSec=0
StartLimitBurst=1
User=root
Group=root
StandardOutput=append:/var/log/filewatcher.log
StandardError=append:/var/log/filewatcher.log

[Install]
WantedBy=multi-user.target
EOF

# 创建 fwcontrol systemd 服务文件
FWCONTROL_SERVICE_FILE="/etc/systemd/system/fwcontrol.service"

echo "正在创建 fwcontrol systemd 服务：$FWCONTROL_SERVICE_FILE..."

sudo bash -c "cat > $FWCONTROL_SERVICE_FILE" <<EOF
[Unit]
Description=FWControl 服务
After=network.target

[Service]
ExecStart=/usr/bin/python3 /usr/local/bin/fwcontrol.py
WorkingDirectory=/usr/local/bin
Restart=always
StartLimitIntervalSec=0
StartLimitBurst=1
User=root
Group=root
StandardOutput=append:/var/log/fwcontrol.log
StandardError=append:/var/log/fwcontrol.log

[Install]
WantedBy=multi-user.target
EOF

# 重载 systemd 配置
echo "正在重载 systemd 配置..."
sudo systemctl daemon-reload

# 启动并启用服务
echo "正在启用 filewatcher 和 fwcontrol 服务..."
sudo systemctl start filewatcher
sudo systemctl enable filewatcher
sudo systemctl start fwcontrol
sudo systemctl enable fwcontrol

# 提示用户服务状态检查命令
echo "所有服务已成功安装并启动！"
# 完成提示
echo "可以使用以下命令来管理服务："
echo "  启动 filewatcher 服务：sudo systemctl start filewatcher"
echo "  查看 filewatcher 服务状态：sudo systemctl status filewatcher"
echo "  停止 filewatcher 服务：sudo systemctl stop filewatcher"
echo "  禁用开机启动：sudo systemctl disable filewatcher"
echo "也可以使用简易web界面来管理服务："
echo "  手机浏览器打开 http://<ip>:7503 "
echo "  建议保存为桌面快捷方式，方遍调用。"
echo "若遇到问题："
echo "  查看日志：cat /var/log/filewatcher.log"
