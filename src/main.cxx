#include "ui/egui.hxx"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <tchar.h>
#include "ui/layout.hxx"
#include "ui/glass.hxx"
#include "ui/preview.hxx"
#include "ui/state.hxx"

static void enable_dpi_awareness()
{
    if (HMODULE user32 = ::GetModuleHandleW(L"user32.dll")) {
        using SetCtx = BOOL (WINAPI*)(HANDLE);
        auto fn = reinterpret_cast<SetCtx>(::GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (fn && fn(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4))))
            return;
    }
}

static RECT virtual_screen()
{
    RECT r;
    r.left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    r.top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.right = r.left + ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.bottom = r.top + ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (r.right <= r.left || r.bottom <= r.top) {
        r.left = 0;
        r.top = 0;
        r.right = ::GetSystemMetrics(SM_CXSCREEN);
        r.bottom = ::GetSystemMetrics(SM_CYSCREEN);
    }
    return r;
}

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static void apply_streamproof(HWND hwnd)
{
    static int applied = -1;
    const int want = vars.gui.streamproof ? 1 : 0;
    if (applied == want)
        return;
    const DWORD affinity = want ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    if (::SetWindowDisplayAffinity(hwnd, affinity))
        applied = want;
}

static bool cursor_over_ui(HWND hwnd)
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx)
        return false;

    POINT pt;
    if (!::GetCursorPos(&pt))
        return false;
    ::ScreenToClient(hwnd, &pt);
    const ImVec2 mouse((float)pt.x, (float)pt.y);
    const bool menu_open = egui::settings.state.menu_open;

    for (int i = 0; i < ctx->Windows.Size; ++i) {
        ImGuiWindow* w = ctx->Windows[i];
        if (!w->Active || w->Hidden || w->IsFallbackWindow)
            continue;
        if (w->Flags & ImGuiWindowFlags_ChildWindow)
            continue;
        if (!menu_open
            && strcmp(w->Name, "##egui_watermark") != 0
            && strcmp(w->Name, "##egui_watching") != 0
            && strcmp(w->Name, "##egui_hotkeys") != 0)
            continue;
        const ImRect& r = w->OuterRectClipped;
        if (r.GetWidth() < 1.0f || r.GetHeight() < 1.0f)
            continue;
        if (r.Contains(mouse))
            return true;
    }
    return false;
}

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void set_clickthrough(HWND hwnd, bool through)
{
    LONG_PTR ex = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR want = through ? (ex | WS_EX_LAYERED | WS_EX_TRANSPARENT)
                                  : ((ex | WS_EX_LAYERED) & ~WS_EX_TRANSPARENT);
    if (want == ex)
        return;
    ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, want);
    ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
}

int main(int, char**)
{
    enable_dpi_awareness();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"FF0L", nullptr };
    ::RegisterClassExW(&wc);
    const RECT screen = virtual_screen();
    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        wc.lpszClassName, L"FF0L", WS_POPUP,
        screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    ::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    const MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    if (!CreateDeviceD3D(hwnd))
    {
        ::MessageBoxW(nullptr, L"The overlay swap chain could not be created.", L"FF0L", MB_OK | MB_ICONERROR);
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    egui::settings.dpi_scale = egui::system_dpi_scale();

    egui::initialize();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    bool done = false;
    while (!done)
    {
        static bool end_down = false;
        const bool end_now = (::GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (end_now && !end_down)
            ::PostQuitMessage(0);
        end_down = end_now;

        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
            egui::glass_invalidate();
        }

        apply_streamproof(hwnd);

        egui::update_dpi();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        egui::menu();
        egui::watermark();

        egui::keybind_list_begin();
        egui::keybind_list_end();

        egui::spectator_list_begin();
        egui::spectator_list_add("Player 1");
        egui::spectator_list_add("Player 2");
        egui::spectator_list_add("Player 3");
        egui::spectator_list_end();

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        set_clickthrough(hwnd, !cursor_over_ui(hwnd));

        g_pSwapChain->Present(1, 0);
    }

    egui::glass_shutdown();
    egui::g_model3d.shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
                                               &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                           featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
                                           &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr))
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DISPLAYCHANGE:
        {
            const RECT screen = virtual_screen();
            ::SetWindowPos(hWnd, HWND_TOPMOST, screen.left, screen.top,
                           screen.right - screen.left, screen.bottom - screen.top,
                           SWP_NOACTIVATE);
        }
        return 0;
    case WM_CLOSE:
        ::DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
