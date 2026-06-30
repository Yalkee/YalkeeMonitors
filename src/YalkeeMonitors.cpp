#define WIN32_LEAN_AND_MEAN
#define STB_IMAGE_IMPLEMENTATION
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <codecvt>
#include <sstream>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "resource.h"
#include "stb_image.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

#pragma warning(disable: 28251)
#pragma warning(disable: 26495)

#ifndef LPPHYSICAL_MONITOR
typedef struct _PHYSICAL_MONITOR {
    HANDLE hPhysicalMonitor;
    WCHAR  szPhysicalMonitorDescription[128];
} PHYSICAL_MONITOR, * LPPHYSICAL_MONITOR;
#endif

typedef enum _MC_VCP_CODE_TYPE {
    MC_MOMENTARY,
    MC_SET_PARAMETER
} MC_VCP_CODE_TYPE, * LPMC_VCP_CODE_TYPE;

struct MonitorInfo {
    HMONITOR hMonitor = NULL;
    HANDLE   hPhysicalMonitor = NULL;
    std::wstring displayName;
    bool     valuesRead = false;
    DWORD    minBrightness = 0, curBrightness = 0, maxBrightness = 0;
    DWORD    minContrast = 0, curContrast = 0, maxContrast = 0;
    bool     supportsBrightness = false;
    bool     supportsContrast = false;
    int      monitorNumber = 0;

    std::string regId;

    int      sliderRed = 100;
    int      sliderGreen = 100;
    int      sliderBlue = 100;
};

std::vector<MonitorInfo> monitors;
int selectedMonitor = -1;

typedef BOOL(WINAPI* PFN_GET_NUMBER_OF_PHYSICAL_MONITORS)(HMONITOR, LPDWORD);
typedef BOOL(WINAPI* PFN_GET_PHYSICAL_MONITORS)(HMONITOR, DWORD, LPPHYSICAL_MONITOR);
typedef BOOL(WINAPI* PFN_GET_MONITOR_BRIGHTNESS)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
typedef BOOL(WINAPI* PFN_SET_MONITOR_BRIGHTNESS)(HANDLE, DWORD);
typedef BOOL(WINAPI* PFN_GET_MONITOR_CONTRAST)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
typedef BOOL(WINAPI* PFN_SET_MONITOR_CONTRAST)(HANDLE, DWORD);
typedef BOOL(WINAPI* PFN_DESTROY_PHYSICAL_MONITOR)(HANDLE);
typedef BOOL(WINAPI* PFN_GET_VCP_FEATURE)(HANDLE, BYTE, LPMC_VCP_CODE_TYPE, LPDWORD, LPDWORD);
typedef BOOL(WINAPI* PFN_SET_VCP_FEATURE)(HANDLE, BYTE, DWORD);

PFN_GET_NUMBER_OF_PHYSICAL_MONITORS pGetNumberOfPhysicalMonitors = nullptr;
PFN_GET_PHYSICAL_MONITORS           pGetPhysicalMonitors = nullptr;
PFN_GET_MONITOR_BRIGHTNESS          pGetMonitorBrightness = nullptr;
PFN_SET_MONITOR_BRIGHTNESS          pSetMonitorBrightness = nullptr;
PFN_GET_MONITOR_CONTRAST            pGetMonitorContrast = nullptr;
PFN_SET_MONITOR_CONTRAST            pSetMonitorContrast = nullptr;
PFN_DESTROY_PHYSICAL_MONITOR        pDestroyPhysicalMonitor = nullptr;
PFN_GET_VCP_FEATURE                 pGetVCPFeature = nullptr;
PFN_SET_VCP_FEATURE                 pSetVCPFeature = nullptr;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

void DebugLog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}

std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

const char* REG_PATH = "Software\\YalkeeMonitors";

void LoadMonitorRgb(const std::string& monitorId, int& red, int& green, int& blue) {
    std::string subKey = std::string(REG_PATH) + "\\Monitors\\" + monitorId;
    HKEY hKey;
    red = green = blue = 100;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val, size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "Red", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) red = val;
        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "Green", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) green = val;
        size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "Blue", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) blue = val;
        RegCloseKey(hKey);
    }
    if (red < 0 || red > 100) red = 100;
    if (green < 0 || green > 100) green = 100;
    if (blue < 0 || blue > 100) blue = 100;
}

