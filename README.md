# Windows Hardware Toolbox

Native Windows hardware management and diagnostics toolbox.

Light, native, fast-starting, low-footprint, module hardware toolbox: system info, device enumeration,
monitoring, diagnostics and management for CPU / GPU / Memory / Storage / USB / Camera / Network / Audio /
Display / Battery / Sensors.

## Stack

C++20 / CMake / Win32 / COM / WMI / SetupAPI / PDH / DXGI / D3D11 / Dear ImGui / spdlog / TOML.

## Status (v0.1.0)

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
| UI: 仪表盘/CPU/GPU/内存/存储/网络/USB/设备/音频/显示/摄像头/诊断/传感器/关于 (中文) | Done |
| CJK font (微软雅黑, scaled up) | Done |
| Crash handler (unhandled exception logging) | Done |
| Unit tests (config, metric, utf, vendor) | Done |

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
hardware/        providers: cpu, gpu, memory, wmi
monitoring/      Metric type (name/value/unit/source/availability)
services/        HardwareService (worker thread + cache)
ui/              pages, theme
tests/           unit tests
tools/           dependency fetching
docs/            architecture notes
```

## Roadmap

- P0: SMART/NVMe health details, battery, sensors
- P1: camera (Media Foundation), audio, display (EDID), driver manager
- P2: diagnostics expansion, SMART raw data
- P3: virtual camera, benchmarks
- P4: kernel-mode experiments (isolated, signed, VM-tested)
