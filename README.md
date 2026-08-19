# 启明星耗材烘干箱固件（STM32F103C8T6）

基于 STM32F103C8T6 + ESP-01S 的耗材烘干箱固件，分为 **Bootloader（OTA 无线升级引导）** 与 **App（烘干控制逻辑）** 两部分。

## 项目特性

- 🔥 **PTC 烘干控制**：PTC 加热 + 温度控制 + 过温保护 + PID 自整定
- 📡 **OTA 无线升级**：ESP-01S AP 模式，浏览器网页上传固件，无需数据线
- 🖥️ **TFT 屏幕**：1.14 寸 ST7789（135×240），显示升级状态/AP 信息/进度条
- 💾 **外部 Flash（W25Q128，16 MiB）**：参数/用户数据持久化（WiFi 配置存 ESP 端）
- 🎛️ **多传感器**：温湿度（AHT20）、称重（CS1237）、NTC 温度检测
- ⚙️ **电机控制**：步进电机（连续/摆动模式）

## 环境要求

| 工具 | 用途 | 获取方式 |
|---|---|---|
| VSCode | 代码编辑 + 编译 | https://code.visualstudio.com |
| EIDE 扩展 | 嵌入式工程管理/构建 | VSCode 扩展市场搜索 "Embedded IDE" |
| Keil MDK（ARMCC5/AC5） | 编译工具链 | Keil 官网安装 MDK v5（含 ARM Compiler 5） |
| PW Link / ST-Link | 烧录下载 | 硬件调试器 + 对应烧录软件 |

> EIDE 构建时编译器路径在 `project/MDK(V5)/.eide/env.ini` 中配置（如 `D:\keil\ARM\ARMCC\bin`）。
> 若 Keil 安装在别处，需修改该文件中的编译器路径。

## 目录结构

```
├── app/              App 应用（烘干控制、UI、WiFi 配置、Web 管理）
├── board/            板级抽象（引脚定义、看门狗、硬件初始化）
├── bootloader/       Bootloader（ESP01S AP 模式 OTA 升级）
├── bsp/              外设驱动（SPI/Flash/USART/TFT/传感器等）
├── module/           App 业务模块（OTA、WiFi、Web 服务器、系统时间等）
├── shared/           平台契约（Flash 分区地址、共享协议）
├── libraries/        标准外设库 + CMSIS
└── project/MDK(V5)/  EIDE/Keil 工程文件
```

## Flash 分区布局（内部 Flash，64 KiB）

STM32F103C8T6 内部 Flash 共 64 KiB，地址范围 `0x08000000` ~ `0x0800FFFF`。布局如下：

| 区间 | 地址 | 分区大小 | 当前固件 | 说明 |
|---|---|---|---|---|
| Bootloader | `0x08000000` | 12 KiB（`0x3000`） | ~11.4 KiB | ESP 网页 OTA 升级引导 |
| 升级标志 | `0x08003000` | 1 KiB（`0x400`） | - | 升级请求标志 + 版本信息 |
| App | `0x08003400` | 51 KiB（`0xCC00`） | - | 烘干控制固件（WiFi 参数已交给 ESP01S 存储，分区并入 App） |

分区校验（编译期静态断言）：
- Bootloader 结束 = 标志区起始 ✓
- 标志区结束 = App 起始 ✓
- App 结束 = Flash 末尾 ✓
- 各分区大小均为页大小（1 KiB）整数倍 ✓

> 注意：OTA 固件由 Bootloader 校验通过后**直写内部 App 分区**，不再经外部 Flash 暂存。

> ⚠️ **ESP 电源 MOS 硬件注意**：ESP 高边开关使用 **AO3401（P-MOS）**，SOT-23 引脚为 1=栅极、2=源极、3=漏极（S 接 3.3V，D 接 ESP VCC，G 接 PA12）。
> 原设计焊盘若按 AO3400（N-MOS）布局，直接焊 AO3401 会导致源/漏接反，体二极管直通、软件无法关断 ESP——需将 2/3 引脚对调。

## 外部 Flash 布局（W25Q128，16 MiB）

W25Q128 外部 Flash 共 16 MiB，用于参数存储、用户数据等：

