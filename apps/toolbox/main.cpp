#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "core/config/Config.h"
#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/VideoTransformPipeline.h"
#include "hardware/camera/VirtualCameraController.h"
#include "hardware/camera/VirtualCameraRegistrar.h"
#include "services/HardwareService.h"
#include "ui/UiApp.h"
#include "ui/themes/Theme.h"

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

using Microsoft::WRL::ComPtr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace htb_app {

static ComPtr<ID3D11Device> g_device;
static ComPtr<ID3D11DeviceContext> g_context;
static ComPtr<IDXGISwapChain1> g_swapChain;
static ComPtr<ID3D11RenderTargetView> g_rtv;
static bool g_deviceLost = false;
static bool g_swapChainOccluded = false;

bool createRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    return SUCCEEDED(g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_rtv));
}

void cleanupRenderTarget() {
    g_rtv.Reset();
}

bool createDeviceAndSwapChain(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
                                   static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &g_device, &featureLevel,
                                   &g_context);
    if (FAILED(hr) && (createFlags & D3D11_CREATE_DEVICE_DEBUG)) {
        HTB_WARN("[app] D3D11 debug layer unavailable; retrying without it");
        createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels,
                               static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &g_device, &featureLevel,
                               &g_context);
    }
    if (FAILED(hr)) {
        HTB_ERROR("[app] D3D11CreateDevice failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(g_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) return false;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) return false;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

    hr = factory->CreateSwapChainForHwnd(g_device.Get(), hwnd, &sd, nullptr, nullptr, &g_swapChain);
    if (FAILED(hr)) {
        HTB_ERROR("[app] CreateSwapChainForHwnd failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
    return createRenderTarget();
}

void cleanupDevice() {
    cleanupRenderTarget();
    g_swapChain.Reset();
    g_context.Reset();
    g_device.Reset();
}

void setDpiAware() {
    ImGui_ImplWin32_EnableDpiAwareness();
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 0;
    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && g_swapChain) {
                cleanupRenderTarget();
                g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                createRenderTarget();
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    if (!ep) return EXCEPTION_CONTINUE_SEARCH;
    char modulePath[MAX_PATH] = "unknown";
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(ep->ExceptionRecord->ExceptionAddress), &module)) {
        GetModuleFileNameA(module, modulePath, MAX_PATH);
    }
    HTB_CRITICAL("[app] Unhandled exception 0x{:08X} at {} in {}", 
                 static_cast<unsigned>(ep->ExceptionRecord->ExceptionCode),
                 static_cast<void*>(ep->ExceptionRecord->ExceptionAddress), modulePath);
    return EXCEPTION_CONTINUE_SEARCH;
}

int runApp(HINSTANCE instance, htb::HardwareService& service, const wchar_t* cmdLine) {
    setDpiAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"HardwareToolboxWindow";
    RegisterClassExW(&wc);

    const HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Windows Hardware Toolbox", WS_OVERLAPPEDWINDOW,
                                      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        HTB_ERROR("[app] CreateWindowExW failed: {}", GetLastError());
        return 1;
    }
    if (!createDeviceAndSwapChain(hwnd)) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    htb::applyTheme();

    {
        const float fontSize = 17.0f;
        ImFontConfig fontCfg{};
        fontCfg.FontNo = 0;
        ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", fontSize, &fontCfg,
                                                    io.Fonts->GetGlyphRangesChineseFull());
        if (!font) {
            HTB_WARN("[app] msyh.ttc unavailable; falling back to simhei.ttf");
            font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simhei.ttf", fontSize, nullptr,
                                                io.Fonts->GetGlyphRangesChineseFull());
        }
        if (!font) {
            HTB_WARN("[app] no CJK font loaded; text may render as boxes");
            io.Fonts->AddFontDefault();
        }
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());

    htb::UiApp uiApp(service, service.config(), g_device.Get(), g_context.Get());
    if (cmdLine && wcsstr(cmdLine, L"--page=camera")) {
        uiApp.setInitialPage("摄像头");
    }
    uiApp.setOnQuit([hwnd] { PostMessageW(hwnd, WM_CLOSE, 0, 0); });
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    const auto frameBudget =
        std::chrono::milliseconds(1000 / std::max(30, service.config().ui.fps));
    bool running = true;

    while (running) {
        const auto frameStart = std::chrono::steady_clock::now();

        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (g_deviceLost) {
            HTB_WARN("[app] D3D11 device lost; recreating");
            ImGui_ImplDX11_Shutdown();
            cleanupDevice();
            if (!createDeviceAndSwapChain(hwnd)) {
                HTB_ERROR("[app] Device recreation failed; exiting");
                break;
            }
            ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());
            g_deviceLost = false;
        }
        if (g_swapChainOccluded) {
            const HRESULT test = g_swapChain->Present(0, DXGI_PRESENT_TEST);
            g_swapChainOccluded = (test == DXGI_STATUS_OCCLUDED);
            if (g_swapChainOccluded) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        uiApp.frame();
        ImGui::Render();

        g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
        const float clearColor[4] = {0.06f, 0.07f, 0.08f, 1.0f};
        g_context->ClearRenderTargetView(g_rtv.Get(), clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        const HRESULT present = g_swapChain->Present(1, 0);
        g_swapChainOccluded = (present == DXGI_STATUS_OCCLUDED);
        if (present == DXGI_ERROR_DEVICE_REMOVED || present == DXGI_ERROR_DEVICE_RESET) {
            HTB_WARN("[app] Present failed: device removed/reset");
            g_deviceLost = true;
        }

        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        if (elapsed < frameBudget) {
            std::this_thread::sleep_for(frameBudget - elapsed);
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDevice();
    DestroyWindow(hwnd);
    return 0;
}

} // namespace htb_app

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int) {
    htb::Logger::init(cmdLine && wcsstr(cmdLine, L"--console") != nullptr);
    SetUnhandledExceptionFilter(&htb_app::crashHandler);

    if (cmdLine && (wcsstr(cmdLine, L"--register-vcamera") || wcsstr(cmdLine, L"--unregister-vcamera"))) {
        const bool unregister = wcsstr(cmdLine, L"--unregister-vcamera") != nullptr;
        std::wstring name = L"Hardware Toolbox 虚拟摄像头";
        const wchar_t* quote = wcsstr(cmdLine, L"\"");
        if (quote) {
            const wchar_t* end = wcschr(quote + 1, L'"');
            if (end) name.assign(quote + 1, end - quote - 1);
        }
        const long hr = unregister ? htb::VirtualCameraRegistrar::unregisterVirtualCamera(htb::toUtf8(name))
                                   : htb::VirtualCameraRegistrar::registerVirtualCamera(htb::toUtf8(name));
        return SUCCEEDED(hr) ? 0 : 1;
    }

    const bool autoRegister = cmdLine && wcsstr(cmdLine, L"--auto-register-vcamera") != nullptr;
    const bool autoUnregister = cmdLine && wcsstr(cmdLine, L"--auto-unregister-vcamera") != nullptr;

    if (cmdLine && wcsstr(cmdLine, L"--test-obs-output")) {
        htb::Logger::init(true);
        htb::CameraOutputParams params{};
        params.outputTarget = 0;
        htb::VideoTransformPipeline pipeline;
        pipeline.start(params);
        std::this_thread::sleep_for(std::chrono::seconds(5));
        pipeline.stop();
        const auto status = pipeline.status();
        HTB_INFO("[app] OBS output test: running={} capturing={} frames={} fps={:.1f} msg={}", status->running,
                 status->capturing, static_cast<unsigned long long>(status->framesSent), status->fps,
                 status->message);
        return 0;
    }

    htb::Config config = htb::Config::load();
    HTB_INFO("[app] Hardware Toolbox v{} starting", HTB_VERSION_STRING);
    if (cmdLine && wcsstr(cmdLine, L"--elevated")) {
        HTB_INFO("[app] running with administrator privileges");
    }

    htb::HardwareService service(std::move(config));
    if (autoRegister || autoUnregister) {
        std::wstring name = L"Hardware Toolbox 虚拟摄像头";
        const wchar_t* quote = wcsstr(cmdLine, L"\"");
        if (quote) {
            const wchar_t* end = wcschr(quote + 1, L'"');
            if (end) name.assign(quote + 1, end - quote - 1);
        }
        service.setPendingVcameraAction(autoUnregister ? htb::HardwareService::VcameraAction::Unregister
                                                       : htb::HardwareService::VcameraAction::Register,
                                        htb::toUtf8(name));
    }
    service.start();
    const int rc = htb_app::runApp(instance, service, cmdLine);
    service.stop();
    HTB_INFO("[app] Hardware Toolbox exiting with code {}", rc);
    return rc;
}
