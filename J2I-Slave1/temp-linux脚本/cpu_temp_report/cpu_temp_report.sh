#!/bin/bash

# 飞腾 D2000 CPU 温度串口上报服务。

export LC_ALL=C

SERIAL_DEVICE="${SERIAL_DEVICE:-/dev/ttyAMA2}"

if ! command -v sensors >/dev/null 2>&1; then
    echo "错误：未安装 sensors 命令，请先安装 lm-sensors。" >&2
    exit 1
fi

if [ ! -c "$SERIAL_DEVICE" ]; then
    echo "错误：串口设备不存在：$SERIAL_DEVICE" >&2
    exit 1
fi

stty -F "$SERIAL_DEVICE" 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw -echo || exit 1

while true; do
    # 读取 sensors 输出中的第一个温度值，并保留一位小数。
    temperature="$(sensors -u 2>/dev/null | awk '/^[[:space:]]*temp[0-9]+_input:/ {printf "%.1f", $2; exit}')"

    if [ -n "$temperature" ]; then
        printf 'CPU_TEMP=%sC\n' "$temperature" > "$SERIAL_DEVICE"
        printf 'CPU_TEMP=%sC\n' "$temperature"
    else
        echo "警告：sensors 未返回温度。" >&2
    fi

    sleep 1
done