| 区间 | 地址 | 大小 | 说明 |
|---|---|---|---|
| 用户数据 | `0x00000000` | 12 MiB | 参数/日志等持久化数据（含 WiFi 配置） |
| 保留区 | `0x00C00000` | ~3.94 MiB | 预留（OTA 已改为直写内部 Flash，不再使用） |
| 元数据 | `0x00FF0000` | 64 KiB | 升级元数据（主/备份） |

## 编译

### 导入工程

1. VSCode 打开工程目录：`File → Open Folder` → 选择 **`project/MDK(V5)`**
2. 确保已安装 **EIDE** 扩展，自动加载 `.eide/eide.yml` 配置
3. EIDE 面板显示两个目标：**Bootloader** 和 **APP**（当前活动目标高亮）

### 构建两个目标

| 目标 | 操作 | 产物 |
|---|---|---|
| Bootloader | `Ctrl+Shift+B` → `Build Bootloader` | `Objects/Bootloader/dryer_bootloader.bin` |
| App | `Ctrl+Shift+B` → `Build APP` | `Objects/APP/dryer_app.bin` |

- Bootloader 目标带 `BOOTLOADER_BUILD` 宏（`eide.yml` 已配置），App 目标不带
- 两目标各自只有唯一的 `main` 入口，互不冲突
- 清除重编：EIDE 面板 → Clean / Rebuild

### 切换目标

点击 EIDE 面板顶部的目标下拉框，选择 `Bootloader` 或 `APP`，或直接运行对应任务。

## 烧录

使用 PW Link / ST-Link 等烧录器，将两个 bin 分别烧到对应地址：

| 文件 | 烧录地址 |
|---|---|
| `dryer_bootloader.bin` | `0x08000000` |
| `dryer_app.bin` | `0x08003400` |

烧录确认：
- Bootloader 当前 ~11.4 KiB，分区上限 12 KiB，不超限
- 两个文件地址不重叠（Bootloader 到 `0x08002FFF`，标志区从 `0x08003000` 开始）

## 代码入口

| 文件 | 作用 |
|---|---|
| `bootloader/bl_main.c` | Bootloader 主流程：开机判定、升级模式、跳转 App |
| `bootloader/bl_esp01s.c` | ESP-01S 通信：AT 命令、AP 配置、OTA 二进制串口协议接收（包级 ACK/NAK + CRC16/CRC32） |
| `bootloader/bl_tft.c` | Bootloader 侧屏幕驱动（ST7789 精简版，含 5×7 点阵字体） |
| `app/main.c` | App 主循环：初始化、传感器采集、控制逻辑 |
| `board/pin_config.h` | **全部引脚定义**（修改硬件设计需同步） |
| `shared/platform_contract.h` | Flash 分区、地址、平台常量（编译期断言验证） |

## 自定义配置

### 修改 WiFi 热点

编辑 `bootloader/bl_esp01s.c` 中 `BL_ESP01S_StartOta()`：

```c
/* 默认热点已硬编码为 QiMingXing / 12345678，修改需同步 ESP-01S 固件 AT+OTAAP 内的 AP_SSID/AP_PASS */
ESP_SendCmd("AT+OTAAP", "OK", 800);
//                ^改为 AT+OTAAP^   ^ESP 自定义固件开 AP 并等待二进制 OTA 转发^
```

### 修改引脚

编辑 `board/pin_config.h`（例如更换 ESP 串口、屏幕引脚时）。

### 修改 Flash 分区

编辑 `shared/platform_contract.h` 中的 `PLATFORM_APP_ADDR` 等常量，同时同步：
- `project/MDK(V5)/.eide/eide.yml` 中两个目标的 `ro-base` 和 IROM 大小
- Keil 工程（`Project.uvprojx`）中 `<Cpu>` 的 IROM 参数
- 运行 `scripts/configure_keil_targets.py` 自动同步 Keil 工程

## OTA 升级流程

> OTA 链路采用 **自定义二进制串口协议**（非浏览器直传）：网页/HTTP 解析全部由 ESP-01S 完成，STM32 只在 UART 上按 1 KiB/包的二进制协议收固件并**直写内部 App 分区**，因此即使 STM32 仅 20 KB RAM 也能稳定升级。

