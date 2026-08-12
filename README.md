# Windows Hardware Toolbox

Native Windows hardware management and diagnostics toolbox.

Light, native, fast-starting, low-footprint, module hardware toolbox: system info, device enumeration,
monitoring, diagnostics and management for CPU / GPU / Memory / Storage / USB / Camera / Network / Audio /
Display / Battery / Sensors.

## Stack

C++20 / CMake / Win32 / COM / WMI / SetupAPI / PDH / DXGI / D3D11 / Dear ImGui / spdlog / TOML.

## Status (v0.1.0 skeleton)

| Area | Status |
| --- | --- |
| Project skeleton (CMake + presets) | Done |
| Logging (spdlog, rotating file) | Done |
| Config (TOML, defaults + validation) | Done |
| HardwareService (worker thread, cached snapshots, async refresh) | Done |
| CPU provider (registry + PDH usage) | Done |
| GPU provider (DXGI enumeration + memory usage + driver info) | Done |
| Memory provider (Win32 API + WMI DIMM) | Done |
| UI: Dashboard / CPU / GPU / Memory pages, dark theme | Done |
| Crash handler (unhandled exception logging) | Done |
| Unit tests (config, metric, utf, vendor) | Done |

## Build

Requirements: Visual Studio 2022 Build Tools (MSVC, C++ workload), CMake >= 3.25, Git.

```text
powershell -ExecutionPolicy Bypass -File tools/fetch_deps.ps1
cmake --preset msvc-debug
cmake --build --preset debug
ctest --preset debug
build\msvc-debug\bin\toolbox.exe
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

- P0: storage (SMART/NVMe), device enumeration (SetupAPI), dashboard expansion
- P1: USB, network, display, audio, camera (Media Foundation), driver info
- P2: sensors, diagnostics, SMART/NVMe health
- P3: virtual camera, benchmarks
- P4: kernel-mode experiments (isolated, signed, VM-tested)
