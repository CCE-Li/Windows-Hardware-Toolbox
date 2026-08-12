# Architecture

## Layering

```text
+--------------------------------------------------------------+
| UI (ui/)            pages, widgets, theme                    |
+--------------------------------------------------------------+
| Application (apps/toolbox)   window, D3D11, ImGui loop       |
+--------------------------------------------------------------+
| Service (services/)          HardwareService                 |
|                              - worker thread                 |
|                              - cached snapshots (atomic ptr) |
+--------------------------------------------------------------+
| Hardware (hardware/)         providers                       |
|                              cpu, gpu, memory, wmi           |
+--------------------------------------------------------------+
| Core (core/)                 logging, config, runtime, util  |
+--------------------------------------------------------------+
| Windows Platform             Win32, WMI, DXGI, PDH, Registry |
+--------------------------------------------------------------+
```

Rule: UI must not call Windows APIs. Data flow:

```text
Windows API -> Provider -> snapshot (shared_ptr<const T>) -> Service cache -> Page -> ImGui
```

## Data model

- `Metric` (monitoring/Metric.h): name, value, unit, timestamp, `Availability`, source.
- `Availability`: Available / Unavailable / Unsupported / PermissionDenied.
- Providers expose immutable snapshots (`std::shared_ptr<const T>`) swapped atomically; UI reads copies.

## Threading

- `HardwareService::loop()` runs on a jthread: refreshes every provider, catches all exceptions,
  stores `lastRefresh`, waits on a condition variable for the configured interval.
- WMI/PDH/registry/DXGI queries never run on the UI thread.
- Provider refresh order is sequential in the worker; per-module parallel workers are a later optimization.

## Adding a module (e.g. Storage)

1. `hardware/storage/StorageProvider.h/.cpp`: struct `StorageInfo` + provider implementing `HardwareProvider`.
2. Register in `HardwareService` (unique_ptr member + refresh in `loop()`).
3. `ui/pages/StoragePage.*`: implement `IPage::draw(UiContext&)`.
4. Register the page in `UiApp` and add a sidebar entry.
5. Add unit tests that do not depend on hardware presence.

## Failure policy

- Optional sensors: mark Unsupported; missing permissions: PermissionDenied; query errors: Unavailable.
- Never crash on a missing device; never display fabricated zeros.