### 进入升级模式

Bootloader 每次上电时执行以下判断：

1. 读取升级标志
2. **无有效 App** → 进入升级模式（开 AP 热点，等待上传）
3. 有有效 App 且无升级请求 → 正常跳转 App
4. **兜底：上电长按编码器按键**（约 3 秒窗口，PB5 按下为低电平）直接进入下载模式，无论 App 是否有效 —— 避免 OTA 写入坏固件后"变砖"无法再进升级界面

两种方式进入升级模式：

- **首次烧录**（只烧 Bootloader，没有 App）→ 自动进入升级模式
- **App 内升级**：App 的 Web 管理页面点击"固件升级"→ 写入升级标志并复位 → 进入 Bootloader 升级模式
- **强制下载**：设备上电时长按编码器按键 → 强制进入升级模式

### 上传固件流程（单阶段直写内部 Flash）

**阶段 — 浏览器 → ESP-01S → STM32 → 内部 App 分区**

1. 手机/电脑搜索 WiFi 热点 **`QiMingXing`**（密码 **`12345678`**），约数秒后出现
2. 连接后浏览器访问 **`http://192.168.4.1`**
3. 选择 `.bin` 固件文件，点击 **上传并更新**
4. 浏览器先在前端用 JS 计算文件 CRC32 并随 `?crc=` 上传；ESP 收到后**再次计算 CRC32**，若与浏览器上报值不一致则判定为 WiFi 上传链路污染，拒绝转发（返回 `FIRMWARE CRC MISMATCH`），避免坏固件进入 STM32
5. 校验通过的固件由 ESP 按二进制协议经 UART 转发给 STM32：
   - **握手**：ESP → STM32 `[0xAA 0x55 0x01] + [4 字节固件大小 大端]`；STM32 收到后**先整区擦除内部 App 分区**（约 2s），擦完回 `0x06`(ACK)（ESP 侧握手等待已放宽到 10s，擦除期间主机不发包）
   - **数据包**：每 1 KiB 一包 `[0xAA] + [2 字节包序号大端] + [≤1024 数据] + [2 字节 CRC16(Modbus)] + [0x55]`，STM32 回 `0x06`(ACK) 或 `0x15`(NAK 重传)
   - **结束**：ESP → STM32 `[0xAA 0x55 0x02] + [4 字节总 CRC32 大端]`，STM32 回 ACK
   - STM32 **边收边直写内部 Flash App 分区**（按 256 字节页编程），屏幕进度条每 1% 差分局部刷新
6. 收完后 STM32 **整包 CRC32 全量校验**（从内部 Flash 重算比对 + 向量表校验），通过后关闭网页，设备自动重启进入新 App

> 升级全程无需人工干预。包级 ACK/NAK + 整包 CRC32 双重防护。误入升级模式但未上传固件时**不会擦除 App**，旧固件保留。

### ESP-01S 自定义固件（独立仓库）

ESP-01S 已替换为自定义 Arduino 固件（`QiMingXing-ESP01S` 仓库），不再依赖官方 stock AT。自定义指令：

| 指令 | 说明 | STM32 用法 |
|---|---|---|
| `AT` | 探测 ESP 就绪，回 `OK` | Bootloader 上电先发 `AT` 等 `OK` |
| `AT+OTAAP` | 开 SoftAP(`QiMingXing`/`12345678`) + 网页上传固件，上传完按二进制协议转发给 STM32 | Bootloader 发 `AT+OTAAP\r\n`，等 `OK\r\n` 后进入 UART 接收状态 |
| `AT+CFGAP` | 开配网 AP，网页选周边 WiFi 并回 `+IP:xxx.xxx.xxx.xxx` | 需配网时发 `AT+CFGAP\r\n` |
| `AT+PUSHDATA=<str>` | 缓存数据，数据展示页每 2 秒轮询显示 | App 定时发送 |
| `AT+OTACLOSE` | 关闭所有 Web Server，ESP 进入 Modem-Sleep 低功耗（**由 STM32 控制时机**） | 固件升级完成/无需网络时发送 |

