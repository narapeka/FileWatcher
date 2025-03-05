#define _GNU_SOURCE
#include <sys/fanotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define BUF_SIZE 8192
#define DEFAULT_READ_THRESHOLD 50
#define DEFAULT_HTTP_SERVER "192.168.1.100"
#define DEFAULT_HTTP_PORT "8080"
#define DEFAULT_PATH_TO_MONITOR "/mnt/media"
#define BD_SUFFIX ".bluray"
#define BD_SUFFIX_LEN (sizeof(BD_SUFFIX) - 1)
#define MAX_BD_PATH_LEN (PATH_MAX - BD_SUFFIX_LEN)

// 状态结构体
typedef struct
{
    char path[PATH_MAX];
    int read_count;
    int request_sent;
} FileState;

// **读取 JSON 配置文件**
void load_config(const char *config_file, int *read_threshold, char *server, char *port, char paths_to_monitor[][PATH_MAX], int *path_count, char file_extensions[][16], int *ext_count)
{
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Config file %s not found, using default values.\n", config_file);
        *read_threshold = DEFAULT_READ_THRESHOLD;
        strcpy(server, DEFAULT_HTTP_SERVER);
        strcpy(port, DEFAULT_HTTP_PORT);
        strcpy(paths_to_monitor[0], DEFAULT_PATH_TO_MONITOR);
        *path_count = 1;
        *ext_count = 0;
        return;
    }

    // 读取 JSON 配置
    struct json_object *parsed_json;
    struct json_object *json_read_threshold, *json_http_server, *json_http_port, *json_path_to_monitor, *json_file_extensions;
    char buffer[1024];

    fread(buffer, sizeof(buffer), 1, fp);
    fclose(fp);

    parsed_json = json_tokener_parse(buffer);
    if (parsed_json == NULL)
    {
        fprintf(stderr, "Failed to parse config.json, using default values.\n");
        *read_threshold = DEFAULT_READ_THRESHOLD;
        strcpy(server, DEFAULT_HTTP_SERVER);
        strcpy(port, DEFAULT_HTTP_PORT);
        strcpy(paths_to_monitor[0], DEFAULT_PATH_TO_MONITOR);
        *path_count = 1;
        *ext_count = 0;
        return;
    }

    // 解析 JSON 值
    if (json_object_object_get_ex(parsed_json, "read_threshold", &json_read_threshold))
        *read_threshold = json_object_get_int(json_read_threshold);
    else
        *read_threshold = DEFAULT_READ_THRESHOLD;

    if (json_object_object_get_ex(parsed_json, "http_server", &json_http_server))
        strcpy(server, json_object_get_string(json_http_server));
    else
        strcpy(server, DEFAULT_HTTP_SERVER);

    if (json_object_object_get_ex(parsed_json, "http_port", &json_http_port))
        strcpy(port, json_object_get_string(json_http_port));
    else
        strcpy(port, DEFAULT_HTTP_PORT);

    if (json_object_object_get_ex(parsed_json, "path_to_monitor", &json_path_to_monitor))
    {
        *path_count = json_object_array_length(json_path_to_monitor);
        for (int i = 0; i < *path_count; i++)
        {
            struct json_object *path_obj = json_object_array_get_idx(json_path_to_monitor, i);
            strcpy(paths_to_monitor[i], json_object_get_string(path_obj));
        }
    }
    else
    {
        fprintf(stderr, "Missing path_to_monitor in config.json. Please specify the target directory.\n");
        exit(EXIT_FAILURE);
    }

    if (json_object_object_get_ex(parsed_json, "file_extensions", &json_file_extensions))
    {
        *ext_count = json_object_array_length(json_file_extensions);
        for (int i = 0; i < *ext_count; i++)
        {
            struct json_object *ext_obj = json_object_array_get_idx(json_file_extensions, i);
            strcpy(file_extensions[i], json_object_get_string(ext_obj));
        }
    }
    else
    {
        *ext_count = 0; // 如果没有指定扩展名，表示监控所有文件
    }

    json_object_put(parsed_json); // 释放 JSON 解析对象
}

// **发送 JSON 格式的 HTTP 请求**
void send_http_request(const char *file_path, const char *server, const char *port)
{
    CURL *curl;
    CURLcode res;

    // 构建 JSON 请求体
    struct json_object *json_obj = json_object_new_object();
    json_object_object_add(json_obj, "file_path", json_object_new_string(file_path));

    // JSON 转换为字符串
    const char *json_str = json_object_to_json_string(json_obj);

    // 生成 HTTP URL
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%s/play", server, port);

    // 初始化 CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl)
    {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);

        fprintf(stderr, "[INFO] Sending HTTP request to %s with file path: %s\n", url, file_path);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
            fprintf(stderr, "[ERROR] Failed to send request to %s: %s\n", url, curl_easy_strerror(res));
        }
        else
        {
            fprintf(stderr, "[INFO] Successfully sent request to %s for file: %s\n", url, file_path);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    else
    {
        fprintf(stderr, "[ERROR] Failed to initialize curl.\n");
    }

    curl_global_cleanup();
    json_object_put(json_obj); // 释放 JSON 对象
}

// 检查文件扩展名是否在允许的扩展名列表中
int has_valid_extension(const char *file_path, char file_extensions[][16], int ext_count)
{
    if (ext_count == 0)
    {
        return 1; // 如果没有指定扩展名，则允许所有文件
    }

    for (int i = 0; i < ext_count; i++)
    {
        if (strstr(file_path, file_extensions[i]) != NULL)
        {
            return 1; // 找到匹配的扩展名
        }
    }
    return 0; // 没有匹配的扩展名
}

