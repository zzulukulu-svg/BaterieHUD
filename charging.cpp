#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#include <powrprof.h>
#include <shellapi.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

// ============================================================================
// KONFIGURATION & KONSTANTEN
// ============================================================================

namespace Config {
    constexpr int HUD_SIZE = 350;
    constexpr BYTE DRAG_ALPHA = 175;
    constexpr int ANIM_FRAMES = 30;
    constexpr int HOLD_FRAMES = 120;
    constexpr int FADEOUT_FRAMES = 20;
    constexpr int TIMER_INTERVAL_MS = 16;
    constexpr wchar_t MUTEX_NAME[] = L"Global\\BatteryHUDMonolith";
    constexpr wchar_t WINDOW_CLASS[] = L"BatteryHUDClass";
}

// Tray Icon IDs
#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1001
#define IDM_TEST 1002
#define TRAY_ICON_ID 1

// ============================================================================
// DATENSTRUKTUREN
// ============================================================================

struct WindowState {
    bool wasLayered;
    BYTE originalAlpha;
    DWORD originalFlags;

    WindowState() : wasLayered(false), originalAlpha(255), originalFlags(0) {}
};

struct HUDState {
    bool isVisible = false;
    int animFrame = 0;
    int holdFrame = 0;
    bool isFadingOut = false;
    BYTE batteryPercent = 0;
    Color themeColor = Color(255, 0, 230, 120);

    void reset() {
        isVisible = false;
        animFrame = 0;
        holdFrame = 0;
        isFadingOut = false;
    }

    void startAnimation(BYTE percent) {
        batteryPercent = (percent > 100) ? 100 : percent;
        themeColor = (batteryPercent < 20) ? Color(255, 255, 50, 50) : Color(255, 0, 230, 120);
        isVisible = true;
        isFadingOut = false;
        animFrame = 0;
        holdFrame = 0;
    }
};

// ============================================================================
// GLOBALER STATE (Thread-Safe)
// ============================================================================

class GlobalState {
public:
    static GlobalState& instance() {
        static GlobalState inst;
        return inst;
    }

    void addDragState(HWND hwnd, const WindowState& state) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dragStates[hwnd] = state;
    }

    bool removeDragState(HWND hwnd, WindowState& outState) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_dragStates.find(hwnd);
        if (it != m_dragStates.end()) {
            outState = it->second;
            m_dragStates.erase(it);
            return true;
        }
        return false;
    }

    HUDState& hud() { return m_hud; }

private:
    GlobalState() = default;
    std::unordered_map<HWND, WindowState> m_dragStates;
    std::mutex m_mutex;
    HUDState m_hud;
};

// ============================================================================
// HILFSFUNKTIONEN
// ============================================================================

namespace Utils {
    bool SetTaskbarSeconds(bool enable) {
        HKEY hKey;
        DWORD val = enable ? 1 : 0;

        LONG result = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
            0,
            KEY_SET_VALUE,
            &hKey
        );

        if (result != ERROR_SUCCESS) return false;

        result = RegSetValueExW(
            hKey,
            L"ShowSecondsInSystemClock",
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&val),
            sizeof(val)
        );

        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS) {
            SendMessageTimeoutW(
                HWND_BROADCAST,
                WM_SETTINGCHANGE,
                0,
                reinterpret_cast<LPARAM>(L"TraySettings"),
                SMTO_ABORTIFHUNG,
                2000,
                nullptr
            );
        }

        return result == ERROR_SUCCESS;
    }

    inline float EaseOutBack(float t) {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        const float t1 = t - 1.0f;
        return 1.0f + c3 * t1 * t1 * t1 + c1 * t1 * t1;
    }

    template<typename T>
    inline T Clamp(T value, T min, T max) {
        return (value < min) ? min : (value > max) ? max : value;
    }

    bool GetBatteryStatus(BYTE& outPercent, bool& outIsCharging) {
        SYSTEM_POWER_STATUS sps;
        if (!GetSystemPowerStatus(&sps)) return false;

        outPercent = (sps.BatteryLifePercent > 100) ? 100 : sps.BatteryLifePercent;
        outIsCharging = (sps.ACLineStatus == 1);

        return true;
    }
}

// ============================================================================
// RENDERING ENGINE
// ============================================================================

