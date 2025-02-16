import subprocess
from flask import Flask, render_template_string, request, jsonify

app = Flask(__name__)

def toggle_system_service(service_name, action):
    """启用或禁用服务"""
    try:
        # 获取服务的当前状态
        result = subprocess.run(
            ['systemctl', 'is-active', '--quiet', service_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        if action == 'start':
            if result.returncode == 0:
                # 如果服务已经在运行，返回当前状态和消息
                return {"message": f"{service_name} 服务已经启动", "status": "running", "icon": "fa-check-circle", "icon_color": "icon-green"}
            else:
                # 启动服务
                start_result = subprocess.run(['sudo', 'systemctl', 'start', service_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if start_result.returncode == 0:
                    return {"message": f"{service_name} 服务启动成功", "status": "running", "icon": "fa-check-circle", "icon_color": "icon-green"}
                else:
                    return {"message": f"启动 {service_name} 服务失败", "status": "inactive", "icon": "fa-times-circle", "icon_color": "icon-red"}

        elif action == 'stop':
            if result.returncode != 0:
                # 服务已经停止
                return {"message": f"{service_name} 服务已经停止", "status": "inactive", "icon": "fa-times-circle", "icon_color": "icon-red"}
            else:
                # 停止服务
                stop_result = subprocess.run(['sudo', 'systemctl', 'stop', service_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if stop_result.returncode == 0:
                    return {"message": f"{service_name} 服务已关闭", "status": "inactive", "icon": "fa-times-circle", "icon_color": "icon-red"}
                else:
                    return {"message": f"停止 {service_name} 服务失败", "status": "running", "icon": "fa-check-circle", "icon_color": "icon-green"}
    except Exception as e:
        return {"message": f"操作失败: {e}", "status": "inactive", "icon": "fa-times-circle", "icon_color": "icon-red"}

@app.route('/')
def index():
    """主页，提供按钮来控制服务"""
    html_template = """
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>系统服务控制</title>
        <!-- 引入Font Awesome CSS -->
        <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css" rel="stylesheet" />
        <link href="https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/css/materialize.min.css" rel="stylesheet" />
        <style>
            body {
                height: 100vh;
                display: flex;
                justify-content: center;
                align-items: center;
                background-color: #f0f4f7;
            }
            .container {
                width: 100%;
                max-width: 400px;
                padding: 20px;
                background-color: #ffffff;
                border-radius: 10px;
                box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            }
            .btn-large {
                width: 48%;
            }
            .button-group {
                display: flex;
                justify-content: space-between;
            }
            .response-message {
                margin-top: 20px;
                text-align: center;
            }
            .icon {
                margin-right: 8px;
            }
            .icon-green {
                color: green;
            }
            .icon-red {
                color: red;
            }
        </style>
    </head>
    <body>

        <div class="container">
            <form id="serviceForm" method="POST" action="/toggle_service">
                <div class="input-field">
                    <label for="serviceName">服务名称</label>
                    <input type="text" id="serviceName" name="serviceName" value="filewatcher" required />
                </div>

                <div class="button-group">
                    <button type="button" id="startButton" class="btn-large waves-effect waves-light blue" onclick="toggleService('start')">启动</button>
                    <button type="button" id="stopButton" class="btn-large waves-effect waves-light red" onclick="toggleService('stop')">停止</button>
                </div>

                <div id="responseMessage" class="response-message"></div>
            </form>
        </div>

        <script src="https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/js/materialize.min.js"></script>
        <script>
            const serviceForm = document.getElementById('serviceForm');
            const responseMessage = document.getElementById('responseMessage');
            const startButton = document.getElementById('startButton');
            const stopButton = document.getElementById('stopButton');

            function toggleService(action) {
                const serviceName = document.getElementById('serviceName').value;

                fetch('/toggle_service', {
                    method: 'POST',
                    body: new URLSearchParams({
                        'serviceName': serviceName,
                        'action': action
                    })
                })
                .then(response => response.json())
                .then(data => {
                    responseMessage.innerHTML = `
                        <i class="fas ${data.icon} ${data.icon_color}"></i>
                        <span class="flow-text">${data.message}</span>
                    `;

                    if (data.status === 'running') {
                        startButton.disabled = true;
                        stopButton.disabled = false;
                    } else if (data.status === 'inactive') {
                        startButton.disabled = false;
                        stopButton.disabled = true;
                    }
                })
                .catch(error => {
                    responseMessage.innerHTML = `<span class="flow-text" style="color: red;">发生错误: ${error}</span>`;
                });
            }
        </script>
    </body>
    </html>
    """
    return render_template_string(html_template)


@app.route('/toggle_service', methods=['POST'])
def toggle_service():
    """处理按钮点击事件，启动或停止服务"""
    service_name = request.form.get('serviceName', 'filewatcher')
    action = request.form.get('action')
    
    response = toggle_system_service(service_name, action)
    
    return jsonify(response)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=7503)
