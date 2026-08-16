# 启明星耗材烘干箱固件（STM32F103C8T6）

基于 STM32F103C8T6 + ESP-01S 的烘干箱固件，含 **Bootloader（OTA 无线升级）** 与 **App（烘干控制）** 两部分。

## 项目特性

- 🔥 **PTC 烘干控制**：PTC 加热 + 温度控制 + 过温保护 + PID 自整定
- 📡 **OTA 无线升级**：ESP-01S AP 模式 + 网页上传固件，无需数据线
- 🖥️ **TFT 屏幕**：1.14 寸 ST7789，显示升级状态/AP 信息/进度条
- 💾 **外部 Flash（W25Q128）**：固件暂存、参数持久化
- 🎛️ **多传感器**：温湿度（AHT20）、称重（CS1237）、NTC 温度检测
- ⚙️ **电机控制**：步进电机（连续/摆动模式）

## 目录结构

```
├── app/          App 应用（烘干控制、UI、WiFi 配置、Web 管理）
├── board/        板级抽象（引脚定义、看门狗、硬件初始化）
├── bootloader/   Bootloader（ESP01S AP 模式 OTA 升级）
├── bsp/          外设驱动（SPI/Flash/USART/TFT/传感器等）
├── module/       App 业务模块（OTA、WiFi、Web 服务器、系统时间等）
├── shared/       平台契约（Flash 分区地址、共享协议）
├── libraries/    标准外设库 + CMSIS
└── project/MDK(V5)/  EIDE/Keil 工程文件
```

## Flash 分区布局

| 区间 | 地址 | 大小 | 说明 |
|---|---|---|---|
| Bootloader | `0x08000000` | 18 KB | OTA 引导 + AP 升级入口（当前 ~16 KB） |
| 升级标志 | `0x08004800` | 1 KB | 升级请求标志 |
| WiFi 配置 | `0x08004C00` | 1 KB | WiFi 参数存储 |
| App | `0x08005000` | 44 KB | 烘干控制固件 |

## 编译

工程使用 **EIDE（Embedded IDE for VSCode）** 管理，编译器为 Keil ARMCC5（AC5）：

1. 用 VSCode 打开 `project/MDK(V5)` 目录（EIDE 扩展自动识别 `eide.yml`）
2. 构建两个目标：
   - **Bootloader**：`Ctrl+Shift+B` → `Build Bootloader`
   - **App**：`Ctrl+Shift+B` → `Build APP`
3. 产物：
   - `Objects/Bootloader/dryer_bootloader.bin`
   - `Objects/APP/dryer_app.bin`

> Bootloader 目标带 `BOOTLOADER_BUILD` 宏；App 目标不带。两目标 `main` 入口互不冲突。

## 烧录

使用 PW Link / ST-Link 烧录到对应地址：

| 文件 | 地址 |
|---|---|
| `dryer_bootloader.bin` | `0x08000000` |
| `dryer_app.bin` | `0x08005000` |

- Bootloader 大小 ≤ 18 KB（当前 ~16 KB）
- 两文件地址不重叠（Bootloader 到 `0x08004FFF`，标志区从 `0x08004800` 开始）

## OTA 升级流程

### 进入升级模式

Bootloader 每次上电：
1. 读取升级标志
2. **无有效 App** 或 **标志为 DOWNLOADED** → 进入升级模式（开 AP）
3. 有有效 App 且无升级请求 → 跳转 App

两种方式进入升级模式：
- **首次烧录**（只有 Bootloader）→ 自动进入升级模式
- **App 内升级**：Web 管理页面点击"固件升级"→ 写入标志并复位

### 连接并上传固件

**阶段 1 — 下载到外部 Flash**
1. 手机/电脑搜索 WiFi **`QiMingXing`**（密码 **`12345678`**）
2. 连接后浏览器访问 **`http://192.168.4.1`**
3. 选择 `.bin` 固件文件，点击 **UPLOAD & UPDATE**
4. 固件下载到外部 Flash（W25Q128），屏幕显示进度
5. 校验向量表后写入升级标志，自动重启

**阶段 2 — 拷贝到单片机**
6. 重启后 Bootloader 检测到升级标志，显示 "WRITE TO MCU" + 进度
7. 从外部 Flash 刷入 App 分区并校验向量表
8. 清标志并重启，跳转新 App

