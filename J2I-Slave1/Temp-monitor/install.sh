 #!/bin/bash

echo "🚀 开始一键安装温度监控服务..."

# 1. 将 temp_monitor.sh 复制到 /usr/local/bin/ 并赋予 755 权限
sudo cp temp_monitor.sh   /usr/local/bin/
sudo chmod 755 /usr/local/bin/temp_monitor.sh
echo "✅ 主脚本已部署"

# 2. 将 temp-monitor.service 复制到 /etc/systemd/system/ 并赋予 644 权限
sudo cp temp-monitor.service /etc/systemd/system/
sudo chmod 644 /etc/systemd/system/temp-monitor.service
echo "✅ 服务文件已部署"

# 3. 重载 systemd 配置，并设置开机自启、立即启动服务
sudo systemctl daemon-reload
sudo systemctl enable temp-monitor.service
sudo systemctl restart temp-monitor.service

echo "✨ 安装完成！"
# 4. 打印当前服务状态供你确认
sudo systemctl status temp-monitor.service --no-pager