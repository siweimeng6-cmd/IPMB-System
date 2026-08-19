# VPX6-Master

IPMB 1.0 规范通信主机（Master）固件工程，基于 STM32F407。请求与应答均由各自发起方主动 `write`，非轮询模式。

从机地址识别算法：从机读取背板 GA0\~GA4 共 5 个地址引脚（低电平有效，读入后按位取反）得到槽位号 Slot_ID（0\~31），按 `I2C 地址 = 0x30 + (Slot_ID << 1)` 计算出自己的 IPMB 地址（不再固定为 0x8E）。主机侧不直接读取从机 GPIO，而是由 `IPMB_Discovery_Task`（[`User/I2C/task_i2c.c`](User/I2C/task_i2c.c)）每 15s 在 IPMB-A/B 双总线上后台扫描 0x30\~0x6E 全部候选地址（逐一发送 Get Device ID 探测），根据实际应答动态建立在线从机地址表 `g_slave_addrs[]`，并通过 Get Slot（cmd 0x15）二次核实从机上报的槽位号。

## 关键宏定义

| 宏 | 位置 | 说明 |
|---|---|---|
| `IPMB_PEM_AUTO_POLL_ENABLE` | [`User/I2C/task_i2c.h`](User/I2C/task_i2c.h) | PEM（Platform Event Message）自动轮询上报总开关。`1` = 每 5s 自动轮询并打印（默认）；`0` = 关闭自动轮询，串口不再周期性刷 PEM 上报，但不影响手动 `ipmb 3x ... 02` 查询。 |
| `BOARD_USE_LY1210A` | [`User/ethernet/bsp_ethernet.h`](User/ethernet/bsp_ethernet.h) | 以太网 PHY 芯片选型总开关。`1` = 量产板 LY1210A-IR-QFN32（MII）；`0` = 测试板 LAN8720A-CP-TR-ABC（RMII，默认）。 |
| `Ipmb_SlotToAddr(slot)` / `Ipmb_AddrToSlot(addr)` | [`User/I2C/task_i2c.h`](User/I2C/task_i2c.h) | 从机 IPMB 地址与背板槽位号互换宏：`addr = 0x30 + (slot << 1)`，slot 范围 0~31。 |

## 修改记录

| 日期 | 说明 |
|---|---|
| 2026-07-31 | 初始提交 |
| 2026-08-02 | 合并入 [IPMB-System](../README.md) 仓库，保留原始提交历史 |
| 2026-08-17 | Get Slot（槽位号回读）改为每轮轮询都实时查询，不再只在从机首次被发现时读一次 |
| 2026-08-17 | Get Board System Status 复用原保留字节，新增真实电源开关状态回读（从机现读 PA4/PWR_CTL 引脚电平），网页 Board Status 卡片同步显示"开关机状态" |
| 2026-08-18 | Get Board System Status 的 CPU 温度字段（响应第 18 字节）改为承载主机真实 CPU 温度：J2I-Slave1 填主机经串口上报的值（`task_fan.c` `Fan_GetCpuTempDebug`，即调试串口 `CpuTemp:xxC`），0xFF 表示未上报、网页显示"未使用"；板卡本体温度改由传感器 0x04（温度1）提供 |


