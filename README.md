# 启明星耗材烘干箱固件（STM32F103C8T6）

基于 STM32F103C8T6 + ESP-01S 的耗材烘干箱固件，分为 **Bootloader（OTA 无线升级引导）** 与 **App（烘干控制逻辑）** 两部分。

## 项目特性

- 🔥 **PTC 烘干控制**：PTC 加热 + 温度控制 + 过温保护 + PID 自整定
- 📡 **OTA 无线升级**：ESP-01S AP 模式，浏览器网页上传固件，无需数据线
- 🖥️ **TFT 屏幕**：1.14 寸 ST7789（135×240），显示升级状态/AP 信息/进度条
- 💾 **外部 Flash（W25Q128，16 MiB）**：固件暂存 + 参数持久化
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
| Bootloader | `0x08000000` | 18 KiB（`0x4800`） | ~16 KiB（15892 B） | ESP 网页 OTA 升级引导 |
| 升级标志 | `0x08004800` | 1 KiB（`0x400`） | - | 升级请求标志 + 版本信息 |
| WiFi 配置 | `0x08004C00` | 1 KiB（`0x400`） | - | WiFi 参数持久化存储 |
| App | `0x08005000` | 44 KiB（`0xB000`） | ~43 KiB（44184 B） | 烘干控制固件 |

分区校验（编译期静态断言）：
- Bootloader 结束 = 标志区起始 ✓
- 标志区结束 = WiFi 配置起始 ✓
- WiFi 配置结束 = App 起始 ✓
- App 结束 = Flash 末尾 ✓
- 各分区大小均为页大小（1 KiB）整数倍 ✓

> 注意：App 当前固件 ~43 KiB，分区上限 44 KiB，剩余 ~872 B 余量。若 App 后续增加功能可能超限，需调整分区（例如缩小 Bootloader 分区至 16 KiB，腾出 2 KiB 给 App）。

> ⚠️ **ESP 电源 MOS 硬件注意**：ESP 高边开关使用 **AO3401（P-MOS）**，SOT-23 引脚为 1=栅极、2=源极、3=漏极（S 接 3.3V，D 接 ESP VCC，G 接 PA12）。
> 原设计焊盘若按 AO3400（N-MOS）布局，直接焊 AO3401 会导致源/漏接反，体二极管直通、软件无法关断 ESP——需将 2/3 引脚对调。

## 外部 Flash 布局（W25Q128，16 MiB）

W25Q128 外部 Flash 共 16 MiB，用于固件暂存和参数存储：

| 区间 | 地址 | 大小 | 说明 |
|---|---|---|---|
| 用户数据 | `0x00000000` | 12 MiB | 参数/日志等持久化数据 |
| 固件暂存 | `0x00C00000` | ~3.94 MiB | OTA 下载时暂存固件，擦除大小 48 KiB |
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
| `dryer_app.bin` | `0x08005000` |

烧录确认：
- Bootloader 当前 ~16 KiB，分区上限 18 KiB，不超限
- 两个文件地址不重叠（Bootloader 到 `0x08004FFF`，标志区/WiFi 配置区从 `0x08004800` 开始）

## 代码入口

| 文件 | 作用 |
|---|---|
| `bootloader/bl_main.c` | Bootloader 主流程：开机判定、升级模式、跳转 App |
| `bootloader/bl_esp01s.c` | ESP-01S 通信：AT 命令、AP 配置、HTTP 分块上传解析 |
| `bootloader/bl_tft.c` | Bootloader 侧屏幕驱动（ST7789 精简版，含 5×7 点阵字体） |
| `app/main.c` | App 主循环：初始化、传感器采集、控制逻辑 |
| `board/pin_config.h` | **全部引脚定义**（修改硬件设计需同步） |
| `shared/platform_contract.h` | Flash 分区、地址、平台常量（编译期断言验证） |

## 自定义配置

### 修改 WiFi 热点

