# 启明星耗材烘干箱固件（STM32F103C8T6）

基于 STM32F103C8T6 + ESP-01S 的烘干箱固件，含 **Bootloader（OTA 升级）** 与 **App（烘干控制）** 两部分。

## 目录结构

```
├── app/          App 应用（烘干控制、UI、WiFi 配置、Web 管理）
├── board/        板级抽象（引脚定义、看门狗、硬件初始化）
├── bootloader/   Bootloader（ESP01S AP 模式 OTA 升级）
├── bsp/          外设驱动（SPI/Flash/USART/TFT/RGB/传感器等）
├── module/       App 业务模块（OTA、WiFi、Web 服务器、系统时间等）
├── shared/       平台契约（Flash 分区地址、共享协议）
├── libraries/    标准外设库 + CMSIS
└── project/MDK(V5)/  EIDE/Keil 工程文件
```

## Flash 分区布局

| 区间 | 地址 | 大小 | 说明 |
|---|---|---|---|
| Bootloader | `0x08000000` | 16 KB | OTA 引导 + AP 升级入口 |
| 升级标志 | `0x08004000` | 2 KB | 升级请求标志 |
| App | `0x08004800` | 46 KB | 烘干控制固件 |

## 编译

工程使用 **EIDE（Embedded IDE for VSCode）** 管理，编译器为 Keil ARMCC5（AC5）：

1. 用 VSCode 打开 `project/MDK(V5)` 目录（EIDE 扩展会自动识别 `eide.yml`）
2. 构建两个目标：
   - **Bootloader**：`Ctrl+Shift+B` 选择 `Build Bootloader`
   - **App**：`Ctrl+Shift+B` 选择 `Build APP`
3. 产物：
   - Bootloader：`Objects/Bootloader/dryer_bootloader.bin`
   - App：`Objects/APP/dryer_app.bin`

> 说明：Bootloader 目标编译时必须带 `BOOTLOADER_BUILD` 宏（eide.yml 已配置）；App 目标不带。两目标各自只有唯一的 `main` 入口，互不冲突。

## 如何使用本项目代码

### 开发环境搭建

| 工具 | 用途 | 获取方式 |
|---|---|---|
| VSCode | 代码编辑 + 编译 | https://code.visualstudio.com |
| EIDE 扩展 | 嵌入式工程管理/构建 | VSCode 扩展市场搜索 "Embedded IDE" |
| Keil MDK（ARMCC5/AC5） | 编译工具链 | Keil 官网安装 MDK v5（含 ARM Compiler 5） |
| PW Link / ST-Link | 烧录下载 | 硬件调试器 + 对应烧录软件 |

> EIDE 构建时编译器路径在 `project/MDK(V5)/.eide/env.ini` 中配置（如 `D:\keil\ARM\ARMCC\bin`），若你的 Keil 安装在别处，需修改该文件中的编译器路径。

### 导入工程

1. VSCode 打开工程目录：`File → Open Folder` 选择 **`project/MDK(V5)`**
2. 确保已安装 **EIDE** 扩展，它会自动加载 `.eide/eide.yml` 工程配置
3. 左侧 EIDE 面板应显示两个目标：**Bootloader** 和 **APP**（当前活动目标高亮）

### 构建

- **构建当前目标**：`Ctrl+Shift+B`（或右键 EIDE 面板目标 → Build）
- **切换目标**：点击 EIDE 面板顶部的目标下拉框选择 `Bootloader` 或 `APP`
- 构建产物（bin）输出到 `Objects/` 目录：
  - `Objects/Bootloader/dryer_bootloader.bin`
  - `Objects/APP/dryer_app.bin`
- 清除重编：EIDE 面板 → Clean / Rebuild

### 代码组织与常用入口

| 文件 | 作用 |
|---|---|
| `bootloader/bl_main.c` | Bootloader 主流程：开机判定 → 升级模式 / 跳转 App |
| `bootloader/bl_esp01s.c` | ESP-01S 通信：AT 命令、AP 配置、网页上传解析 |
| `bootloader/bl_tft.c` | Bootloader 屏幕驱动（ST7789 精简版） |
| `app/main.c` | App 主循环：初始化、传感器采集、控制逻辑 |
| `module/mod_wifi_manager.c` | App 侧 WiFi 管理 |
| `module/http_server.c` | App 侧 Web 服务器（管理页面） |
| `board/pin_config.h` | **全部引脚定义**（修改硬件设计需同步这里） |
| `shared/platform_contract.h` | Flash 分区地址等平台常量 |

### 自定义配置

**修改 WiFi 热点名称/密码**：编辑 `bootloader/bl_esp01s.c` 中的 `BL_ESP01S_StartAP()`：

```c
ESP_SendCmd("AT+CWSAP=\"QiMingXing\",\"12345678\",1,4", "OK", 800);
//                ^SSID^            ^密码^   ^通道^  ^加密:4=WPA/WPA2^
```

**修改引脚**：编辑 `board/pin_config.h`（例如更换 ESP 串口、屏幕引脚时）。

