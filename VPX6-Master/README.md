# VPX6-Master

IPMB 1.0 规范通信主机（Master）固件工程，基于 STM32F407。请求与应答均由各自发起方主动 `write`，非轮询模式。

## 关键宏定义

| 宏 | 位置 | 说明 |
|---|---|---|
| `IPMB_PEM_AUTO_POLL_ENABLE` | [`User/I2C/task_i2c.h`](User/I2C/task_i2c.h) | PEM（Platform Event Message）自动轮询上报总开关。`1` = 每 5s 自动轮询并打印（默认）；`0` = 关闭自动轮询，串口不再周期性刷 PEM 上报，但不影响手动 `ipmb 3x ... 02` 查询。 |
| `BOARD_USE_LY1210A` | [`User/ethernet/bsp_ethernet.h`](User/ethernet/bsp_ethernet.h) | 以太网 PHY 芯片选型总开关。`1` = 量产板 LY1210A-IR-QFN32（MII）；`0` = 测试板 LAN8720A-CP-TR-ABC（RMII，默认）。 |

## 修改记录

| 日期 | 说明 |
|---|---|
| 2026-07-31 | 初始提交 |
| 2026-08-02 | 合并入 [IPMB-System](../README.md) 仓库，保留原始提交历史 |


