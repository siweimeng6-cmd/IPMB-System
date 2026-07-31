# PHY 芯片切换 + 网页控制台占位页面 说明

## 背景

板子有两个版本：
- **测试板**：LAN8720A-CP-TR-ABC（24 脚，仅支持 RMII 接口）
- **量产板**：LY1210A-IR-QFN32（32 脚，MII/RMII 双模，寄存器与 LAN8720 系列兼容）

目标是先在测试板上把"IPMB 网页控制台"调通，量产板做好后只改一个宏就能切过去，不用再动其它代码。本次改动只做到 **"控制页面能在浏览器里打开"** 这一步，不含 IPMB 命令桥接、动态数据、表单交互——那些是下一步的工作。

---

## 一、PHY 芯片切换

### 开关位置
[`User\ethernet\bsp_ethernet.h`](bsp_ethernet.h) 第 23-32 行：

```c
/* PHY芯片选择总开关: 1 = 量产板LY1210A-IR-QFN32(MII), 0 = 测试板LAN8720A-CP-TR-ABC(RMII) */
#define BOARD_USE_LY1210A      0

#if BOARD_USE_LY1210A
  #define ETH_PHY_ADDRESS   0x03    /* LY1210A板载地址, 已验证 */
  #define MII_MODE
#else
  #define ETH_PHY_ADDRESS   0x00    /* LAN8720A占位地址, 以启动时串口打印的PHY扫描结果为准 */
  #define RMII_MODE
#endif
```

**换板子时只改 `BOARD_USE_LY1210A` 这一个数字**，其它代码不用动。

### 改动了什么
- 原来分散硬编码的 `LY1210_PHY_ADDRESS` 宏（`bsp_ethernet.c` 7 处、`task_ethernet.c` 1 处、`bsp_init.c` 1 处死代码）统一替换成 `ETH_PHY_ADDRESS`，跟着总开关联动。
- 修复了 [`LY1210.c`](LY1210.c) RMII 分支里的复位引脚 bug：原来注释写 PD3、`GPIO_Init` 配的是 PD5、实际复位函数硬编码 PD4，三处对不上；现已统一成 PD4，跟 `ETH_NRST_PIN_LOW/HIGH()` 实际操作的引脚一致。这是让测试板 RMII 模式下 PHY 复位真正生效的必要修复。

### 明确没动的部分
- [`stm32f4x7_eth_conf.h`](stm32f4x7_eth_conf.h) 里的 `PHY_SR`(0x11)/`PHY_SPEED_STATUS`(0x0008)/`PHY_DUPLEX_STATUS`(0x0002) 和 `LY1210_RESET_DELAY`(50ms)——这些是 LAN8720 系列的通用寄存器值，两块板子都适用。
- [`DP83848.c`/`.h`](DP83848.c)——历史遗留、根本没编译进 Keil 工程，不要重新启用（会和 `LY1210.c` 里同名函数 `ETH_GPIO_Config`/`ETH_NRST_PIN_LOW/HIGH` 冲突）。
- `bsp_init.h` 里的 `PHY_CLOCK_MCO`——确认是死宏，没有任何地方真正用它配置 MCO 时钟输出，不在本次范围。

### 待确认项
`ETH_PHY_ADDRESS` 在 LAN8720A 分支下先填占位值 `0x00`。`bsp_ethernet.c` 里已有一段上电自动扫描代码（扫描 MDIO 地址 0~31，跟 MII/RMII 模式无关，因为 MDIO/MDC 两根线两种模式下都在同样的 PA2/PC1），会在串口日志里报出真实地址；如果和占位值不一致，只改这一行宏即可。

---

## 二、网页控制台占位页面

### 现状（改动前）
工程里完全没有 HTTP 服务器：`lwip-1.4.1` 没有 `apps/httpd` 模块，没有任何 `.html`/`.js`/`.css` 资源。但 `lwipopts.h` 里 `LWIP_TCP=1`、`NO_SYS=0`（tcpip 线程已跑），`LWIP_NETCONN=0`、`LWIP_SOCKET=0`，所以只能用 lwIP raw API（跟现有 `UDPBase.c` 是同一套风格）。

### 新增内容
新增 [`bsp_httpd.c`](bsp_httpd.c) / [`bsp_httpd.h`](bsp_httpd.h)，用 lwIP raw TCP API 手写了一个最小 HTTP 服务：
- `httpd_init()`：`tcp_new → tcp_bind(80端口) → tcp_listen → tcp_accept`
- 收到任何请求都不解析路径，直接回一段内嵌的静态 HTML（标题 "IPMB Web Console"），然后关闭连接
- 在 [`bsp_ethernet.c`](bsp_ethernet.c) 的 `Ethernet_init()` 里，紧跟 `CreateUDPConnect()` 之后调用一次 `httpd_init()`
- 已加入 Keil 工程 `Project\RVMDK（uv5）\6UFT5K32_MCU.uvprojx` 的 `ETHERNET` 文件分组