**修改 Flash 分区**：编辑 `shared/platform_contract.h` 中 `PLATFORM_APP_ADDR` 等常量，同时同步 Bootloader 与 App 两个目标的链接地址（`eide.yml` 的 `ro-base`）。

### 常见问题

**Q: 编译报错 `main` 重复定义？**
A: 确认当前目标定义正确。Bootloader 目标必须带 `BOOTLOADER_BUILD` 宏，App 目标不能带。若 `app/main.c` 的 `main()` 进了 Bootloader 链接，检查 `.eide/eide.yml` 的 excludeList 是否生效。

**Q: 找不到热点？**
A: ① 确认只烧了 Bootloader 或升级标志为 DOWNLOADED（否则 Bootloader 会直接跳 App 不开 AP）；② 确认 ESP-01S 供电正常（PA12 控制 AO3401 为 ESP 供电）。

**Q: 热点连上了但网页 404？**
A: 重新编译确保包含最新的 `BL_ESP01S_Process()` 解析修复（旧固件的 `+IPD` 解析 bug 会导致 404）。

**Q: 烧录后反复重启？**
A: 检查是否有 SPI/延时死循环未喂狗。若使用独立屏幕/Flash 驱动，确认其内部延时函数已 `Watchdog_Kick()`。

## 烧录

使用 PW Link / ST-Link 等烧录器，把两个 bin 分别烧到对应地址：

| 文件 | 地址 |
|---|---|
| `dryer_bootloader.bin` | `0x08000000` |
| `dryer_app.bin` | `0x08004800` |

烧录时务必确认：
- Bootloader 大小 ≤ 16 KB
- 两个文件地址不重叠（Bootloader 到 `0x08003FFF`，标志区 `0x08004000-0x080047FF`，App 从 `0x08004800` 起）

## OTA 升级流程

### 进入升级模式

Bootloader 每次上电时：

1. 读取升级标志
2. **没有有效 App** 或 **升级标志为 DOWNLOADED** → 进入升级模式（开 AP）
3. 有有效 App 且无升级请求 → 正常跳转 App

因此有两种方式进入升级模式：

- **首次烧录**（只有 Bootloader、没有 App）→ 自动进入升级模式
- **App 内升级**：在 App 的 Web 管理页面点击"固件升级"→ 写入升级标志并复位 → 进入升级模式

### 连接并上传固件

1. 手机/电脑搜索 WiFi 热点 **`QiMingXing`**（密码 **`12345678`**）
2. 连接后浏览器访问 **`http://192.168.4.1`**
3. 选择 `.bin` 固件文件，点击 **UPLOAD & UPDATE**
4. 固件上传到外部 Flash（W25Q128）暂存 → 校验 CRC 与向量表 → 刷入 App 分区
5. 成功后设备自动重启，Bootloader 检测到有效 App 后跳转运行

> 注意：Bootloader 会在进入升级模式时发送 `AT+RST` 重置 ESP-01S 以清除残留配置，热点约需数秒后出现。

## 更新日志

### 2026-08-16

#### 新增
- Bootloader 完整 OTA 升级链路：ESP01S AP 模式 + Web 网页上传固件
- 上传固件暂存外部 Flash（W25Q128）→ 校验（CRC32 + 向量表）→ 刷入 App 分区
- App 端 Web 页面"固件升级"按钮：写入升级标志并复位跳转 Bootloader
- `bl_tft.c` 增加真实 5×7 点阵字体，屏幕可显示 AP 名称/密码/IP/升级进度

#### 修复
- **修复 `ESP_WaitResponse` / `BL_ESP01S_Process` 的 RX 读取逻辑反转 bug**（`EspUart_ReadByte` 返回 1=有字节，原代码写成 `==0`，导致 ESP 回复的每个字节都被丢弃、AT 握手永远失败）
- 修复 ESP 残留 STA 配置导致的热点延迟出现问题（进入升级模式时 `AT+RST` 重置 ESP）
- 修复 WS2812 时序错误（改用 SysTick 精确周期延时，符合 800kHz 协议）
- 修复 TFT 背光初始化缺失 `GPIOB` 时钟导致背光无法点亮
- 修复 TFT SPI 分频过高（36MHz→9MHz）、SPI 收发无超时导致看门狗复位
- 修复 `bl_tft.c` `Delay_ms` 不喂狗导致的复位循环
- `EspUart_Init` 不再在初始化时切换 ESP 电源，避免复位抖动
- `EspUart_SetEnabled` 适配 P 沟道 MOS（AO3401）低电平导通逻辑

#### 变更
- Bootloader 入口切换为 V2 引导（ESP AP 升级），移除 V1 引导文件
- OTA 上传协议改为先存外部 Flash 再刷入，支持校验后写入
- 热点名称 `QiMingXing`，密码 `12345678`

#### 已知问题
- 屏幕背光（BLK）当前为硬件短路（LED 击穿或焊连），背光无法点亮，需修复硬件后验证 TFT 显示
