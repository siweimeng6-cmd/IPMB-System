 #!/bin/bash
# 脚本功能：每5秒将温度写入串口 /dev/ttyS3，并在本地终端回显状态

# 【按需修改】设置串口波特率，请根据你接收端设备的实际波特率进行调整（常见为 9600 或 115200）
BAUDRATE=115200

# 初始化串口配置：设置波特率、8位数据位、无校验、1位停止位，并关闭本地回显和换行符自动转换
# 这样可以极大降低接收端出现乱码的概率
stty -F /dev/ttyS3 $BAUDRATE cs8 -parenb -cstopb -echo -onlcr

echo "温度监控脚本已启动，正在向 /dev/ttyS3 发送数据..."

while true; do
    # 获取当前时间戳
    #current_time=$(date "+%Y-%m-%d %H:%M:%S")
    
    # 读取原始温度（毫摄氏度）
    raw_temp=$(cat /sys/class/hwmon/hwmon0/temp1_input)
    # 转换为摄氏度，保留3位小数
    temp_c=$(echo "scale=3; $raw_temp / 1000" | /usr/bin/bc)
    
    # 将温度数据写入串口
    echo "$temp_c" > /dev/ttyS3
    
    # 检查上一条命令（写入串口）是否执行成功
    # if [ $? -eq 0 ]; then
    #   写入成功，在本地终端打印回显温度和成功提示
    # echo " 当前温度: $temp_c  :-> 成功写入串口"
    # else
    # 写入失败，在本地终端打印错误警告
    #   echo "警告：写入串口 /dev/ttyS3 失败！请检查设备是否存在或被占用。"
    #  fi
    
    # 等待2秒
    sleep 2
done


