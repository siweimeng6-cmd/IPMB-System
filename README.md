# IPMB-System

IPMB 1.0 规范主从通信系统固件工程合集。请求与应答均由各自发起方主动 `write`，非轮询模式。

## 子目录

| 目录 | 角色 | 主控芯片 | 原仓库 |
|---|---|---|---|
| [`VPX6-Master/`](VPX6-Master/) | 主机 | STM32F407 | VPX6-6UFT5K32-M128-731-IPMB-Master |
| [`J2I-Slave1/`](J2I-Slave1/) | 从机 1 | STM32F103 | HKV-6UD2K-J2I_BMC-IPMB-slave-1 |
| [`6UHS310P2-Slave2/`](6UHS310P2-Slave2/) | 从机 2 | STM32F103 | HKV-6UHS310P2_260716 |

每个子目录是独立的 Keil MDK 工程（FreeRTOS + lwIP/自研通信栈），各自维护自己的 `.gitignore`，编译产物（`Output/`、`Listing/`）不纳入版本库。

各子目录的提交历史通过 `git subtree` 从原独立仓库合并而来，保留了合并前的完整历史记录。