## 常见问题

**Q: 编译报错 `main` 重复定义？**
A: 检查目标宏。Bootloader 必须带 `BOOTLOADER_BUILD` 宏，App 目标不能带。若 `eide.yml` 的 excludeList 未正确排除，会导致 `app/main.c` 的 `main()` 进入 Bootloader 链接。

**Q: 找不到热点？**
A: ① 确认只烧了 Bootloader 或升级标志为 DOWNLOADED（否则 Bootloader 会直接跳 App 不开 AP）；② 检查 ESP-01S 供电正常（PA12 控制 AO3401 P-MOS 为 ESP 供电）。

**Q: 热点连上了但网页打不开？**
A: 确认已烧录**自定义 ESP-01S 固件**（`QiMingXing-ESP01S` 仓库），网页由 ESP 自身托管，不再依赖 Bootloader 的 `+IPD` 解析。Bootloader 只负责在 `AT+OTAAP` 后按二进制协议收固件。

**Q: OTA 上传提示 FIRMWARE CRC MISMATCH？**
A: 浏览器上报的 CRC32 与 ESP 端重新计算的文件 CRC32 不一致，说明 WiFi 上传链路出现丢包污染。这是预期的防护，直接在网页重试即可（TCP 节流通常重试即成功）。

**Q: 进度卡在某百分比不动 / 写入 W25Q128 报 WERR？**
A: 大固件通过 SPI1 写外部 Flash 对时序较敏感，可降低 SPI 速率（如 `/32` 分频）或确认 `W25Q128_ClearProtection()` 已清状态寄存器 BP 写保护位；写前已回读 WEL 确认页编程未被硬件忽略。

**Q: 烧录后反复重启？**
A: 检查 SPI/延时循环中是否喂狗。确认 `Watchdog_Kick()` 在长循环中调用（如 `Delay_ms` 内部每 131072 次迭代喂一次）。

**Q: TFT 屏幕不亮？**
A: 确认背光引脚（PB0）配置为推挽输出并置高。若硬件上背光 MOS 已拆除，需将背光 LED 负极直接接地或通过电阻接 3.3V。

## 更新日志

### 2026-08-19

#### 变更（分区收敛 + OTA 直写）
- **Bootloader 分区缩至 12 KiB（`0x08000000`~`0x08002FFF`）**：体积瘦身（移除全部 `sprintf/printf`，省约 1.2 KiB printf 核心库），原 ~12.6 KiB bin 降至 ~11.4 KiB
- **移除内部 WiFi 配置分区**：WiFi 参数已交给 ESP01S/外部 Flash 存储；分区并入 App，App 起点改为 `0x08003400`、大小 51 KiB（`0xCC00`），`VECT_TAB_OFFSET=0x3400`
- **OTA 改为直写内部 App 分区**：删除外部 Flash 暂存/二次拷贝流程（`verify_staged_image`/`flash_staged_to_app`/`EnterCopyMode`）；握手校验大小合法后擦除 App 分区，数据包边收边直写，收完整包 CRC32 + 向量表校验通过即复位跳转
- **擦除时序**：握手 → 先整区擦除（约 2s）→ 回 ACK；ESP 侧握手等待放宽至 10s，擦除期间 ESP 不发数据包，消除 USART 溢出丢字节导致的首包损坏
- **FSM 容错**：`S_PKT_AA`/`S_PKT_TAIL` 对噪声/残串忽略而非置错；握手后 20s 无完整包超时中止重试，不再死等
- **进度条差分刷新**：每 1% 只画 2~3px 增量（40ms 节流），文字不动，人眼不可见刷新
- **SPI1 频率 36 MHz**（`SPI_BaudRatePrescaler_2`，F103 极限）

