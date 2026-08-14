#!/bin/bash

# 安装飞腾 CPU 温度串口上报服务。

set -eu

INSTALL_ROOT="/usr/local/cpu_temp_report"
SERVICE_NAME="cpu_temp_report.service"
SERVICE_ROOT="/etc/systemd/system"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    echo "错误：请使用 root 权限执行安装脚本。" >&2
    exit 1
fi

if ! command -v sensors >/dev/null 2>&1; then
    echo "错误：未安装 sensors 命令，请先安装 lm-sensors。" >&2
    exit 1
fi

if [ ! -f "$SCRIPT_DIR/cpu_temp_report.sh" ] || [ ! -f "$SCRIPT_DIR/$SERVICE_NAME" ]; then
    echo "错误：安装包缺少 cpu_temp_report.sh 或 $SERVICE_NAME。" >&2
    exit 1
fi

install -d -m 755 "$INSTALL_ROOT"
install -m 755 "$SCRIPT_DIR/cpu_temp_report.sh" "$INSTALL_ROOT/cpu_temp_report.sh"
install -m 644 "$SCRIPT_DIR/$SERVICE_NAME" "$SERVICE_ROOT/$SERVICE_NAME"

systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME"

echo "CPU 温度上报服务安装完成。"
echo "服务名称：$SERVICE_NAME"
echo "串口设备：/dev/ttyAMA2（115200 8N1）"
echo "查看状态：systemctl status $SERVICE_NAME"