> 热点约数秒后出现，连接后访问 `192.168.4.1` 进入上传页面。上传过程无需人工干预，进度条实时显示。

## 代码入口

| 文件 | 作用 |
|---|---|
| `bootloader/bl_main.c` | Bootloader 主流程：开机判定 → 升级模式 / 跳转 App |
| `bootloader/bl_esp01s.c` | ESP-01S 通信：AT 命令、AP 配置、网页上传解析 |
| `bootloader/bl_tft.c` | Bootloader 屏幕驱动（ST7789 精简版） |
| `app/main.c` | App 主循环：初始化、传感器采集、控制逻辑 |
| `board/pin_config.h` | **全部引脚定义**（修改硬件需同步） |
| `shared/platform_contract.h` | Flash 分区地址等平台常量 |

## 自定义配置

**WiFi 热点**：编辑 `bootloader/bl_esp01s.c` 中 `BL_ESP01S_StartAP()`：
```c
ESP_SendCmd("AT+CWSAP=\"QiMingXing\",\"12345678\",1,4", "OK", 800);
```

**引脚**：编辑 `board/pin_config.h`。

**Flash 分区**：编辑 `shared/platform_contract.h`，同步 `eide.yml` 的 `ro-base`。

## 常见问题

**Q: 编译报错 `main` 重复定义？**
A: 检查目标宏。Bootloader 需 `BOOTLOADER_BUILD` 宏，App 不需要。检查 `eide.yml` 的 excludeList。

**Q: 找不到热点？**
A: ① 确认只烧了 Bootloader 或升级标志为 DOWNLOADED；② 检查 ESP-01S 供电（PA12 控制 AO3401）。

**Q: 热点连上了但网页 404？**
A: 重新编译烧录最新 Bootloader（旧版 `+IPD` 解析有 bug）。

**Q: OTA 下载卡在 10%？**
A: 这是旧版 bug，已修复。重新编译 Bootloader 烧录即可。

**Q: 烧录后反复重启？**
A: 检查 SPI/延时是否喂狗。确认 `Watchdog_Kick()` 在长循环中调用。

## 更新日志

### 2026-08-16

#### 新增
- Bootloader OTA 升级链路：ESP01S AP + Web 网页上传固件（两阶段：下载到外部 Flash → 拷贝到 App 分区）
- 上传固件暂存 W25Q128 → 校验向量表 → 刷入 App 分区
- App 端 Web 页面"固件升级"按钮
- `bl_tft.c` 5×7 点阵字体，显示 AP 名称/密码/IP/进度
- 屏幕 1.14 寸 135×240 ST7789 横屏 UI

#### 修复
- **OTA 分块上传**：1KB 分块 POST + `Content-Length: 0` 响应，解决"卡 10%"
- **HTTP 解析**：`\r\n\r\n` 后进入 body，避免换行符写入固件
- **+IPD 帧残留**：`GET /done` 处理后帧状态被 `ESP_WaitResponse` 打乱，新增 `ipd_skip_pending` 丢弃剩余帧，防止吃掉下一请求
- **ESP 电源关断**：`EnterCopyMode`/`JumpToApp` 中初始化 ESP_EN 推挽输出置高，PA9 输出低电平克服 ESP 内部上拉回灌（2.6V 伪供电）
- **拷贝模式黑屏**：`EnterCopyMode` 先 `W25Q128_Init`（含 `Spi1Bus_Init`）再 `BL_TFT_Init`
- **超时处理**：10s 无数据时若 `fw_received == total_expected` 直接完成，不等 `/done`
- `verify_staged_image` 向量表偏移 0 误判"未找到"

#### 变更
- **移除旧 v1 Bootloader**（`boot_main.c`/`boot_updater.c`/`boot_jump.c`/`boot_recovery.c`/`boot_platform_stm32.c`），已由 V2 引导替代
- `eide.yml` APP 分区修正为 `0x08005000`/`0xB000`
- 上传页 JS 精简：移除浏览器进度条、"OK, restarting..."（省 ~400B）
- 清理 host 测试及 Makefile 中旧 v1 引用
- Keil 工程（uvprojx）同步至新分区配置