#### 修复（按键进菜单 + UI 闪烁）— 8 月 19 日测试正常版
- **App 启用全局中断**：Bootloader 在 `BootloaderV2_JumpToApp()` 跳转前调用 `__disable_irq()`，而 App 的 `SystemInit`/`main` 从不重新开中断 → `PRIMASK` 保持 1 → SysTick 永不触发 → `SystemTime_Millis()` 冻结 → 编码器**单击/长按**（依赖 millis 计时）全部失效（旋转仍可用，因其只轮询 GPIO）。`main.c` 在 `SystemTime_Init()` 后新增 `__enable_irq()`，恢复 SysTick 走时 → 长按 1s 进菜单、单击导航均恢复。
- **上电强制下载窗口**：原代码先 `Encoder_Process()`（内部消费事件）再 `Encoder_GetEvent()`，永远拿不到 `LONG_PRESS`；改为直接轮询 PB5 引脚，强制进 Bootloader 真正可用。
- **主界面时间栏闪烁**：`UI_UpdateMainDynamic()` 原每 50ms 无条件重绘底部时间文字，`TFT_DrawChar` 先填字符格背景再画前景 → 直接写 GRAM 无双缓冲撕裂闪；改为仅当时间字符串变化时重绘（值未变不画），REM 倒计时同理 gating 并在退出烘干时清残留行。
- **菜单旋转无效**：`UI_Update()` 同屏动态分支无 `SCREEN_MENU`，旋转改 `selected_item` 后不重绘；新增 `SCREEN_MENU` 局部分支 + `UI_RefreshMenuSel(old,new)` 只重绘旧/新两行（不整屏刷新），旋转即可移动选中项。
- 顺带：ESP-01S 握手 ACK 等待 3s→10s，覆盖 STM32 擦除 App 分区（约 2s）耗时（详见 `QiMingXing-ESP01S` 仓库）。

### 2026-08-16

#### 新增
- Bootloader 完整 OTA 升级链路：ESP01S AP + Web 网页上传固件（两阶段：下载到外部 Flash → 拷贝到 App 分区）
- 上传固件暂存 W25Q128 → 校验向量表 → 刷入内部 Flash App 分区
- App 端 Web 页面"固件升级"按钮：写入升级标志并复位跳转 Bootloader
- `bl_tft.c` 实现 5×7 点阵字体，屏幕可显示 AP 名称/密码/IP/升级进度
- 屏幕适配 1.14 寸 135×240 ST7789 横屏（240×135），UI 完整显示标题/状态/进度/AP 信息

#### 修复
- **OTA 分块上传协议**：改为 1 KiB 分块 POST（替代单次 multipart 大 POST），每块响应 `Content-Length: 0`，让浏览器立即完成 XHR，解决"卡 10%"
- **HTTP 解析**：`\r\n\r\n` 后进入 body 阶段，避免把换行符当固件数据写入
- **+IPD 帧残留**：`GET /done` / `GET / ` 等处理提前返回后，剩余帧字节被 `ESP_WaitResponse` 消耗，导致 `in_ipd`/`ipd_remain` 状态残留、吃掉下一请求数据——新增 `ipd_skip_pending` 机制丢弃剩余帧字节
- **ESP 电源关断**：`EnterCopyMode` 和 `JumpToApp` 中初始化 ESP_EN 引脚（PA12）为推挽输出并置高，同时 PA9（UART TX）输出低电平克服 ESP 内部上拉电阻导致的回灌供电（2.6V 伪供电）
- **拷贝模式 SPI1 未初始化**：`EnterCopyMode` 中先调 `W25Q128_Init`（含 `Spi1Bus_Init`）再 `BL_TFT_Init`，解决屏幕黑屏
- **超时处理**：10s 无数据时若 `fw_received == total_expected` 直接完成传输，不等浏览器 `/done`，避免浏览器响应丢失导致"Upload Error"
- `verify_staged_image` 修正向量表在偏移 0 时被误判"未找到"
- 上传页 JS 精简：移除浏览器进度条、"OK, restarting..." 文字（节省 ~400 B）

#### 变更
- **移除旧 v1 Bootloader**（`boot_main.c`/`boot_updater.c`/`boot_jump.c`/`boot_recovery.c`/`boot_platform_stm32.c` 及对应 `.h`），已由 V2 引导（`bl_main.c`）替代
- 清理 host 测试及 Makefile 中旧 v1 bootloader 引用
- `eide.yml` App 分区修正为 `0x08005000`/`0xB000`（原 `0x08004800`/`0xB800` 错误，会覆盖升级标志区）
- `scripts/configure_keil_targets.py` Bootloader 入口改 `bl_main.c`，分区地址与 `platform_contract.h` 一致
- Keil 工程（`Project.uvprojx`、`Project.uvoptx`）已同步至新分区配置