### 明确不做的部分（留给下一步）
- 不解析请求路径、不做多页面路由
- 不接入 `task_usart.c`/`task_i2c.c` 现有的 IPMB 命令队列（`xIPMB_CmdQueue`/`xIPMB_RspQueue`）
- 不做表单/AJAX/CGI 动态交互

---

## 三、完整测试步骤（以 LAN8720A 测试板为例）

### 烧录前
1. `bsp_ethernet.h` 确认 `BOARD_USE_LY1210A` 为 `0`
2. Keil Rebuild All，确认 0 Error
3. 网线接好，串口调试助手连 USART1，波特率 **115200**，8N1

### 上电后按顺序核对串口打印
1. **GPIO/DMA**：`>>[ETH]DMA软复位完成(说明PHY时钟已到MAC)...OK`（若超时，查 PD4 复位/Y6 晶振/REFCLKO 接线）
2. **PHY 地址扫描**（关键一步）：`>>[ETH]  发现PHY @addr=X ...`
   - 若提示 `!! PHY真实地址=X, 但代码里ETH_PHY_ADDRESS=0, 请修改该宏 !!` → 改 `bsp_ethernet.h` 里 `ETH_PHY_ADDRESS` 的占位值为 X，重新编译烧录
   - 若全部无应答 → 硬件问题（PHY 供电/复位/MDIO-MDC 接线），先排硬件
3. **网络栈+HTTP启动**：`>>配置以太网成功...OK` → `>>初始化LWIP协议栈成功...OK` → `>>[HTTPD]网页服务已启动...OK`
4. **链路检测**（`Ethernet_LinkCheckTask` 每 2 秒轮询）：`>>网卡配置成功...OK`

### 网络层验证
1. 测试电脑网卡设成同网段静态 IP，如 `192.168.0.100/255.255.255.0`
2. `ping 192.168.0.20` 确认通
3. 浏览器访问 `http://192.168.0.20/`，应看到 "IPMB Web Console" 占位页面
   - 打不开但 ping 通：抓包看有没有 SYN-ACK，检查串口日志里 `>>[HTTPD]网页服务已启动` 是否打印出来
4.（可选）用网络调试助手监听 UDP `192.168.0.3:5000`，验证板子每 4 秒发的测试包，确认底层协议栈整体正常（原有功能，和本次改动无关）

### 以后切到 LY1210A 量产板
1. `bsp_ethernet.h` 的 `BOARD_USE_LY1210A` 改成 `1`
2. Rebuild、烧录
3. 重复上面全部检查，重点是 PHY 地址扫描——量产板走 `ETH_PHY_ADDRESS=0x03`

---

## 四、涉及改动的文件

| 文件 | 改动 |
|---|---|
| `bsp_ethernet.h` | 新增 `BOARD_USE_LY1210A` 总开关，替换原 `LY1210_PHY_ADDRESS`/`MII_MODE`/`RMII_MODE` 手动切换 |
| `bsp_ethernet.c` | `LY1210_PHY_ADDRESS`→`ETH_PHY_ADDRESS`（7处），新增 `httpd_init()` 调用 |
| `task_ethernet.c` | `LY1210_PHY_ADDRESS`→`ETH_PHY_ADDRESS`（1处） |
| `bsp_init.c` | `LY1210_PHY_ADDRESS`→`ETH_PHY_ADDRESS`（1处，死代码参数） |
| `LY1210.c` | 修复 RMII 分支复位引脚 bug（PD5→PD4） |
| `bsp_httpd.c` / `bsp_httpd.h` | 新增，最小 HTTP 服务 |
| `6UFT5K32_MCU.uvprojx` | 把 `bsp_httpd.c` 加入 ETHERNET 文件分组 |

## 五、已知遗留 / 后续工作

- IPMB 命令桥接（把网页表单/请求接到 `task_usart.c`/`task_i2c.c` 现有队列）尚未开始，需要单独规划
- `DP83848.c/.h`、`bsp_init.h` 里的 `PHY_CLOCK_MCO`、`bsp_ethernet.h` 里几个从未实现的 `Eth_Link_*` 函数声明——均为历史遗留死代码，本次未清理，不影响编译
