# FileWatcher

FileWatcher 是一个用于监控文件访问事件并发送 HTTP 通知的工具。它通过 `fanotify` 监控指定目录中的文件访问事件，并在文件被连续访问达到指定次数时，向配置的 HTTP 服务器发送通知。

## 用途

FileWatcher 主要用于以下场景：
- **文件访问监控**：监控指定目录中的文件访问事件，记录文件的访问次数。
- **自动化通知**：当文件被连续访问达到指定次数时，自动向配置的 HTTP 服务器发送通知。
- **扩展名过滤**：支持根据文件扩展名过滤监控的文件类型。

## 适用环境
- 理论上兼容各大Linux发行版本
- 项目默认提供的二进制文件基于Linux/amd64编译。如需运行在其他架构，请自行编译。

## 配置说明

FileWatcher 的配置文件为 `config.json`，位于 `/etc/filewatcher/` 目录下。配置文件包含以下字段：

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
    "read_threshold": 50,
    "http_server": "192.168.1.50",
    "http_port": "7507"
}
```

## 自动安装

- 下载本项目所有文件，解压
- 修改配置文件内容
- 执行install.sh

```bash
sudo ./install.sh
```

## 编译安装

安装依赖
```bash
sudo apt-get update
sudo apt-get install -y libcurl4 libjson-c-dev
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

## 注意事项
- **访问次数阈值**：fanotify模块在监测文件访问行为时，会有大量干扰，比如查看文件属性也视为多次读取。阈值的作用是仅当监测事件达到一定量级（默认50）的时候才视为真正读取。在不同系统和应用场景中，判定文件被真正读取的阈值可能不一样，请自行调试。
- **权限要求**：FileWatcher 需要以 root 权限运行，因为它使用 fanotify 来监控文件访问事件。
- **配置文件**：确保 config.json 文件中的路径和扩展名配置正确，否则可能导致监控失败。

## HTTP通知

端点必须定义为 POST http://server:port/play
，程序发送的request body如下：
```json
{
  "file_path": "path/to/file"
}
```

可参考BlurayPoster项目https://github.com/narapeka/BlurayPoster
，该项目联动FileWatcher可实现自动调用蓝光机播放媒体文件。