void SaveMonitorRgb(const std::string& monitorId, int red, int green, int blue) {
    std::string subKey = std::string(REG_PATH) + "\\Monitors\\" + monitorId;
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, subKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = red;
        RegSetValueExA(hKey, "Red", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        val = green;
        RegSetValueExA(hKey, "Green", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        val = blue;
        RegSetValueExA(hKey, "Blue", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

void DeleteMonitorRgb() {
    std::string subKey = std::string(REG_PATH) + "\\Monitors";
    RegDeleteKeyA(HKEY_CURRENT_USER, subKey.c_str());
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0, size = sizeof(DWORD);
        RegQueryValueExA(hKey, "AutoStart", NULL, NULL, (LPBYTE)&val, &size);
        RegCloseKey(hKey);
        return val != 0;
    }
    return false;
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = enable ? 1 : 0;
        RegSetValueExA(hKey, "AutoStart", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    wchar_t startupPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        std::wstring shortcut = std::wstring(startupPath) + L"\\YalkeeMonitors.lnk";
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);

            CoInitialize(NULL);
            IShellLinkW* pShellLink = nullptr;
            IPersistFile* pPersistFile = nullptr;

            HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                IID_IShellLinkW, (void**)&pShellLink);
            if (SUCCEEDED(hr)) {
                pShellLink->SetPath(exePath);
                pShellLink->SetArguments(L"--silent");
                pShellLink->SetDescription(L"Yalkee Monitors Gamma Control");

                hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
                if (SUCCEEDED(hr)) {
                    pPersistFile->Save(shortcut.c_str(), TRUE);
                    pPersistFile->Release();
                }
                pShellLink->Release();
            }
            CoUninitialize();
        }
        else {
            DeleteFileW(shortcut.c_str());
        }
    }
}

BOOL LoadDxva2Functions() {
    HMODULE hDxva2 = LoadLibraryW(L"Dxva2.dll");
    if (!hDxva2) return FALSE;

    pGetNumberOfPhysicalMonitors = (PFN_GET_NUMBER_OF_PHYSICAL_MONITORS)GetProcAddress(hDxva2, "GetNumberOfPhysicalMonitorsFromHMONITOR");
    pGetPhysicalMonitors = (PFN_GET_PHYSICAL_MONITORS)GetProcAddress(hDxva2, "GetPhysicalMonitorsFromHMONITOR");
    pGetMonitorBrightness = (PFN_GET_MONITOR_BRIGHTNESS)GetProcAddress(hDxva2, "GetMonitorBrightness");
    pSetMonitorBrightness = (PFN_SET_MONITOR_BRIGHTNESS)GetProcAddress(hDxva2, "SetMonitorBrightness");
    pGetMonitorContrast = (PFN_GET_MONITOR_CONTRAST)GetProcAddress(hDxva2, "GetMonitorContrast");
    pSetMonitorContrast = (PFN_SET_MONITOR_CONTRAST)GetProcAddress(hDxva2, "SetMonitorContrast");
    pDestroyPhysicalMonitor = (PFN_DESTROY_PHYSICAL_MONITOR)GetProcAddress(hDxva2, "DestroyPhysicalMonitor");
    pGetVCPFeature = (PFN_GET_VCP_FEATURE)GetProcAddress(hDxva2, "GetVCPFeatureAndVCPFeatureReply");
    pSetVCPFeature = (PFN_SET_VCP_FEATURE)GetProcAddress(hDxva2, "SetVCPFeature");

    return (pGetNumberOfPhysicalMonitors && pGetPhysicalMonitors && pGetMonitorBrightness && pSetMonitorBrightness);
}

std::wstring GetMonitorFriendlyName(HMONITOR hMonitor) {
    std::wstring name;
    MONITORINFOEXW mix;
    mix.cbSize = sizeof(MONITORINFOEXW);
    if (GetMonitorInfoW(hMonitor, (LPMONITORINFO)&mix)) {
        DISPLAY_DEVICEW dd = { sizeof(dd) };
        for (int i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); ++i) {
            if (_wcsicmp(dd.DeviceName, mix.szDevice) == 0) {
                DISPLAY_DEVICEW mon = { sizeof(mon) };
                if (EnumDisplayDevicesW(dd.DeviceName, 0, &mon, 0)) {
                    name = mon.DeviceString;
                    break;
                }
            }
        }
    }
    return name;
}

