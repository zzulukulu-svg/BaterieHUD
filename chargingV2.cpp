#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#include <powrprof.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <mmsystem.h>
#include <string>
#include <cmath>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;

namespace Config {
    constexpr int HUD_SIZE = 350;
    constexpr int ANIM_FRAMES = 30;
    constexpr int HOLD_FRAMES = 120;
    constexpr int FADEOUT_FRAMES = 20;
    constexpr int TIMER_INTERVAL_MS = 16;
    constexpr wchar_t WINDOW_CLASS[] = L"BatteryHUDClass";
    constexpr wchar_t CONFIG_FILE[] = L"\\BatteryHUD\\config.dat";
}

#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1001
#define IDM_TEST 1002
#define IDM_COLOR_CHARGE 1003
#define IDM_COLOR_DISCHARGE 1004
#define IDM_RESET 1005
#define IDM_TOGGLE_UNPLUG 1006
#define IDM_TOGGLE_SOUND 1007
#define TRAY_ICON_ID 1

struct AppSettings {
    Color chargeColor = Color(255, 0, 230, 120);
    Color dischargeColor = Color(255, 255, 150, 50);
    bool useCustomChargeColor = false;
    bool useCustomDischargeColor = false;
    bool showOnUnplug = false;
    bool playSound = false;
};

struct HUDState {
    bool isVisible = false;
    int animFrame = 0;
    int holdFrame = 0;
    bool isFadingOut = false;
    BYTE batteryPercent = 0;
    Color themeColor = Color(255, 0, 230, 120);
    bool isCharging = false;

    void reset() {
        isVisible = false;
        animFrame = 0;
        holdFrame = 0;
        isFadingOut = false;
    }

    void startAnimation(BYTE percent, bool charging, const AppSettings& settings) {
        batteryPercent = (percent > 100) ? 100 : percent;
        isCharging = charging;

        if (charging) {
            themeColor = settings.useCustomChargeColor
                ? settings.chargeColor
                : (batteryPercent < 20 ? Color(255, 255, 50, 50) : Color(255, 0, 230, 120));
        }
        else {
            themeColor = settings.useCustomDischargeColor
                ? settings.dischargeColor
                : Color(255, 255, 150, 50);
        }

        isVisible = true;
        isFadingOut = false;
        animFrame = 0;
        holdFrame = 0;
    }
};

namespace Utils {
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

    bool ChooseColor(HWND hwnd, Color& color) {
        static COLORREF customColors[16] = { 0 };

        CHOOSECOLOR cc = {};
        cc.lStructSize = sizeof(CHOOSECOLOR);
        cc.hwndOwner = hwnd;
        cc.rgbResult = RGB(color.GetR(), color.GetG(), color.GetB());
        cc.lpCustColors = customColors;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;

        if (ChooseColor(&cc)) {
            color = Color(255, GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
            return true;
        }
        return false;
    }

    void PlayNotificationSound(bool isCharging) {
        if (isCharging) {
            PlaySound(L"SystemNotification", nullptr, SND_ALIAS | SND_ASYNC);
        }
        else {
            PlaySound(L"SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
        }
    }

    std::wstring GetConfigPath() {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
            std::wstring configPath = path;
            configPath += Config::CONFIG_FILE;
            return configPath;
        }
        return L"";
    }

    void SaveSettings(const AppSettings& settings) {
        std::wstring configPath = GetConfigPath();
        if (configPath.empty()) return;

        std::wstring dir = configPath.substr(0, configPath.find_last_of(L'\\'));
        CreateDirectoryW(dir.c_str(), nullptr);

        std::ofstream file(configPath, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&settings.chargeColor), sizeof(Color));
            file.write(reinterpret_cast<const char*>(&settings.dischargeColor), sizeof(Color));
            file.write(reinterpret_cast<const char*>(&settings.useCustomChargeColor), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&settings.useCustomDischargeColor), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&settings.showOnUnplug), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&settings.playSound), sizeof(bool));
            file.close();
        }
    }

    bool LoadSettings(AppSettings& settings) {
        std::wstring configPath = GetConfigPath();
        if (configPath.empty()) return false;

        std::ifstream file(configPath, std::ios::binary);
        if (file.is_open()) {
            file.read(reinterpret_cast<char*>(&settings.chargeColor), sizeof(Color));
            file.read(reinterpret_cast<char*>(&settings.dischargeColor), sizeof(Color));
            file.read(reinterpret_cast<char*>(&settings.useCustomChargeColor), sizeof(bool));
            file.read(reinterpret_cast<char*>(&settings.useCustomDischargeColor), sizeof(bool));
            file.read(reinterpret_cast<char*>(&settings.showOnUnplug), sizeof(bool));
            file.read(reinterpret_cast<char*>(&settings.playSound), sizeof(bool));
            file.close();
            return true;
        }
        return false;
    }

    void DeleteConfig() {
        std::wstring configPath = GetConfigPath();
        if (!configPath.empty()) {
            DeleteFileW(configPath.c_str());
        }
    }
}