编辑 `bootloader/bl_esp01s.c` 中 `BL_ESP01S_StartAP()`：

```c
ESP_SendCmd("AT+CWSAP=\"QiMingXing\",\"12345678\",1,4", "OK", 800);
//                ^SSID^            ^密码^   ^通道^  ^加密:4=WPA/WPA2^
```

### 修改引脚

编辑 `board/pin_config.h`（例如更换 ESP 串口、屏幕引脚时）。

### 修改 Flash 分区

编辑 `shared/platform_contract.h` 中的 `PLATFORM_APP_ADDR` 等常量，同时同步：
- `project/MDK(V5)/.eide/eide.yml` 中两个目标的 `ro-base` 和 IROM 大小
- Keil 工程（`Project.uvprojx`）中 `<Cpu>` 的 IROM 参数
- 运行 `scripts/configure_keil_targets.py` 自动同步 Keil 工程

## OTA 升级流程

### 进入升级模式

Bootloader 每次上电时执行以下判断：

1. 读取升级标志
2. **无有效 App** 或 **升级标志为 DOWNLOADED** → 进入升级模式（开 AP 热点，等待上传）
3. 有有效 App 且无升级请求 → 正常跳转 App

两种方式进入升级模式：

- **首次烧录**（只烧 Bootloader，没有 App）→ 自动进入升级模式
- **App 内升级**：App 的 Web 管理页面点击"固件升级"→ 写入升级标志并复位 → 进入 Bootloader 升级模式

### 上传固件流程（两阶段）

**阶段 1 — 下载到外部 Flash**

1. 手机/电脑搜索 WiFi 热点 **`QiMingXing`**（密码 **`12345678`**），约数秒后出现
2. 连接后浏览器访问 **`http://192.168.4.1`**
3. 选择 `.bin` 固件文件，点击 **UPLOAD & UPDATE**
4. 浏览器以 1 KiB 分块方式上传固件，设备下载到外部 Flash（W25Q128），屏幕显示实时进度
5. 上传完成后校验向量表，写入升级标志，自动重启

**阶段 2 — 拷贝到内部 Flash**

6. 重启后 Bootloader 检测到升级标志，显示 "WRITE TO MCU" + 拷贝进度条
7. 从外部 Flash 读取固件，刷入内部 Flash App 分区，每 10% 刷新进度
8. 校验 App 向量表有效后，清标志并再次重启
9. 重启后 Bootloader 跳转新 App，App 正常运行

> 升级全程无需人工干预，进度条实时显示。

## 常见问题

**Q: 编译报错 `main` 重复定义？**
A: 检查目标宏。Bootloader 必须带 `BOOTLOADER_BUILD` 宏，App 目标不能带。若 `eide.yml` 的 excludeList 未正确排除，会导致 `app/main.c` 的 `main()` 进入 Bootloader 链接。

**Q: 找不到热点？**
A: ① 确认只烧了 Bootloader 或升级标志为 DOWNLOADED（否则 Bootloader 会直接跳 App 不开 AP）；② 检查 ESP-01S 供电正常（PA12 控制 AO3401 P-MOS 为 ESP 供电）。

**Q: 热点连上了但网页 404？**
A: 重新编译烧录最新 Bootloader（旧版 `+IPD` 解析有 bug，已修复）。

**Q: OTA 下载卡在 10% 不动的？**
A: 旧版 Bootloader 的 HTTP 响应缺少 `Content-Length` 头，导致浏览器等待连接关闭。已修复，重新编译烧录即可。

**Q: 烧录后反复重启？**
A: 检查 SPI/延时循环中是否喂狗。确认 `Watchdog_Kick()` 在长循环中调用（如 `Delay_ms` 内部每 131072 次迭代喂一次）。

**Q: TFT 屏幕不亮？**
A: 确认背光引脚（PB0）配置为推挽输出并置高。若硬件上背光 MOS 已拆除，需将背光 LED 负极直接接地或通过电阻接 3.3V。

## 更新日志

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