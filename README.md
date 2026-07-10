# WT32-ETH01 HomeKit WOL

这个工程把 WT32-ETH01 做成 Apple Home 里的“电脑开关”：

- 开：通过 WT32-ETH01 的 RJ45 口向电脑有线网卡发送 Wake-on-LAN Magic Packet。
- 关：向电脑上的本地 HTTP helper 发请求，由 Windows 执行关机。
- 状态：通过 helper 的 `/status` 接口同步在线状态。

## 硬件连接

推荐拓扑：

```text
HomePod / Apple TV
        |
Apple Home
        |
      Wi-Fi
        |
   WT32-ETH01
        |
      网线
        |
      电脑
```

WT32-ETH01 的 RJ45 直连电脑网口时，请给电脑有线网卡设置静态地址：

- IP: `192.168.0.201`
- 子网掩码: `255.255.255.0`
- 网关: `192.168.0.1`

WT32-ETH01 默认使用：

- IP: `192.168.0.7`
- 子网掩码: `255.255.255.0`
- 网关: `192.168.0.1`

文档里也提醒：直连电脑时不要给模块以太网口使用 DHCP，因为普通电脑不会给它分配 IP。

## 修改配置

编辑 `include/config.h`：

- `PC_MAC_ADDRESS`: 电脑有线网卡 MAC 地址，必须改。
- `PC_IP_ADDRESS`: 电脑有线网卡静态 IP。
- `SHUTDOWN_URL` 和 `STATUS_URL`: 电脑 helper 地址。
- `HOMEKIT_PAIRING_CODE`: HomeKit 配对码，HomeSpan 代码里必须写成 8 位数字，例如 `11122333`；添加到 Apple Home 时显示为 `111-22-333`。

## 编译和烧录

安装 PlatformIO 后，在工程根目录运行：

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

如果烧录时报 `Failed to connect to ESP32: No serial data received`，先确认接的是 WT32-ETH01 的调试烧录接口 `TXD0(IO1)` / `RXD0(IO3)`，并把 `IO0` 接地后复位进入下载模式。工程默认烧录速度已设为 `115200`，更适合 USB-TTL 转接板排查。

本工程在 PlatformIO `espressif32` 的 Arduino-ESP32 `2.0.x` 核心下使用 HomeSpan `1.9.1`。不要升级到 HomeSpan `2.x`，除非你的 Arduino-ESP32 核心也升级到 `3.x`，否则会出现 `THIS VERSION OF HOMESPAN REQUIRES VERSION 3.x` 的编译错误。

HomeKit 固件体积较大，工程已使用 `huge_app.csv` 分区表，牺牲 OTA 分区来获得更大的可烧录 APP 空间。

WT32-ETH01 烧录接口来自规格书：

- `TXD`: IO1 / TXD0
- `RXD`: IO3 / RXD0
- `IO0`: 下载模式
- `EN1`: 复位/使能，高电平有效
- `3V3` 或 `5V`: 二选一供电，不能同时接
- `GND`: 地

烧录时通常需要让 `IO0` 接地后复位进入下载模式；烧录完成后断开 `IO0` 到地并复位运行。

## HomeKit 配网

首次启动后，HomeSpan 会在串口里输出配网/配对信息。按 HomeSpan 提示给设备配置家庭 Wi-Fi，然后在 Apple Home 中添加配件，使用 `include/config.h` 里的配对码。

如果需要清除 HomeKit 配对数据，在串口监视器里按 HomeSpan 的 CLI 提示执行重置命令。

## Windows 关机 Helper

在电脑上以管理员 PowerShell 运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows-shutdown-server.ps1
```

测试时可加 `-NoShutdown`，这样访问 `/shutdown` 不会真的关机：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows-shutdown-server.ps1 -NoShutdown
```

Windows 防火墙需要允许 TCP `8765` 入站访问。确认浏览器能打开：

```text
http://192.168.0.201:8765/status
```

## 电脑 WOL 设置

BIOS/UEFI 中启用：

- Wake on LAN
- PCIe Wake Up
- ErP Disabled

Windows 网卡中启用：

- 允许此设备唤醒计算机
- 仅允许魔术封包唤醒计算机

## 重要限制

这个工程假设电脑通过自己的 Wi-Fi 或其它网络方式接入家庭局域网，WT32-ETH01 的网线只负责唤醒电脑。如果电脑只能靠这一根网线上网，需要额外路由/桥接方案，不建议直接让 ESP32 做长期稳定网桥。
