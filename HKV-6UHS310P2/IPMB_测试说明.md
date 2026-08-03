# IPMB 测试说明

---

## 1. 测试环境

### 硬件连接
- I2C1 (PB6/PB7) 与 I2C2 (PB10/PB11) 需在板上短接 (SDA-SDA, SCL-SCL)，构成回环总线
- 两块 NCA9511 I2C 缓冲器分别由 PA2 (IPMB-A) 和 PA3 (IPMB-B) 使能
- UART4 通过 USB 转串口连接 PC，波特率 115200，用于发送指令和观察日志

### I2C 地址
| 角色 | 通道 | 地址 |
|------|------|------|
| IPMB-A 主机 | I2C1 | 0x8E (8-bit) |
| IPMB-B 从机 | I2C2 | 0x8E (8-bit) |

### 通信速率
- 默认 100 Kbps

---

## 2. 测试方法

通过 UART4 串口工具发送 HEX 裸帧，格式为空格分隔的十六进制字节，无需 `0x` 前缀。

示例输入格式:
```
8E 18 5A 20 00 01 DF
```

日志输出包含:
- `[IPMB-A Master] TX` — 主机发出的请求帧
- `[IPMB-B] RX cmd=0x...` — 从机收到的命令
- `[IPMB-B] TX resp` — 从机响应的帧
- `[IPMB-A Master] RX` — 主机收到的响应帧

---

## 3. 标准 IPMB 测试

### 3.1 Get Device ID (获取设备信息)

**请求帧** (7 字节):
```
8E 18 5A 20 00 01 DF
```
| 字节 | 值 | 说明 |
|------|----|------|
| [0] | 0x8E | 响应地址 (从机地址) |
| [1] | 0x18 | netFn=0x06<<2, lun=0 |
| [2] | 0x5A | 头部校验 |
| [3] | 0x20 | 请求地址 (主机) |
| [4] | 0x00 | rqSeq=0, lun=0 |
| [5] | 0x01 | Get Device ID |
| [6] | 0xDF | 尾部校验 |

**期望响应**: 19 字节帧，CC=0x00，DeviceID=0x03

---

### 3.2 Get Sensor Reading (读取传感器数值)

**读取温度 (sensor 0x04)**:
```
8E 10 7E 20 00 2D 04 25
```
**读取 12V (sensor 0x03)**:
```
8E 10 7E 20 00 2D 03 26
```
**读取 3.3V (sensor 0x16)**:
```
8E 10 7E 20 00 2D 16 13
```
**读取风扇档位 (sensor 0x41)**:
```
8E 10 7E 20 00 2D 41 EE
```

**期望响应**: 11 字节帧(2026-07-06 起,原为12字节,已去掉协议外的多余填充字节),CC=0x00

data 3 字节 (2026-07-20 起,恢复满 16bit 采样精度):
| 偏移 | 说明 |
|------|------|
| 08h (data[0]) | 采样值 LSB |
| 09h (data[1]) | 采样值 MSB (不再固定 0x80) |
| 0Ah (data[2]) | bit7=事件允许上报(原 09h=0x80 的语义搬到这一位), bit6=保留固定0, bit5:0=阈值状态占位(维持0,未接入) |

08h/09h 按小端拼成 `int16_t` 后除以 100 还原成物理量(0.01 分辨率),例如 12.34V → 1234 = `D2 04`；风扇档位(0x41)是整数百分比,不除以 100。0Ah 默认值是 `0x80`(与 HKV-6UD2K-J2I_BMC 保持一致)。

---

### 3.3 Get Board System Status (获取板卡系统状态)

```
8E 18 5A 20 00 16 CA
```

**期望响应**: 22 字节帧，含 CPU 在位、自检结果、温度、版本等信息

---

### 3.4 Get Slot (获取槽位号)

```
8E 18 5A 20 00 15 CB
```

**期望响应**: 9 字节帧，slot 值由 GA0..GA4 GPIO 读取

---

### 3.5 Get Device SDR (标准分页读取,reservation ID + record ID + 偏移量 + 读取长度)

```
8E 10 62 20 00 21 00 00 00 00 00 FF C0
```
(字节6-11 依次为:reservation ID LS/MS=00 00,record ID LS/MS=00 00,偏移量=00,读取长度=FF 表示整条记录)

**期望响应**: 26 字节帧,`20 14 CC 8E 00 21 00 01 00 01 01 43 50 55 5F 54 45 4D 50 00 00 00 00 00 00 D1`(rqSeq 需与请求一致,固件不再自动改写)——完成码00,next record ID=1(还有下一条),record ID=0 对应温度传感器:sensorId=01,sensorType=01(温度),sensorName="CPU_TEMP"