// 获取 BD 根路径
int get_bd_root_path(const char *file_path, char *bd_root_path, size_t bd_root_size)
{
    char *bdmv_pos = strstr(file_path, "BDMV");
    if (bdmv_pos == NULL)
        return 0;

    size_t root_len = bdmv_pos - file_path;
    if (root_len >= MAX_BD_PATH_LEN || root_len >= bd_root_size)
    {
        fprintf(stderr, "[ERROR] BD root path too long: %s\n", file_path);
        return 0;
    }

    strncpy(bd_root_path, file_path, root_len);
    bd_root_path[root_len] = '\0';
    if (bd_root_path[root_len - 1] != '/')
        strcat(bd_root_path, "/");
    return 1;
}

// 处理事件
void handle_events(int fanotify_fd, int read_threshold, const char *server, const char *port, char file_extensions[][16], int ext_count)
{
    char buf[BUF_SIZE];
    ssize_t len;
    struct fanotify_event_metadata *metadata;
    FileState bd_state = {{0}, 0, 0};   // BD rip 状态
    FileState file_state = {{0}, 0, 0}; // 单文件状态

    while (1)
    {
        len = read(fanotify_fd, buf, sizeof(buf));
        if (len == -1)
        {
            if (errno == EAGAIN)
                continue;
            perror("read");
            break;
        }
        if (len <= 0)
            continue;

        metadata = (struct fanotify_event_metadata *)buf;
        while (FAN_EVENT_OK(metadata, len))
        {
            if (metadata->fd >= 0)
            {

                char path[PATH_MAX];
                char resolved_path[PATH_MAX] = {0};

                snprintf(path, sizeof(path), "/proc/self/fd/%d", metadata->fd);

                if (readlink(path, resolved_path, sizeof(resolved_path) - 1) != -1)
                {
                    resolved_path[sizeof(resolved_path) - 1] = '\0';

                    // 只处理符合扩展名的文件
                    if (has_valid_extension(resolved_path, file_extensions, ext_count))
                    {
                        // 检查是否为 BD rip
                        char temp_bd_path[PATH_MAX] = {0};
                        int is_bd_rip = get_bd_root_path(resolved_path, temp_bd_path, sizeof(temp_bd_path));

                        if (is_bd_rip)
                        {
                            // BD rip 处理
                            if (strcmp(bd_state.path, temp_bd_path) != 0)
                            {
                                // 新 BD rip，重置状态
                                strcpy(bd_state.path, temp_bd_path);
                                bd_state.read_count = 0;
                                bd_state.request_sent = 0;
                            }

                            if (!bd_state.request_sent)
                            {
                                bd_state.read_count++;
                                if (bd_state.read_count >= read_threshold)
                                {
                                    char request_path[PATH_MAX + BD_SUFFIX_LEN + 1];
                                    snprintf(request_path, sizeof(request_path), "%s%s", bd_state.path, BD_SUFFIX);
                                    fprintf(stderr, "[READ] BD rip accessed: %s\n", resolved_path);
                                    send_http_request(request_path, server, port);
                                    bd_state.request_sent = 1;
                                }
                            }
                        }
                        else
                        {
                            // 单文件处理
                            if (strcmp(file_state.path, resolved_path) != 0)
                            {
                                // 新文件，重置状态
                                strcpy(file_state.path, resolved_path);
                                file_state.read_count = 0;
                                file_state.request_sent = 0;
                            }

                            if (!file_state.request_sent)
                            {
                                file_state.read_count++;
                                if (file_state.read_count >= read_threshold)
                                {
                                    fprintf(stderr, "[READ] File accessed: %s\n", resolved_path);
                                    send_http_request(resolved_path, server, port);
                                    file_state.request_sent = 1;
                                }
                            }
                        }
                    }
                }
                else
                {
                    perror("readlink");
                    fprintf(stderr, "Failed to resolve path for fd: %d\n", metadata->fd);
                }
                close(metadata->fd);
            }
            metadata = FAN_EVENT_NEXT(metadata, len);
        }
    }
}

// 主程序
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *config_file = argv[1];

    int fanotify_fd;
    int read_threshold;
    char server[128];
    char port[16];
    char paths_to_monitor[10][PATH_MAX]; // 假设最多监控 10 个路径
    char file_extensions[10][16];        // 假设最多监控 10 个扩展名
    int path_count, ext_count;

    // 读取配置文件
    load_config(config_file, &read_threshold, server, port, paths_to_monitor, &path_count, file_extensions, &ext_count);

    fprintf(stderr, "Monitoring directories:\n");
    for (int i = 0; i < path_count; i++)
    {
        fprintf(stderr, " - %s\n", paths_to_monitor[i]);
    }
    fprintf(stderr, "Read threshold: %d\n", read_threshold);


    // 初始化 fanotify
    fanotify_fd = fanotify_init(FAN_CLASS_NOTIF, O_RDONLY | O_CLOEXEC);
    if (fanotify_fd == -1)
    {
        perror("fanotify_init");
        exit(EXIT_FAILURE);
    }

    // 监控多个目录
    for (int i = 0; i < path_count; i++)
    {
        if (fanotify_mark(fanotify_fd, FAN_MARK_ADD | FAN_MARK_MOUNT, FAN_ACCESS, AT_FDCWD, paths_to_monitor[i]) == -1)
        {
            perror("fanotify_mark");
            close(fanotify_fd);
            exit(EXIT_FAILURE);
        }
    }

    // 处理文件事件
    handle_events(fanotify_fd, read_threshold, server, port, file_extensions, ext_count);

    close(fanotify_fd);
    return 0;
}