class HUDRenderer {
public:
    void Render(HWND hwnd, const HUDState& state) {
        HDC hdcScreen = GetDC(nullptr);
        if (!hdcScreen) return;

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        if (!hdcMem) {
            ReleaseDC(nullptr, hdcScreen);
            return;
        }

        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, Config::HUD_SIZE, Config::HUD_SIZE);
        if (!hBitmap) {
            DeleteDC(hdcMem);
            ReleaseDC(nullptr, hdcScreen);
            return;
        }

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
        wcscpy_s(nid.szTip, L"Battery HUD");

        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    static void Remove(HWND hwnd) {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = TRAY_ICON_ID;

        Shell_NotifyIconW(NIM_DELETE, &nid);
    }

    static void ShowContextMenu(HWND hwnd, const AppSettings& settings) {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, IDM_TEST, L"Animation testen");
        AppendMenuW(hMenu, MF_STRING, IDM_COLOR_CHARGE, L"Farbe Laden");
        AppendMenuW(hMenu, MF_STRING, IDM_COLOR_DISCHARGE, L"Farbe Entladen");
        AppendMenuW(hMenu, MF_STRING, IDM_RESET, L"Farben zurücksetzen");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, settings.showOnUnplug ? MF_CHECKED : MF_UNCHECKED, IDM_TOGGLE_UNPLUG, L"Animation beim Ausstecken");
        AppendMenuW(hMenu, settings.playSound ? MF_CHECKED : MF_UNCHECKED, IDM_TOGGLE_SOUND, L"Sound abspielen");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Beenden");

        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hMenu);
    }
};

HUDState g_hud;
HUDRenderer g_renderer;
AppSettings g_settings;
bool g_lastChargingState = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        TrayIconManager::Create(hwnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            TrayIconManager::ShowContextMenu(hwnd, g_settings);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TEST: {
            BYTE percent;
            bool isCharging;

            if (Utils::GetBatteryStatus(percent, isCharging)) {
                g_hud.startAnimation(percent, isCharging, g_settings);
                SetTimer(hwnd, 1, Config::TIMER_INTERVAL_MS, nullptr);
            }
            return 0;
        }
        case IDM_COLOR_CHARGE: {
            Color newColor = g_settings.chargeColor;
            if (Utils::ChooseColor(hwnd, newColor)) {
                g_settings.chargeColor = newColor;
                g_settings.useCustomChargeColor = true;
                Utils::SaveSettings(g_settings);
            }
            return 0;
        }
        case IDM_COLOR_DISCHARGE: {
            Color newColor = g_settings.dischargeColor;
            if (Utils::ChooseColor(hwnd, newColor)) {
                g_settings.dischargeColor = newColor;
                g_settings.useCustomDischargeColor = true;
                Utils::SaveSettings(g_settings);
            }
            return 0;
        }
        case IDM_RESET: {
            g_settings.useCustomChargeColor = false;
            g_settings.useCustomDischargeColor = false;
            Utils::SaveSettings(g_settings);
            return 0;
        }
        case IDM_TOGGLE_UNPLUG: {
            g_settings.showOnUnplug = !g_settings.showOnUnplug;
            Utils::SaveSettings(g_settings);
            return 0;
        }
        case IDM_TOGGLE_SOUND: {
            g_settings.playSound = !g_settings.playSound;
            Utils::SaveSettings(g_settings);
            return 0;
        }
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMPOWERSTATUSCHANGE) {
            if (!g_hud.isVisible) {
                BYTE percent;
                bool isCharging;

                if (Utils::GetBatteryStatus(percent, isCharging)) {
                    bool stateChanged = (isCharging != g_lastChargingState);
                    g_lastChargingState = isCharging;

                    if (stateChanged) {
                        if (isCharging || g_settings.showOnUnplug) {
                            if (g_settings.playSound) {
                                Utils::PlayNotificationSound(isCharging);
                            }
                            g_hud.startAnimation(percent, isCharging, g_settings);
                            SetTimer(hwnd, 1, Config::TIMER_INTERVAL_MS, nullptr);
                        }
                    }
                }
            }
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == 1) {
            if (!g_hud.isVisible) {
                KillTimer(hwnd, 1);
                return 0;
            }

            if (!g_hud.isFadingOut) {
                if (g_hud.animFrame < Config::ANIM_FRAMES) {
                    g_hud.animFrame++;
                }
                else if (++g_hud.holdFrame > Config::HOLD_FRAMES) {
                    g_hud.isFadingOut = true;
                    g_hud.holdFrame = Config::FADEOUT_FRAMES;
                }
            }
            else {
                if (--g_hud.holdFrame <= 0) {
                    g_hud.reset();
                    KillTimer(hwnd, 1);
                }
            }

            g_renderer.Render(hwnd, g_hud);
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

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    HWND existing = FindWindowW(Config::WINDOW_CLASS, nullptr);
    if (existing) {
        MessageBoxW(nullptr, L"Battery HUD läuft bereits!", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        return -1;
    }

    Utils::LoadSettings(g_settings);

    BYTE percent;
    bool isCharging;
    if (Utils::GetBatteryStatus(percent, isCharging)) {
        g_lastChargingState = isCharging;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = Config::WINDOW_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        GdiplusShutdown(gdiplusToken);
        return -1;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        Config::WINDOW_CLASS,
        L"Battery HUD",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        UnregisterClassW(Config::WINDOW_CLASS, hInstance);
        GdiplusShutdown(gdiplusToken);
        return -1;
    }

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(Config::WINDOW_CLASS, hInstance);
    GdiplusShutdown(gdiplusToken);

    return static_cast<int>(msg.wParam);
}