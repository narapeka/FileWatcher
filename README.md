# FileWatcher

FileWatcher 是一个用于监控文件访问事件并发送 HTTP 通知的工具。它通过 `fanotify` 监控指定目录中的文件访问事件，并在文件被连续访问达到指定次数时，向配置的 HTTP 服务器发送通知。

## 用途

FileWatcher 主要用于以下场景：
- **文件访问监控**：监控指定目录中的文件访问事件，记录文件的访问次数。
- **自动化通知**：当文件被连续访问达到指定次数时，自动向配置的 HTTP 服务器发送通知。
- **扩展名过滤**：支持根据文件扩展名过滤监控的文件类型。

## 适用环境
- 理论上兼容各大Linux发行版本，推荐Ubuntu/或者基于Debian的NAS系统比如群晖，绿联，飞牛
- 项目默认提供的二进制文件基于Linux/amd64编译。如需运行在其他架构，请自行编译。

## 安装与使用

### 1. 安装
- [点此下载](https://github.com/narapeka/FileWatcher/releases)本项目所有文件，解压
- 修改配置文件内容
- 执行install.sh

```bash
sudo -i

# 解压缩
tar -xvf filewatcher-<version>.tar.gz
cd filewatcher-<version>

# 安装python依赖
pip install flask
# 如果报错，则执行
apt install python3-flask

# 用文本编辑器修改好配置文件config.json的内容

# 修改完后，执行安装脚本
./install.sh

```

### 2. 配置说明

FileWatcher 的配置文件为 `config.json`，安装后位于 `/etc/filewatcher/` 目录下。配置文件包含以下字段：

- `path_to_monitor`：需要监控的目录路径列表。可以指定多个目录。
- `file_extensions`：需要监控的文件扩展名列表。如果未指定，则监控所有文件。
- `read_threshold`：文件被连续访问的次数阈值，达到该次数后发送 HTTP 通知。
- `http_server`：HTTP 服务器的地址。
- `http_port`：HTTP 服务器的端口。

示例配置文件：

```json
{
    "path_to_monitor": [
        "/volume1/cloud/115",
        "/volume2/media"
    ],
    "file_extensions": [
        ".iso",
        ".m2ts"
    ],
    "read_threshold": 40,
    "http_server": "192.168.1.50",
    "http_port": "7507"
}
```
### 3. 使用说明
- 安装完成后，服务即启动并监控目录的访问事件。
- 在刮削或者其他场景中，为避免误拉起蓝光机，可暂时停止监控服务。
- **使用浏览器访问 http://服务器IP:7503 即可快捷开启/停止服务。**
- 遇到问题可首先查看日志，参见install脚本给出的提示：

```bash
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
```

### 4. 注意事项
- **访问次数阈值**：fanotify模块在监测文件访问行为时，会有大量干扰，比如查看文件属性，预览文件等也会触发多次fanotify事件。阈值的作用是仅当监测事件达到一定量级（默认40）的时候才视为真正读取。在不同系统和应用场景中，判定文件被真正读取的阈值可能不一样，请自行结合日志输出的信息进行微调。

## 面向开发者

### 编译安装

安装依赖
```bash
sudo apt-get update
sudo apt-get install -y build-essential libcurl4-openssl-dev libjson-c-dev
```
编译
```bash
git clone https://github.com/narapeka/FileWatcher.git
cd FileWatcher
gcc -o filewatcher filewatcher.c -lcurl -ljson-c
```
手动运行
```bash
sudo ./filewatcher config.json
```

### 如何接收FileWatcher发出的通知

端点必须定义为 POST [http://server:port/play](http://server:port/play)，程序发送的request body如下：
```json
{
  "file_path": "path/to/file"
}
```

可参考[BlurayPoster项目](https://github.com/narapeka/BlurayPoster)
，该项目联动FileWatcher实现自动调用蓝光机播放媒体文件。