HDC GetMonitorDC(HMONITOR hMonitor) {
    MONITORINFOEXW mix;
    mix.cbSize = sizeof(mix);
    if (GetMonitorInfoW(hMonitor, (LPMONITORINFO)&mix)) {
        return CreateDCW(L"DISPLAY", mix.szDevice, NULL, NULL);
    }
    return NULL;
}

void ApplyGamma(const MonitorInfo& info) {
    HDC hdc = GetMonitorDC(info.hMonitor);
    if (!hdc) return;

    int effectiveRed = 50 + (info.sliderRed * 50) / 100;
    int effectiveGreen = 50 + (info.sliderGreen * 50) / 100;
    int effectiveBlue = 50 + (info.sliderBlue * 50) / 100;

    WORD ramp[3][256];
    for (int i = 0; i < 256; i++) {
        DWORD val = (i * 65535) / 255;
        ramp[0][i] = (WORD)((val * effectiveRed) / 100);
        ramp[1][i] = (WORD)((val * effectiveGreen) / 100);
        ramp[2][i] = (WORD)((val * effectiveBlue) / 100);
    }
    SetDeviceGammaRamp(hdc, ramp);
    DeleteDC(hdc);
}

void ResetAllGammas() {
    for (auto& m : monitors) {
        HDC hdc = GetMonitorDC(m.hMonitor);
        if (hdc) {
            WORD ramp[3][256];
            for (int i = 0; i < 256; i++) {
                DWORD val = (i * 65535) / 255;
                ramp[0][i] = ramp[1][i] = ramp[2][i] = (WORD)val;
            }
            SetDeviceGammaRamp(hdc, ramp);
            DeleteDC(hdc);
        }
    }
}

void EnumerateMonitors() {
    for (auto& mon : monitors) {
        if (mon.hPhysicalMonitor && pDestroyPhysicalMonitor)
            pDestroyPhysicalMonitor(mon.hPhysicalMonitor);
    }
    monitors.clear();
    selectedMonitor = -1;

    struct EnumParams {
        std::vector<MonitorInfo>* monVec;
    } params;
    params.monVec = &monitors;

    EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
        EnumParams* p = (EnumParams*)lParam;
        auto& monitors = *p->monVec;

        HANDLE hPhys = NULL;
        WCHAR desc[128] = L"";

        PHYSICAL_MONITOR pm;
        if (pGetPhysicalMonitors(hMonitor, 1, &pm) && pm.hPhysicalMonitor != NULL && pm.hPhysicalMonitor != INVALID_HANDLE_VALUE) {
            hPhys = pm.hPhysicalMonitor;
            wcsncpy_s(desc, 128, pm.szPhysicalMonitorDescription, _TRUNCATE);
        }
        else {
            DWORD numMonitors = 0;
            if (pGetNumberOfPhysicalMonitors(hMonitor, &numMonitors) && numMonitors > 0) {
                LPPHYSICAL_MONITOR pPhysMonitors = (LPPHYSICAL_MONITOR)malloc(numMonitors * sizeof(PHYSICAL_MONITOR));
                if (pPhysMonitors) {
                    if (pGetPhysicalMonitors(hMonitor, numMonitors, pPhysMonitors)) {
                        for (DWORD i = 0; i < numMonitors; i++) {
                            if (pPhysMonitors[i].hPhysicalMonitor != NULL && pPhysMonitors[i].hPhysicalMonitor != INVALID_HANDLE_VALUE) {
                                hPhys = pPhysMonitors[i].hPhysicalMonitor;
                                wcsncpy_s(desc, 128, pPhysMonitors[i].szPhysicalMonitorDescription, _TRUNCATE);
                                break;
                            }
                        }
                    }
                    free(pPhysMonitors);
                }
            }
        }

        if (hPhys == NULL) return TRUE;

        MonitorInfo info;
        info.hMonitor = hMonitor;
        info.hPhysicalMonitor = hPhys;
        info.monitorNumber = (int)monitors.size() + 1;
        std::wstring friendly = GetMonitorFriendlyName(hMonitor);
        if (!friendly.empty()) info.displayName = friendly;
        else if (desc[0] != L'\0' && wcscmp(desc, L"%1") != 0) info.displayName = desc;
        else info.displayName = L"Monitor " + std::to_wstring(info.monitorNumber);

        info.regId = std::to_string(monitors.size());
        LoadMonitorRgb(info.regId, info.sliderRed, info.sliderGreen, info.sliderBlue);
        ApplyGamma(info);

        monitors.push_back(info);
        return TRUE;
        }, (LPARAM)&params);
}

