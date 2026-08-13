# AGENTS.md

Long-term project: **Windows Hardware Toolbox** - a native Windows hardware management and diagnostics toolbox.
Read this file, `README.md`, and `docs/architecture.md` before working. Never duplicate existing functionality.

## Tech stack (mandatory)

- C++20, CMake, Windows SDK, Win32, COM/WMI, SetupAPI/CfgMgr32, PDH, DXGI/D3D11, Media Foundation (later)
- UI: Dear ImGui (Win32 + D3D11 backend)
- Logging: spdlog, Config: TOML (toml++), Tests: GoogleTest
- Forbidden without explicit justification: Electron, Chromium, Python/Node runtimes, web runtimes

## Architecture (strict layering)

    Windows API -> Provider (hardware/) -> Service (services/) -> UI (ui/)

- UI never calls Windows API directly. No `ImGui::Text("%d", GetSomething())` in UI code.
- Dependencies: `.deps/` (vendored, gitignored). Fetch with `tools/fetch_deps.ps1` if missing.

## Build

```text
cmake --preset msvc-debug
cmake --build --preset debug
ctest --preset debug
```

Binary: `build/msvc-debug/bin/toolbox.exe`. Run with `--console` for console logging.
Logs: `%LOCALAPPDATA%/HardwareToolbox/logs/toolbox.log`

## Conventions

- Every data point carries a `source` (WMI, DXGI, PDH, Registry...) and `Availability`
  (Available / Unavailable / Unsupported / PermissionDenied). Never render fake zeros for missing data
  (e.g. temperature must show "N/A", not "0 C").
- Hardware queries run only on worker threads (HardwareService). Never block the UI thread.
- Any single device/sensor read failure must not crash the app; wrap provider refreshes in try/catch.
- Optional data must use `std::optional<T>` or the Availability enum.
- Vendor-specific capabilities (NVAPI etc.) must be optional plugins, never hard dependencies.
- New modules: Provider -> Service -> Page, following existing patterns.
- Log via `HTB_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL` macros; include module context in the message.

## Git

- Commit style: `feat:`, `fix:`, `refactor:`, `perf:`, `docs:`, `test:`, `build:`, `ci:`
- e.g. `feat(cpu): add PDH-based usage monitoring`
- No large refactors / tech stack changes / API redesigns without explaining Why/Impact/Alternative/Migration.
- No admin-elevated operations, kernel drivers, or system modifications without explicit confirmation.

## Status

v0.2.0: logging, config, worker-thread service, CPU/GPU/Memory/Device/Storage/Network/Audio/Battery/
Display/Camera providers, 中文 UI (14 pages: 仪表盘/CPU/GPU/内存/存储/网络/USB/设备/音频/显示/摄像头/诊断/传感器/关于),
CJK font, gateway ping.
Next: Sensors (vendor SDK plugins), SMART raw data, camera preview (Media Foundation pipeline), EDID details.
