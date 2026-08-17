# Windows Hardware Toolbox

Native Windows hardware management and diagnostics toolbox.

Light, native, fast-starting, low-footprint, module hardware toolbox: system info, device enumeration,
monitoring, diagnostics and management for CPU / GPU / Memory / Storage / USB / Camera / Network / Audio /
Display / Battery / Sensors.

## Stack

C++20 / CMake / Win32 / COM / WMI / SetupAPI / PDH / DXGI / D3D11 / Dear ImGui / spdlog / TOML.

## Status (v0.2.0)

| Area | Status |
| --- | --- |
| Project skeleton (CMake + presets) | Done |
| Logging (spdlog, rotating file) | Done |
| Config (TOML, defaults + validation) | Done |
| HardwareService (worker thread, cached snapshots, async refresh) | Done |
| CPU provider (registry + PDH usage) | Done |
| GPU provider (DXGI enumeration + memory usage + driver info) | Done |
| Memory provider (Win32 API + WMI DIMM) | Done |
| Device enumeration (SetupAPI + CfgMgr32, parent/child) | Done |
| Storage provider (WMI disks + health/temperature) | Done |
| Network provider (IP Helper: addresses, link speed, live rates, gateway ping) | Done |
| Audio provider (MMDevice endpoints: state, format, default) | Done |
| Battery provider (WMI) + dashboard tiles | Done |
| Display provider (EnumDisplayDevices: resolution, refresh, monitor) | Done |
| Camera enumeration (Media Foundation) | Done |
| **Process provider (NT API 快照: PID/PPID/CPU% (差值采样)/内存/IO 速率/线程/句柄/会话 + 控制: 结束/结束进程树/暂停/恢复/优先级/CPU 亲和性/重启/详细信息)** | Done |
| **Service provider (SCM: 枚举/启停/重启/描述/路径)** | Done |
| **Startup provider (注册表 Run + 启动文件夹: 枚举/启用/禁用/删除/打开位置)** | Done |
| UI: 仪表盘/CPU/GPU/内存/存储/网络/USB/设备/音频/显示/摄像头/进程/服务/启动项/诊断/传感器/关于 (中文) | Done |
| CJK font (微软雅黑, scaled up) | Done |
| Crash handler (unhandled exception logging) | Done |
| Unit tests (config, metric, utf, vendor, process) | Done |

## Build

Requirements: Visual Studio 2022/2026 Build Tools (MSVC, C++ workload), CMake >= 3.25, Git.

```text
powershell -ExecutionPolicy Bypass -File tools/fetch_deps.ps1
cmake --preset msvc-debug
cmake --build --preset debug
ctest --preset debug
build\msvc-debug\bin\toolbox.exe
```

On machines where the VS instance is not auto-detected by CMake (e.g. relocated Build Tools
without installer COM registration), configure through the toolchain wrapper:

```text
tools\msvc_env.bat cmake --preset ninja-msvc-debug
tools\msvc_env.bat cmake --build --preset debug
```

Run with `--console` to attach console output. Logs: `%LOCALAPPDATA%/HardwareToolbox/logs/`.
Config: `%LOCALAPPDATA%/HardwareToolbox/config.toml`.

## Layout

```text
apps/toolbox/    application entry (Win32 + D3D11 + ImGui loop)
core/            logging, config, runtime, utilities
hardware/        providers: cpu, gpu, memory, wmi, process, systemservice, startup
monitoring/      Metric type (name/value/unit/source/availability)
services/        HardwareService (worker thread + cache)
ui/              pages, theme
tests/           unit tests
tools/           dependency fetching
docs/            architecture notes
```

## 任务管理器 (进程 / 服务 / 启动项)

- **进程页**: NtQuerySystemInformation 一次性获取进程表 (PID/PPID/CPU 时间/线程/句柄/会话),
  配合 `GetProcessMemoryInfo` / `GetProcessIoCounters` 计算内存与磁盘 IO 速率;
  CPU% 通过两次采样差值归一化 (总和约等于系统总占用)。支持排序、右键菜单
  (结束任务/结束进程树/暂停/恢复/重启/设置优先级/CPU 亲和性/获取详细信息/复制信息),
  以及可选的命令行/可执行路径/用户名详情查看。
- **服务页**: SCM 枚举服务 (状态/启动类型/PID/路径/描述, 配置按 60s 缓存), 支持
  启动/停止/重启 (带确认), 状态筛选与搜索。
- **启动项页**: 枚举 HKCU/HKLM Run 与 Policies\Explorer\Run 注册表启动项及用户/公共
  启动文件夹; 启用/禁用通过可逆的重命名实现 (注册表值加后缀 / 文件名加 `.disabled`),
  支持删除与打开位置。

## Roadmap

- P0: SMART/NVMe health details, battery, sensors
- P1: camera (Media Foundation), audio, display (EDID), driver manager
- P2: diagnostics expansion, SMART raw data
- P3: virtual camera, benchmarks
- P4: kernel-mode experiments (isolated, signed, VM-tested)