仓库共5条记录(record ID 0..4,依次:温度/12V电压/3.3V电压/3.3V电流/12V电流),把请求里 record ID(字节8-9)改成1~4可读其他传感器,读到最后一条(record ID=4)时 next record ID 会变成 FFFF。

---

## 4. OEM 命令测试

### 4.1 Get Module Info — 传感器信息 (itemID=0x11)

请求传感器类型和名称:
```
32 2E A0 20 00 12 01 11 BC
```
| 字节 | 值 | 说明 |
|------|----|------|
| [0] | 0x32 | 响应地址 (从机 0x8E 的 7-bit) |
| [1] | 0x2E | netFn 原始值 (非标, 不左移) |
| [2] | 0xA0 | 头部校验 |
| [3] | 0x20 | 请求地址 (主机) |
| [4] | 0x00 | rqSeq=0, lun=0 |
| [5] | 0x12 | Get Module Info |
| [6] | 0x01 | item_count=1 |
| [7] | 0x11 | itemID=传感器信息 |
| [8] | 0xBC | 尾部校验 |

**期望响应**: ~92 字节帧，含 5 个传感器的名称和类型 (sensorId + sensorType + sensorName[14])

---

### 4.2 Get Module Info — 传感器数值 (itemID=0x12)

请求传感器读数:
```
32 2E A0 20 00 12 01 12 BB
```

**期望响应**: ~32 字节帧，item_count=1，itemLen=20 (5 个传感器 × 4 字节)
```
sensorId=1: 温度 × 10 (Int16 LE) + alarm
sensorId=2: 12V电压 × 10 + alarm
sensorId=3: 3.3V电压 × 10 + alarm
sensorId=4: 3.3V电流 (当前填 0x7FFF, alarm=0xFF)
sensorId=5: 12V电流  (当前填 0x7FFF, alarm=0xFF)
```

---

### 4.3 Get Module Info — 同时请求两个 item

```
32 2E A0 20 00 12 02 11 12 B9
```

**期望响应**: ~116 字节帧，同时包含传感器信息和数值

---

## 5. Chassis Control 测试 (开关机/复位)

### 5.1 Set FRU Activation 命令

**断电** (control_req=0x00):
```
8E B0 1E 20 00 0C 03 00 00 DB
```

**上电** (control_req=0x01):
```
8E B0 1E 20 00 0C 03 00 01 DA
```

**复位** (control_req=0x02):
```
8E B0 1E 20 00 0C 03 00 02 D9
```

**软关机** (control_req=0x03):
```
8E B0 1E 20 00 0C 03 00 03 D8
```

**软复位** (control_req=0x04):
```
8E B0 1E 20 00 0C 03 00 04 D7
```

帧格式: rsSA=0x8E, netFn_byte=0xB0, cs1=0x1E, rqSA=0x20, rqSeq=0, cmd=0x0C, VSO=0x03, FRU_ID=0x00, ctrl_req, cs2

**期望响应**: 9 字节帧，CC=0x00，VSO=0x03

---

## 6. 校验和速算

IPMB 使用 2's complement checksum：
```
cs = (uint8_t)(0 - sum(bytes))
```
含校验字节后，所有字节累加 mod 256 == 0。

### 速算各请求帧的校验和

| 命令 | cs1 (覆盖[0..1]) | cs2 (覆盖[3..N-1]) |
|------|------------------|-------------------|
| Get Device ID | 0x8E+0x18=0xA6 → cs1=0x5A | 0x20+0x01=0x21 → cs2=0xDF |
| Get Sensor (0x04) | 0x8E+0x10=0x9E → cs1=0x62? | 按 sensor num 计算 |
| Get Board Status | 0x8E+0x18=0xA6 → cs1=0x5A | 0x20+0x16=0x36 → cs2=0xCA |
| Get Module Info (1 item) | 0x32+0x2E=0x60 → cs1=0xA0 | 数据段 + 0xBB/0xBC |

---

## 7. 常见问题

| 现象 | 原因 | 排查 |
|------|------|------|
| TX err=1 (START) | I2C1 BUSY 卡死 (F103 勘误) | 代码已内置 SWRST 恢复，重启即可 |
| TX err=2 (NACK) | 从机地址不匹配 | 确认从机 OAR1=0x8E，主机发 0x8E |
| CS error, drop | 校验和不匹配 | 检查 UART4 发送的 hex 帧数据正确 |
| Slave ready 但 RX 全 0xFF | 从机未在 STOP 后准备好 tx_buffer | 确认从机 IPMB_B_Task 正常运行 |