### 2026-08-18

#### 新增
- **主界面浅色主题**：浅灰背景 + 白卡片 + 灰描边 + 各卡片淡彩底色（温度暖橙 / 湿度冷青 / 重量紫 / PTC 红 / 时间浅绿），替代原深色蓝黑主题
- **圆角描边**：新增 `draw_frame_rounded()`，四角圆弧像素落在 `[r-SEL_FRAME_W, r]` 圆环带内，卡片与菜单选中项圆角均带完整灰色外轮廓
- **数值+单位统一绘制**：抽取 `draw_value_unit()` 公共函数，单位（℃/g）与数字同字号 size2、紧跟数值后，全屏与局部刷新共用，消除单位字号/位置不一致
- **时间栏增强**：烘干时间栏增加运行状态文字（IDLE/HEAT/DRY/COOL/DONE）与 REM 剩余时间显示
- **SPI1 TX DMA 局部刷新**：新增 `Spi1Bus_TransferDma()`（DMA1_Channel3），`TFT_FillRect` 大矩形（≥32 像素）走 DMA、小矩形（字符笔画等）走轮询，降低 SPI 刷新 CPU 占用
- **字体扩展**：`bsp_tft_st7789.c` 新增 `&` 与撇号 `'` 字形

#### 修复
- 开屏 "QiMingXing" X 方向居中（10 字符 × 18px = 180px，原误用 198px）；"LianYan & -e-" 居中（156px，原 168px）
- ℃ 单位圆圈位置修正：° 小圆圈（scale=1）置于 C 左上角，与 C 分离不重叠；C/g 颜色与数字同步（原灰色 `UI_TEXT_DIM`）
- **旋转编码器文字闪烁**：`UI_UpdateMainDynamic()` 原每 50ms 无条件用固定 `CARD_BG_*` 底色重绘值文字、未考虑选中态底色（`UI_CARD_HI`），导致选中卡文字反复擦写闪烁；改为值文字底色随 `selected_item` 动态选择，并加 `last_val[4]` 缓存（值不变不重绘）+ `last_sel` 检测（切换选中项时统一刷底色）
- `draw_card_pulse` 末行缩进错乱修复

#### OTA 协议重构（与 `QiMingXing-ESP01S` 自定义固件配套）
- **HTTP 解析从 STM32 转移到 ESP-01S**：Bootloader 不再解析 `+IPD`/HTTP，改由 ESP 自定义固件托管网页、接收固件并以二进制串口协议转发给 STM32（1 KiB/包，包级 ACK/NAK + 包内 CRC16 Modbus + 整包 CRC32 IEEE802.3），STM32 仅 `BL_ESP01S_Process()` 收二进制并写 W25Q128
- **整包 CRC32 全量校验**：纠正旧版"只校验向量表"漏检大固件静默损坏的问题；ESP 端入口校验——浏览器端算文件 CRC32 随 `?crc=` 上传，ESP 重算不一致即拒绝转发（`FIRMWARE CRC MISMATCH`），挡住 WiFi 上传链路污染
- **ESP 休眠由 STM32 控制**：新增 `BL_ESP01S_CloseWeb()`（发 `AT+OTACLOSE`），在下载收完并写 DOWNLOADED 标志、复位前调用，让 ESP 关闭网页/AP 进入 Modem-Sleep
- **上电兜底强制下载**：`EncoderButton_HeldAtBoot()` 长按编码器（PB5）直接进入下载模式，避免坏固件变砖
- **W25Q128 写保护清除**：拷贝/下载前调用 `W25Q128_ClearProtection()` 清 BP 位，避免页编程被硬件忽略；写前回读 WEL 确认
- `BL_ESP01S_StartAP()` 重命名为 `BL_ESP01S_StartOta()`（发 `AT+OTAAP`）；移除 `BL_ESP01S_GetBodyLen()` 等 HTTP 相关接口