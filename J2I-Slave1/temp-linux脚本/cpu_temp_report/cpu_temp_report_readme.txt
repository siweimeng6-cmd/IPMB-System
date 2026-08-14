飞腾 D2000 CPU 温度串口上报安装包

功能：
1. 使用 sensors -u 命令读取第一个 CPU 温度值。
2. 每秒将 CPU 温度发送到单片机串口 /dev/ttyAMA2。
3. 串口参数为 115200、8 数据位、无校验、1 停止位、无流控。
4. ASCII 报文格式为 CPU_TEMP=45.2C，帧尾为 LF（0x0A）。

安装：
    请先确认 sensors 命令可以正常显示 CPU 温度。
    sudo bash install_cpu_temp_report.sh

检查：
    systemctl status cpu_temp_report.service
    journalctl -u cpu_temp_report.service -f

停止和禁用：
    systemctl disable --now cpu_temp_report.service

卸载：
    systemctl disable --now cpu_temp_report.service
    rm -f /etc/systemd/system/cpu_temp_report.service
    rm -rf /usr/local/cpu_temp_report
    systemctl daemon-reload

隔离说明：
本安装包只使用 cpu_temp_report 名称，不会安装、覆盖、停止或删除 sys_led
状态灯服务及其 /usr/local/led 目录。