class HUDRenderer {
public:
    void Render(HWND hwnd, const HUDState& state) {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, Config::HUD_SIZE, Config::HUD_SIZE);
        HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, hBitmap));

        Graphics graphics(hdcMem);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        graphics.Clear(Color(0, 0, 0, 0));

        float progress = Utils::Clamp(
            static_cast<float>(state.animFrame) / Config::ANIM_FRAMES,
            0.0f, 1.0f
        );

        float scale, alphaFactor;
        if (state.isFadingOut) {
            float fadeProgress = static_cast<float>(state.holdFrame) / Config::FADEOUT_FRAMES;
            scale = 1.0f - (1.0f - fadeProgress) * 0.1f;
            alphaFactor = fadeProgress;
        }
        else {
            scale = Utils::EaseOutBack(progress);
            alphaFactor = (progress > 0.5f) ? 1.0f : progress * 2.0f;
        }

        graphics.TranslateTransform(Config::HUD_SIZE / 2.0f, Config::HUD_SIZE / 2.0f);
        graphics.ScaleTransform(scale, scale);
        graphics.TranslateTransform(-Config::HUD_SIZE / 2.0f, -Config::HUD_SIZE / 2.0f);

        int alpha = Utils::Clamp(static_cast<int>(255 * alphaFactor), 0, 255);

        RenderGlow(graphics, state.themeColor, alpha);
        RenderBatteryRing(graphics, state.themeColor, state.batteryPercent, alpha);
        RenderPercentageText(graphics, state.batteryPercent, alpha);
        UpdateLayeredWindowContent(hwnd, hdcScreen, hdcMem, alpha);

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

private:
    void RenderGlow(Graphics& graphics, const Color& themeColor, int alpha) {
        GraphicsPath path;
        path.AddEllipse(0, 0, Config::HUD_SIZE, Config::HUD_SIZE);

        PathGradientBrush brush(&path);
        Color centerColor(alpha / 4, themeColor.GetR(), themeColor.GetG(), themeColor.GetB());
        Color edgeColor(0, 0, 0, 0);

        int count = 1;
        brush.SetCenterColor(centerColor);
        brush.SetSurroundColors(&edgeColor, &count);

        graphics.FillEllipse(&brush, 0, 0, Config::HUD_SIZE, Config::HUD_SIZE);
    }

    void RenderBatteryRing(Graphics& graphics, const Color& themeColor, BYTE percent, int alpha) {
        Pen ringPen(
            Color(alpha, themeColor.GetR(), themeColor.GetG(), themeColor.GetB()),
            10.0f
        );

        ringPen.SetStartCap(LineCapRound);
        ringPen.SetEndCap(LineCapRound);

        constexpr int margin = 60;
        float sweepAngle = 360.0f * (percent / 100.0f);

        graphics.DrawArc(&ringPen, margin, margin,
            Config::HUD_SIZE - 2 * margin, Config::HUD_SIZE - 2 * margin,
            -90.0f, sweepAngle);
    }

    void RenderPercentageText(Graphics& graphics, BYTE percent, int alpha) {
        FontFamily fontFamily(L"Segoe UI");
        Font font(&fontFamily, 50, FontStyleBold, UnitPixel);
        SolidBrush textBrush(Color(alpha, 255, 255, 255));

        std::wstring text = std::to_wstring(percent) + L"%";

        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);

        RectF layoutRect(0, 0, static_cast<float>(Config::HUD_SIZE), static_cast<float>(Config::HUD_SIZE));
        graphics.DrawString(text.c_str(), -1, &font, layoutRect, &format, &textBrush);
    }

    void UpdateLayeredWindowContent(HWND hwnd, HDC hdcScreen, HDC hdcMem, int alpha) {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        POINT ptDest = {
            (screenWidth - Config::HUD_SIZE) / 2,
            (screenHeight - Config::HUD_SIZE) / 2
        };
        SIZE size = { Config::HUD_SIZE, Config::HUD_SIZE };
        POINT ptSrc = { 0, 0 };

        BLENDFUNCTION blend = {
            AC_SRC_OVER, 0,
            static_cast<BYTE>(alpha),
            AC_SRC_ALPHA
        };

        UpdateLayeredWindow(hwnd, hdcScreen, &ptDest, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    }
};

// ============================================================================
// WINDOW TRANSPARENCY MANAGER
// ============================================================================

class TransparencyManager {
public:
    static void BeginDrag(HWND hwnd) {
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        bool wasLayered = (exStyle & WS_EX_LAYERED) != 0;

        WindowState state;
        state.wasLayered = wasLayered;

        if (wasLayered) {
            GetLayeredWindowAttributes(hwnd, nullptr, &state.originalAlpha, &state.originalFlags);
        }

        GlobalState::instance().addDragState(hwnd, state);

        if (!wasLayered) {
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }

        SetLayeredWindowAttributes(hwnd, 0, Config::DRAG_ALPHA, LWA_ALPHA);
    }

    static void EndDrag(HWND hwnd) {
        WindowState state;
        if (GlobalState::instance().removeDragState(hwnd, state)) {
            if (state.wasLayered) {
                SetLayeredWindowAttributes(hwnd, 0, state.originalAlpha, state.originalFlags);
            }
            else {
                SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
                LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            }
        }
    }
};

// ============================================================================
// TRAY ICON MANAGER
// ============================================================================

class TrayIconManager {
public:
    static void Create(HWND hwnd) {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = TRAY_ICON_ID;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(nid.szTip, L"Battery HUD - Aktiv");

        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    static void Remove(HWND hwnd) {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = TRAY_ICON_ID;

        Shell_NotifyIconW(NIM_DELETE, &nid);
    }

    static void ShowContextMenu(HWND hwnd) {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, IDM_TEST, L"Animation testen");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Beenden");

        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);
    }
};