void ReadMonitorValues(int idx) {
    MonitorInfo& info = monitors[idx];
    if (info.valuesRead) return;

    BOOL br = pGetMonitorBrightness(info.hPhysicalMonitor, &info.minBrightness, &info.curBrightness, &info.maxBrightness);
    if (!br && pGetVCPFeature) {
        DWORD cur, max;
        if (pGetVCPFeature(info.hPhysicalMonitor, 0x10, NULL, &cur, &max)) {
            if (max == 0) max = 100;
            info.curBrightness = (cur * 100) / max;
            info.minBrightness = 0;
            info.maxBrightness = 100;
            br = TRUE;
        }
    }
    info.supportsBrightness = br;

    BOOL cn = pGetMonitorContrast(info.hPhysicalMonitor, &info.minContrast, &info.curContrast, &info.maxContrast);
    if (!cn && pGetVCPFeature) {
        DWORD cur, max;
        if (pGetVCPFeature(info.hPhysicalMonitor, 0x12, NULL, &cur, &max)) {
            if (max == 0) max = 100;
            info.curContrast = (cur * 100) / max;
            info.minContrast = 0;
            info.maxContrast = 100;
            cn = TRUE;
        }
    }
    info.supportsContrast = cn;
    info.valuesRead = true;
}

void SetBrightness(HANDLE hMon, DWORD val) {
    if (pSetMonitorBrightness && pSetMonitorBrightness(hMon, val)) return;
    if (pSetVCPFeature) pSetVCPFeature(hMon, 0x10, val);
}