// ============================================================================
// EVENT CALLBACKS
// ============================================================================

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HUDRenderer renderer;

    switch (msg) {
    case WM_CREATE:
        TrayIconManager::Create(hwnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            TrayIconManager::ShowContextMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TEST: {
            auto& hud = GlobalState::instance().hud();
            BYTE percent;
            bool isCharging;

            if (Utils::GetBatteryStatus(percent, isCharging)) {
                hud.startAnimation(percent);
                SetTimer(hwnd, 1, Config::TIMER_INTERVAL_MS, nullptr);
            }
            return 0;
        }
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMPOWERSTATUSCHANGE || wParam == PBT_POWERSETTINGCHANGE) {
            auto& hud = GlobalState::instance().hud();
            if (!hud.isVisible) {
                BYTE percent;
                bool isCharging;

                if (Utils::GetBatteryStatus(percent, isCharging) && isCharging) {
                    hud.startAnimation(percent);
                    SetTimer(hwnd, 1, Config::TIMER_INTERVAL_MS, nullptr);
                }
            }
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == 1) {
            auto& hud = GlobalState::instance().hud();

            if (!hud.isFadingOut) {
                if (hud.animFrame < Config::ANIM_FRAMES) {
                    hud.animFrame++;
                }
                else if (++hud.holdFrame > Config::HOLD_FRAMES) {
                    hud.isFadingOut = true;
                    hud.holdFrame = Config::FADEOUT_FRAMES;
                }
            }
            else {
                if (--hud.holdFrame <= 0) {
                    hud.reset();
                    KillTimer(hwnd, 1);
                }
            }

            renderer.Render(hwnd, hud);
        }
        return 0;

    case WM_DESTROY:
        TrayIconManager::Remove(hwnd);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
    LONG idObj, LONG idChild, DWORD thread, DWORD time)
{
    if (idObj != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd || !IsWindowVisible(hwnd)) return;

    switch (event) {
    case EVENT_SYSTEM_MOVESIZESTART:
        TransparencyManager::BeginDrag(hwnd);
        break;

    case EVENT_SYSTEM_MOVESIZEEND:
        TransparencyManager::EndDrag(hwnd);
        break;
    }
}

// ============================================================================
// APPLICATION CLASS
// ============================================================================

class Application {
public:
    Application() : m_instance(nullptr), m_hudWindow(nullptr), m_hook(nullptr), m_gdiplusToken(0) {}

    ~Application() {
        Cleanup();
    }

    bool Initialize(HINSTANCE hInstance) {
        m_instance = hInstance;

        GdiplusStartupInput gdiplusStartupInput;
        if (GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
            return false;
        }

        Utils::SetTaskbarSeconds(true);

        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = m_instance;
        wc.lpszClassName = Config::WINDOW_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        if (!RegisterClassW(&wc)) return false;

        m_hudWindow = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
            Config::WINDOW_CLASS,
            L"Battery HUD",
            WS_POPUP,
            0, 0, 1, 1,
            nullptr, nullptr,
            m_instance,
            nullptr
        );

        if (!m_hudWindow) return false;

        ShowWindow(m_hudWindow, SW_SHOW);

        m_hook = SetWinEventHook(
            EVENT_SYSTEM_MOVESIZESTART,
            EVENT_SYSTEM_MOVESIZEEND,
            nullptr,
            WinEventProc,
            0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
        );

        return m_hook != nullptr;
    }

    int Run() {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    void Cleanup() {
        if (m_hook) {
            UnhookWinEvent(m_hook);
            m_hook = nullptr;
        }

        if (m_hudWindow) {
            DestroyWindow(m_hudWindow);
            m_hudWindow = nullptr;
        }

        if (m_instance) {
            UnregisterClassW(Config::WINDOW_CLASS, m_instance);
        }

        if (m_gdiplusToken) {
            GdiplusShutdown(m_gdiplusToken);
            m_gdiplusToken = 0;
        }
    }

private:
    HINSTANCE m_instance;
    HWND m_hudWindow;
    HWINEVENTHOOK m_hook;
    ULONG_PTR m_gdiplusToken;
};

// ============================================================================
// ENTRY POINT
// ============================================================================

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, Config::MUTEX_NAME);
    if (!hMutex) return -1;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        MessageBoxW(nullptr, L"Battery HUD läuft bereits!", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    Application app;
    int exitCode = 0;

    if (app.Initialize(hInstance)) {
        MessageBoxW(nullptr,
            L"Battery HUD ist aktiv!\n\n"
            L"→ Tray-Icon (Rechtsklick für Menü)\n"
            L"→ Ladekabel einstecken = Animation\n"
            L"→ Fenster verschieben = Transparent",
            L"Battery HUD",
            MB_OK | MB_ICONINFORMATION);

        exitCode = app.Run();
    }
    else {
        exitCode = -1;
    }

    app.Cleanup();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return exitCode;
}