void SetContrast(HANDLE hMon, DWORD val) {
    if (pSetMonitorContrast && pSetMonitorContrast(hMon, val)) return;
    if (pSetVCPFeature) pSetVCPFeature(hMon, 0x12, val);
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
    {
        HBRUSH hBrush = CreateSolidBrush(RGB(32, 32, 32));
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)hBrush);

        HICON hBigIcon = (HICON)LoadImageW(
            GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(IDI_MAINICON),
            IMAGE_ICON,
            256, 256,
            LR_DEFAULTCOLOR
        );
        if (hBigIcon) {
            SendDlgItemMessageW(hDlg, IDI_MAINICON, STM_SETICON, (WPARAM)hBigIcon, 0);
        }
        return TRUE;
    }
    case WM_CTLCOLORDLG:
    {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(32, 32, 32));
        HBRUSH hBrush = (HBRUSH)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (hBrush) return (INT_PTR)hBrush;
        return (INT_PTR)GetStockObject(BLACK_BRUSH);
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hStatic = (HWND)lParam;

        if (GetDlgCtrlID(hStatic) == IDC_LINK) {
            SetTextColor(hdc, RGB(100, 180, 255));
        }
        else {
            SetTextColor(hdc, RGB(220, 220, 220));
        }

        SetBkColor(hdc, RGB(32, 32, 32));
        HBRUSH hBrush = (HBRUSH)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (hBrush) return (INT_PTR)hBrush;
        return (INT_PTR)GetStockObject(BLACK_BRUSH);
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_LINK && HIWORD(wParam) == STN_CLICKED) {
            ShellExecuteW(nullptr, L"open", L"https://github.com/somenmi", nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDOK);
        return TRUE;
    }
    return FALSE;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HINSTANCE g_hInst = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\YalkeeMonitors_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    if (strstr(lpCmdLine, "--silent") != NULL) {
        if (!LoadDxva2Functions()) {
            CloseHandle(hMutex);
            return 1;
        }
        int monitorIndex = 0;
        EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
            int idx = *(int*)lParam;
            std::string regId = std::to_string(idx);
            int r = 100, g = 100, b = 100;
            LoadMonitorRgb(regId, r, g, b);
            HDC hdc = GetMonitorDC(hMonitor);
            if (hdc) {
                int efR = 50 + (r * 50) / 100;
                int efG = 50 + (g * 50) / 100;
                int efB = 50 + (b * 50) / 100;
                WORD ramp[3][256];
                for (int i = 0; i < 256; i++) {
                    DWORD val = (i * 65535) / 255;
                    ramp[0][i] = (WORD)((val * efR) / 100);
                    ramp[1][i] = (WORD)((val * efG) / 100);
                    ramp[2][i] = (WORD)((val * efB) / 100);
                }
                SetDeviceGammaRamp(hdc, ramp);
                DeleteDC(hdc);
            }
            (*(int*)lParam)++;
            return TRUE;
            }, (LPARAM)&monitorIndex);
        CloseHandle(hMutex);
        return 0;
    }

    g_hInst = hInstance;

    HICON hIconBig = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    HICON hIconSmall = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, hInstance,
                       hIconBig,
                       nullptr,
                       (HBRUSH)(COLOR_WINDOW + 1),
                       nullptr,
                       L"YalkeeMonitorsClass",
                       hIconSmall };
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~WS_CAPTION & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Yalkee Monitors", dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 287,
        nullptr, nullptr, wc.hInstance, nullptr);

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (scrW - winW) / 2;
    int posY = (scrH - winH) / 2;
    SetWindowPos(hwnd, nullptr, posX, posY, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        CloseHandle(hMutex);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = NULL;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    if (!LoadDxva2Functions()) {
        MessageBoxW(hwnd, L"Failed to load Dxva2.dll", L"Error", MB_OK);
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        CloseHandle(hMutex);
        return 1;
    }
    EnumerateMonitors();

    ID3D11ShaderResourceView* logo_texture = nullptr;
    HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(IDB_PNG1), L"PNG");
    if (!hRes) {
        OutputDebugStringW(L"[Logo] FindResource failed\n");
    }
    else {
        HGLOBAL hData = LoadResource(nullptr, hRes);
        DWORD dataSize = SizeofResource(nullptr, hRes);
        const void* pData = LockResource(hData);
        if (pData && dataSize > 0) {
            int w, h;
            unsigned char* img = stbi_load_from_memory((const unsigned char*)pData, dataSize, &w, &h, nullptr, 4);
            if (img) {
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = w;
                desc.Height = h;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA initData = { img, (UINT)(w * 4), 0 };
                ID3D11Texture2D* tex = nullptr;
                if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &initData, &tex)) && tex) {
                    g_pd3dDevice->CreateShaderResourceView(tex, nullptr, &logo_texture);
                    tex->Release();
                }
                stbi_image_free(img);
            }
        }
    }

    bool autoStart = IsAutoStartEnabled();
    bool done = false;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(600, 287));
        ImGui::Begin("MainWindow", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            done = true;

        ImGui::Spacing();
        float windowWidth = ImGui::GetWindowWidth();
        const int btnCount = 10;
        const float btnWidth = 30.0f;
        const float spacing = 4.0f;
        float totalWidth = btnCount * btnWidth + (btnCount - 1) * spacing;
        float startX = (windowWidth - totalWidth) * 0.5f;
        ImGui::SetCursorPosX(startX);

        for (int i = 1; i <= 9; ++i) {
            bool disabled = (i > (int)monitors.size());
            if (disabled) ImGui::BeginDisabled();
            if (ImGui::Button(std::to_string(i).c_str(), ImVec2(btnWidth, 25))) {
                if (i <= (int)monitors.size()) {
                    if (selectedMonitor >= 0 && selectedMonitor < (int)monitors.size())
                        monitors[selectedMonitor].valuesRead = false;
                    selectedMonitor = i - 1;
                    monitors[selectedMonitor].valuesRead = false;
                }
            }
            if (disabled) ImGui::EndDisabled();
            ImGui::SameLine(0, spacing);
        }

        if (ImGui::Button("X", ImVec2(btnWidth, 25))) {
            selectedMonitor = -1;
        }

        ImGui::Spacing();
        float sepLeft = 12.0f;
        float sepRight = 28.0f;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(cursor.x + sepLeft, cursor.y + 3.0f),
            ImVec2(cursor.x + availWidth - sepRight, cursor.y + 3.0f),
            IM_COL32(80, 80, 80, 255),
            1.0f
        );
        ImGui::Dummy(ImVec2(0, 8));

        if (selectedMonitor >= 0 && selectedMonitor < (int)monitors.size()) {
            MonitorInfo& mon = monitors[selectedMonitor];
            ReadMonitorValues(selectedMonitor);

            std::wstring label = L"Monitor " + std::to_wstring(mon.monitorNumber) + L": " + mon.displayName;
            std::string utf8Label = WStringToUTF8(label);
            ImGui::SetCursorPosX((windowWidth - ImGui::CalcTextSize(utf8Label.c_str()).x) * 0.5f);
            ImGui::TextUnformatted(utf8Label.c_str());
            ImGui::Spacing();

            if (mon.supportsBrightness) {
                int brightness = (int)mon.curBrightness;
                ImGui::SetCursorPosX(80);
                ImGui::Text("Brightness");
                ImGui::SameLine(180);
                ImGui::SetNextItemWidth(250);
                if (ImGui::SliderInt("##Brightness", &brightness, 0, 100, "")) {
                    SetBrightness(mon.hPhysicalMonitor, brightness);
                    mon.curBrightness = brightness;
                }
                ImGui::SameLine(440);
                ImGui::SetNextItemWidth(45);
                if (ImGui::InputInt("##BrVal", &brightness, 0, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (brightness >= 0 && brightness <= 100) {
                        SetBrightness(mon.hPhysicalMonitor, brightness);
                        mon.curBrightness = brightness;
                    }
                }
                ImGui::SameLine(490);
                ImGui::Text("%%");
            }
            else {
                ImGui::SetCursorPosX(80);
                ImGui::Text("Brightness: N/A");
            }

            if (mon.supportsContrast) {
                int contrast = (int)mon.curContrast;
                ImGui::SetCursorPosX(80);
                ImGui::Text("Contrast");
                ImGui::SameLine(180);
                ImGui::SetNextItemWidth(250);
                if (ImGui::SliderInt("##Contrast", &contrast, 0, 100, "")) {
                    SetContrast(mon.hPhysicalMonitor, contrast);
                    mon.curContrast = contrast;
                }
                ImGui::SameLine(440);
                ImGui::SetNextItemWidth(45);
                if (ImGui::InputInt("##CtVal", &contrast, 0, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (contrast >= 0 && contrast <= 100) {
                        SetContrast(mon.hPhysicalMonitor, contrast);
                        mon.curContrast = contrast;
                    }
                }
                ImGui::SameLine(490);
                ImGui::Text("%%");
            }
            else {
                ImGui::SetCursorPosX(80);
                ImGui::Text("Contrast: N/A");
            }

            ImGui::Dummy(ImVec2(0, 10));
            ImVec2 cursorRgb = ImGui::GetCursorScreenPos();
            float availRgb = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(cursorRgb.x + sepLeft, cursorRgb.y + 3.0f),
                ImVec2(cursorRgb.x + availRgb - sepRight, cursorRgb.y + 3.0f),
                IM_COL32(80, 80, 80, 255),
                1.0f
            );
            ImGui::Dummy(ImVec2(0, 20));

            {
                int red = mon.sliderRed;
                ImGui::SetCursorPosX(80);
                ImGui::Text("Red");
                ImGui::SameLine(180);
                ImGui::SetNextItemWidth(250);
                if (ImGui::SliderInt("##Red", &red, 0, 100, "")) {
                    mon.sliderRed = red;
                    ApplyGamma(mon);
                    SaveMonitorRgb(mon.regId, mon.sliderRed, mon.sliderGreen, mon.sliderBlue);
                }
                ImGui::SameLine(440);
                ImGui::SetNextItemWidth(45);
                if (ImGui::InputInt("##RedVal", &red, 0, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (red >= 0 && red <= 100) {
                        mon.sliderRed = red;
                        ApplyGamma(mon);
                        SaveMonitorRgb(mon.regId, mon.sliderRed, mon.sliderGreen, mon.sliderBlue);
                    }
                }
                ImGui::SameLine(490);
                ImGui::Text("%%");
            }

            {
                int green = mon.sliderGreen;
                ImGui::SetCursorPosX(80);
                ImGui::Text("Green");
                ImGui::SameLine(180);
                ImGui::SetNextItemWidth(250);
                if (ImGui::SliderInt("##Green", &green, 0, 100, "")) {
                    mon.sliderGreen = green;
                    ApplyGamma(mon);
                    SaveMonitorRgb(mon.regId, mon.sliderRed, mon.sliderGreen, mon.sliderBlue);
                }
                ImGui::SameLine(440);
                ImGui::SetNextItemWidth(45);
                if (ImGui::InputInt("##GreenVal", &green, 0, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (green >= 0 && green <= 100) {
                        mon.sliderGreen = green;
                        ApplyGamma(mon);
                        SaveMonitorRgb(mon.regId, mon.sliderRed, mon.sliderGreen, mon.sliderBlue);
                    }
                }
                ImGui::SameLine(490);
                ImGui::Text("%%");

                ImGui::SameLine(0, 10);
                float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2;
                float btnHeight = ImGui::GetFrameHeight();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (lineHeight - btnHeight) * 0.5f);
                ImGui::SameLine(0, 10);
                if (ImGui::Button("Reset", ImVec2(55, 0))) {
                    DeleteMonitorRgb();
                    for (auto& m : monitors) {
                        m.sliderRed = m.sliderGreen = m.sliderBlue = 100;
                    }
                    ResetAllGammas();
                    SetAutoStart(false);
                    autoStart = false;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Delete registry config and restore default gamma + delete auto-start");
            }

            {
                int blue = mon.sliderBlue;
                ImGui::SetCursorPosX(80);
                ImGui::Text("Blue");
                ImGui::SameLine(180);
                ImGui::SetNextItemWidth(250);
                if (ImGui::SliderInt("##Blue", &blue, 0, 100, "")) {
                    mon.sliderBlue = blue;
                    ApplyGamma(mon);
                    SaveMonitorRgb(mon.regId, mon.sliderRed, mon.sliderGreen, mon.sliderBlue);
                }
                ImGui::SameLine(440);
                ImGui::SetNextItemWidth(45);
                if (ImGui::InputInt("##BlueVal", &blue, 0, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (blue >= 0 && blue <= 100) {
                        mon.sliderBlue = blue;
                        ApplyGamma(mon);
                        SaveMonitorRgb(mon.regId, mon.sliderRed, mon.sliderGreen, mon.sliderBlue);
                    }
                }
                ImGui::SameLine(490);
                ImGui::Text("%%");
            }

            ImGui::Dummy(ImVec2(0, 10));
        }
        else {
            ImGui::SetCursorPosX((windowWidth - ImGui::CalcTextSize("No monitor selected").x) * 0.5f);
            ImGui::Text("No monitor selected");

            ImGui::Dummy(ImVec2(0.0f, 50.0f));

            const char* pre = "by ";
            const char* mid = "Yalkee - ";
            const char* link = "GitHub";

            float logoW = 16.0f;
            float space = 4.0f;
            float preW = ImGui::CalcTextSize(pre).x;
            float midW = ImGui::CalcTextSize(mid).x;
            float linkW = ImGui::CalcTextSize(link).x;
            float totalW = preW + logoW + space + midW + linkW;

            float startX = (windowWidth - totalW) * 0.5f;
            ImGui::SetCursorPosX(startX);

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 160, 160, 255));
            ImGui::TextUnformatted(pre);
            ImGui::SameLine(0, 0);

            if (logo_texture) {
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                ImGui::Image((void*)logo_texture, ImVec2(16, 16));
                ImGui::SetCursorScreenPos(cursorPos);
                if (ImGui::InvisibleButton("aboutLogo", ImVec2(16, 16))) {
                    DialogBoxW(g_hInst, MAKEINTRESOURCEW(IDD_ABOUT), hwnd, AboutDialogProc);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 16, cursorPos.y));
                ImGui::SameLine(0, space);
            }
            else {
                ImGui::SameLine(0, logoW + space);
            }

            ImGui::TextUnformatted(mid);
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted(link);
            ImVec2 linkStart = ImGui::GetItemRectMin();
            ImVec2 linkEnd = ImGui::GetItemRectMax();
            ImGui::SetCursorScreenPos(linkStart);
            if (ImGui::InvisibleButton("githubLink", ImVec2(linkEnd.x - linkStart.x, linkEnd.y - linkStart.y))) {
                ShellExecuteA(nullptr, "open", "https://github.com/somenmi", nullptr, nullptr, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetCursorScreenPos(linkStart);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 180, 255, 255));
                ImGui::TextUnformatted(link);
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();
            const char* autoText = "Auto-start with Windows";
            float autoTextWidth = ImGui::CalcTextSize(autoText).x + 25.0f;
            ImGui::SetCursorPosX((windowWidth - autoTextWidth) * 0.5f);
            if (ImGui::Checkbox(autoText, &autoStart)) {
                SetAutoStart(autoStart);
            }
        }

        ImGui::End();

        ImGui::Render();
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    for (auto& mon : monitors) {
        if (mon.hPhysicalMonitor && pDestroyPhysicalMonitor)
            pDestroyPhysicalMonitor(mon.hPhysicalMonitor);
    }

    ImGui_ImplDX11_Shutdown();
    if (logo_texture) logo_texture->Release();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CloseHandle(hMutex);
    return 0;
}