// Win32 GUI for cm-rgb
// Pure Win32 API - no MFC, no .NET, no resource files - works on Windows XP

#include "gui.h"
#include "controller.h"
#include <windows.h>
#include <commctrl.h>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

// ---------------------------------------------------------------------------
// Window controls IDs
// ---------------------------------------------------------------------------
enum {
    ID_TAB = 100,
    // Logo tab
    ID_LOGO_MODE,
    ID_LOGO_COLOR,
    ID_LOGO_BRIGHTNESS,
    ID_LOGO_SPEED,
    // Fan tab
    ID_FAN_MODE,
    ID_FAN_COLOR,
    ID_FAN_BRIGHTNESS,
    ID_FAN_SPEED,
    ID_FAN_MIRAGE_R,
    ID_FAN_MIRAGE_G,
    ID_FAN_MIRAGE_B,
    // Ring tab
    ID_RING_MODE,
    ID_RING_COLOR,
    ID_RING_BRIGHTNESS,
    ID_RING_SPEED,
    // Buttons
    ID_APPLY,
    ID_DISABLE,
    ID_RESTORE,
    // Color picker
    ID_CP_R_SLIDER,
    ID_CP_G_SLIDER,
    ID_CP_B_SLIDER,
    ID_CP_R_EDIT,
    ID_CP_G_EDIT,
    ID_CP_B_EDIT,
    ID_CP_PREVIEW,
    ID_CP_OK,
    ID_CP_CANCEL
};

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
struct GUIState {
    HWND hTab;
    HWND hLogoPage, hFanPage, hRingPage;  // Container windows for each tab
    HWND hLogoMode, hLogoColor, hLogoBright, hLogoSpeed;
    HWND hFanMode,  hFanColor,  hFanBright,  hFanSpeed;
    HWND hFanMirageR, hFanMirageG, hFanMirageB;
    HWND hRingMode, hRingColor, hRingBright, hRingSpeed;
    HWND hApply, hDisable, hRestore;

    // Color values (0-255)
    int logoR, logoG, logoB;
    int fanR,  fanG,  fanB;
    int ringR, ringG, ringB;
};

static GUIState g_state;

// ---------------------------------------------------------------------------
// Color picker state (per instance)
// ---------------------------------------------------------------------------
struct ColorPickerData {
    int r, g, b;
    int* outR;
    int* outG;
    int* outB;
    HWND hR, hG, hB;
    HWND hRSlider, hGSlider, hBSlider;
    HWND hPreview;
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static LRESULT CALLBACK ColorPickerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void ShowColorPicker(HWND hParent, int& r, int& g, int& b);

// ---------------------------------------------------------------------------
// Create a labeled control on a parent
// ---------------------------------------------------------------------------
static HWND CreateLabel(HWND hParent, const char* text, int x, int y, int w, int h) {
    return CreateWindow("STATIC", text, WS_VISIBLE | WS_CHILD,
                        x, y, w, h, hParent, NULL, GetModuleHandle(NULL), NULL);
}

static HWND CreateCombo(HWND hParent, int id, int x, int y, int w, int h) {
    return CreateWindow("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                        x, y, w, h, hParent, (HMENU)id, GetModuleHandle(NULL), NULL);
}

static HWND CreateEdit(HWND hParent, int id, int x, int y, int w, int h) {
    return CreateWindow("EDIT", "3", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                        x, y, w, h, hParent, (HMENU)id, GetModuleHandle(NULL), NULL);
}

static HWND CreateButton(HWND hParent, const char* text, int id, int x, int y, int w, int h) {
    return CreateWindow("BUTTON", text, WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        x, y, w, h, hParent, (HMENU)id, GetModuleHandle(NULL), NULL);
}

static HWND CreateColorBtn(HWND hParent, int id, int x, int y, int w, int h) {
    return CreateWindow("BUTTON", "", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        x, y, w, h, hParent, (HMENU)id, GetModuleHandle(NULL), NULL);
}

// ---------------------------------------------------------------------------
// Tab content pages - each page is a child window of the tab control
// ---------------------------------------------------------------------------
static void CreateLogoPage(HWND hTab) {
    RECT rc;
    GetClientRect(hTab, &rc);
    TabCtrl_AdjustRect(hTab, FALSE, &rc);

    g_state.hLogoPage = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,
                                      rc.left, rc.top,
                                      rc.right - rc.left, rc.bottom - rc.top,
                                      hTab, NULL, GetModuleHandle(NULL), NULL);

    int y = 10;
    CreateLabel(g_state.hLogoPage, "Mode:", 10, y, 50, 20);
    g_state.hLogoMode = CreateCombo(g_state.hLogoPage, ID_LOGO_MODE, 70, y, 100, 200);
    SendMessage(g_state.hLogoMode, CB_ADDSTRING, 0, (LPARAM)"Off");
    SendMessage(g_state.hLogoMode, CB_ADDSTRING, 0, (LPARAM)"Static");
    SendMessage(g_state.hLogoMode, CB_ADDSTRING, 0, (LPARAM)"Breathe");
    SendMessage(g_state.hLogoMode, CB_SETCURSEL, 1, 0);

    y += 30;
    CreateLabel(g_state.hLogoPage, "Color:", 10, y, 50, 20);
    g_state.hLogoColor = CreateColorBtn(g_state.hLogoPage, ID_LOGO_COLOR, 70, y, 40, 20);
    g_state.logoR = 0; g_state.logoG = 255; g_state.logoB = 255;

    y += 30;
    CreateLabel(g_state.hLogoPage, "Brightness (1-5):", 10, y, 100, 20);
    g_state.hLogoBright = CreateEdit(g_state.hLogoPage, ID_LOGO_BRIGHTNESS, 120, y, 40, 20);

    y += 30;
    CreateLabel(g_state.hLogoPage, "Speed (1-5):", 10, y, 100, 20);
    g_state.hLogoSpeed = CreateEdit(g_state.hLogoPage, ID_LOGO_SPEED, 120, y, 40, 20);
}

static void CreateFanPage(HWND hTab) {
    RECT rc;
    GetClientRect(hTab, &rc);
    TabCtrl_AdjustRect(hTab, FALSE, &rc);

    g_state.hFanPage = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,
                                     rc.left, rc.top,
                                     rc.right - rc.left, rc.bottom - rc.top,
                                     hTab, NULL, GetModuleHandle(NULL), NULL);

    int y = 10;
    CreateLabel(g_state.hFanPage, "Mode:", 10, y, 50, 20);
    g_state.hFanMode = CreateCombo(g_state.hFanPage, ID_FAN_MODE, 70, y, 100, 200);
    SendMessage(g_state.hFanMode, CB_ADDSTRING, 0, (LPARAM)"Off");
    SendMessage(g_state.hFanMode, CB_ADDSTRING, 0, (LPARAM)"Static");
    SendMessage(g_state.hFanMode, CB_ADDSTRING, 0, (LPARAM)"Breathe");
    SendMessage(g_state.hFanMode, CB_SETCURSEL, 1, 0);

    y += 30;
    CreateLabel(g_state.hFanPage, "Color:", 10, y, 50, 20);
    g_state.hFanColor = CreateColorBtn(g_state.hFanPage, ID_FAN_COLOR, 70, y, 40, 20);
    g_state.fanR = 255; g_state.fanG = 165; g_state.fanB = 0;

    y += 30;
    CreateLabel(g_state.hFanPage, "Brightness (1-5):", 10, y, 100, 20);
    g_state.hFanBright = CreateEdit(g_state.hFanPage, ID_FAN_BRIGHTNESS, 120, y, 40, 20);

    y += 30;
    CreateLabel(g_state.hFanPage, "Speed (1-5):", 10, y, 100, 20);
    g_state.hFanSpeed = CreateEdit(g_state.hFanPage, ID_FAN_SPEED, 120, y, 40, 20);

    y += 30;
    CreateLabel(g_state.hFanPage, "Mirage R freq:", 10, y, 100, 20);
    g_state.hFanMirageR = CreateEdit(g_state.hFanPage, ID_FAN_MIRAGE_R, 120, y, 60, 20);

    y += 25;
    CreateLabel(g_state.hFanPage, "Mirage G freq:", 10, y, 100, 20);
    g_state.hFanMirageG = CreateEdit(g_state.hFanPage, ID_FAN_MIRAGE_G, 120, y, 60, 20);

    y += 25;
    CreateLabel(g_state.hFanPage, "Mirage B freq:", 10, y, 100, 20);
    g_state.hFanMirageB = CreateEdit(g_state.hFanPage, ID_FAN_MIRAGE_B, 120, y, 60, 20);
}

static void CreateRingPage(HWND hTab) {
    RECT rc;
    GetClientRect(hTab, &rc);
    TabCtrl_AdjustRect(hTab, FALSE, &rc);

    g_state.hRingPage = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,
                                      rc.left, rc.top,
                                      rc.right - rc.left, rc.bottom - rc.top,
                                      hTab, NULL, GetModuleHandle(NULL), NULL);

    int y = 10;
    CreateLabel(g_state.hRingPage, "Mode:", 10, y, 50, 20);
    g_state.hRingMode = CreateCombo(g_state.hRingPage, ID_RING_MODE, 70, y, 100, 200);
    SendMessage(g_state.hRingMode, CB_ADDSTRING, 0, (LPARAM)"Off");
    SendMessage(g_state.hRingMode, CB_ADDSTRING, 0, (LPARAM)"Static");
    SendMessage(g_state.hRingMode, CB_ADDSTRING, 0, (LPARAM)"Breathe");
    SendMessage(g_state.hRingMode, CB_ADDSTRING, 0, (LPARAM)"Swirl");
    SendMessage(g_state.hRingMode, CB_ADDSTRING, 0, (LPARAM)"Rainbow");
    SendMessage(g_state.hRingMode, CB_ADDSTRING, 0, (LPARAM)"Cycle");
    SendMessage(g_state.hRingMode, CB_SETCURSEL, 1, 0);

    y += 30;
    CreateLabel(g_state.hRingPage, "Color:", 10, y, 50, 20);
    g_state.hRingColor = CreateColorBtn(g_state.hRingPage, ID_RING_COLOR, 70, y, 40, 20);
    g_state.ringR = 0; g_state.ringG = 255; g_state.ringB = 0;

    y += 30;
    CreateLabel(g_state.hRingPage, "Brightness (1-5):", 10, y, 100, 20);
    g_state.hRingBright = CreateEdit(g_state.hRingPage, ID_RING_BRIGHTNESS, 120, y, 40, 20);

    y += 30;
    CreateLabel(g_state.hRingPage, "Speed (1-5):", 10, y, 100, 20);
    g_state.hRingSpeed = CreateEdit(g_state.hRingPage, ID_RING_SPEED, 120, y, 40, 20);
}

// ---------------------------------------------------------------------------
// Color picker window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK ColorPickerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ColorPickerData* data = (ColorPickerData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        data = (ColorPickerData*)cs->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)data);

        // RGB sliders
        CreateLabel(hWnd, "R:", 10, 10, 20, 20);
        data->hRSlider = CreateWindow(TRACKBAR_CLASS, "", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                                       35, 10, 180, 25, hWnd, (HMENU)ID_CP_R_SLIDER, cs->hInstance, NULL);
        SendMessage(data->hRSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(data->hRSlider, TBM_SETPOS, TRUE, data->r);
        data->hR = CreateEdit(hWnd, ID_CP_R_EDIT, 225, 10, 40, 20);

        CreateLabel(hWnd, "G:", 10, 40, 20, 20);
        data->hGSlider = CreateWindow(TRACKBAR_CLASS, "", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                                       35, 40, 180, 25, hWnd, (HMENU)ID_CP_G_SLIDER, cs->hInstance, NULL);
        SendMessage(data->hGSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(data->hGSlider, TBM_SETPOS, TRUE, data->g);
        data->hG = CreateEdit(hWnd, ID_CP_G_EDIT, 225, 40, 40, 20);

        CreateLabel(hWnd, "B:", 10, 70, 20, 20);
        data->hBSlider = CreateWindow(TRACKBAR_CLASS, "", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                                       35, 70, 180, 25, hWnd, (HMENU)ID_CP_B_SLIDER, cs->hInstance, NULL);
        SendMessage(data->hBSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(data->hBSlider, TBM_SETPOS, TRUE, data->b);
        data->hB = CreateEdit(hWnd, ID_CP_B_EDIT, 225, 70, 40, 20);

        // Preview
        CreateLabel(hWnd, "Preview:", 10, 100, 50, 20);
        data->hPreview = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
                                       70, 100, 60, 30, hWnd, (HMENU)ID_CP_PREVIEW, cs->hInstance, NULL);

        // OK / Cancel
        CreateButton(hWnd, "OK",     ID_CP_OK,     50,  140, 60, 25);
        CreateButton(hWnd, "Cancel", ID_CP_CANCEL, 130, 140, 60, 25);

        // Update edit fields
        char buf[16];
        sprintf_s(buf, "%d", data->r); SetWindowText(data->hR, buf);
        sprintf_s(buf, "%d", data->g); SetWindowText(data->hG, buf);
        sprintf_s(buf, "%d", data->b); SetWindowText(data->hB, buf);

        return 0;
    }

    case WM_HSCROLL: {
        if (!data) break;
        data->r = (int)SendMessage(data->hRSlider, TBM_GETPOS, 0, 0);
        data->g = (int)SendMessage(data->hGSlider, TBM_GETPOS, 0, 0);
        data->b = (int)SendMessage(data->hBSlider, TBM_GETPOS, 0, 0);

        char buf[16];
        sprintf_s(buf, "%d", data->r); SetWindowText(data->hR, buf);
        sprintf_s(buf, "%d", data->g); SetWindowText(data->hG, buf);
        sprintf_s(buf, "%d", data->b); SetWindowText(data->hB, buf);

        InvalidateRect(data->hPreview, NULL, TRUE);
        return 0;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == ID_CP_PREVIEW && data) {
            HBRUSH br = CreateSolidBrush(RGB(data->r, data->g, data->b));
            FillRect(dis->hDC, &dis->rcItem, br);
            DeleteObject(br);
            FrameRect(dis->hDC, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        return 0;
    }

    case WM_COMMAND: {
        if (!data) break;
        switch (LOWORD(wParam)) {
        case ID_CP_OK:
            if (data->outR) *data->outR = data->r;
            if (data->outG) *data->outG = data->g;
            if (data->outB) *data->outB = data->b;
            DestroyWindow(hWnd);
            return 0;
        case ID_CP_CANCEL:
            DestroyWindow(hWnd);
            return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowColorPicker(HWND hParent, int& r, int& g, int& b) {
    const char CLASS_NAME[] = "CMRGB_ColorPicker";

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = ColorPickerWndProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    ColorPickerData data;
    data.r = r; data.g = g; data.b = b;
    data.outR = &r; data.outG = &g; data.outB = &b;

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, "Pick Color",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                CW_USEDEFAULT, CW_USEDEFAULT, 290, 210,
                                hParent, NULL, GetModuleHandle(NULL), &data);

    if (hWnd) {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        // Modal loop
        MSG msg;
        while (IsWindow(hWnd) && GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Get integer from edit control
// ---------------------------------------------------------------------------
static int GetEditInt(HWND hEdit, int def) {
    char buf[64];
    GetWindowText(hEdit, buf, sizeof(buf));
    return atoi(buf);
}

// ---------------------------------------------------------------------------
// Apply / Disable handlers
// ---------------------------------------------------------------------------
static void OnApply(HWND hWnd) {
    CMRGBController ctrl;
    if (!ctrl.Open()) {
        MessageBox(hWnd, "Failed to open Wraith Prism device.\n\n"
                   "Make sure the cooler is connected and no other\n"
                   "program is using the device.", "Error", MB_ICONERROR);
        return;
    }

    BYTE logoChannel = CMRGBController::CH_OFF;
    BYTE fanChannel  = CMRGBController::CH_OFF;
    BYTE ringChannels[15];
    for (int i = 0; i < 15; ++i) ringChannels[i] = CMRGBController::CH_OFF;
    int mirageHz[3] = {0, 0, 0};

    // Logo
    int logoMode = (int)SendMessage(g_state.hLogoMode, CB_GETCURSEL, 0, 0);
    if (logoMode > 0) {
        int bright = GetEditInt(g_state.hLogoBright, 3);
        int speed  = GetEditInt(g_state.hLogoSpeed, 3);
        BYTE modeVal = (logoMode == 2) ? CMRGBController::MODE_BREATHE
                                        : CMRGBController::MODE_STATIC;
        const char* modeStr = (logoMode == 2) ? "breathe" : "static";
        ctrl.SetChannel(CMRGBController::CH_LOGO, modeVal,
                        CMRGBController::BrightnessToByte(modeStr, bright),
                        g_state.logoR, g_state.logoG, g_state.logoB,
                        CMRGBController::SpeedToByte(modeStr, speed));
        logoChannel = CMRGBController::CH_LOGO;
    }

    // Fan
    int fanMode = (int)SendMessage(g_state.hFanMode, CB_GETCURSEL, 0, 0);
    if (fanMode > 0) {
        int bright = GetEditInt(g_state.hFanBright, 3);
        int speed  = GetEditInt(g_state.hFanSpeed, 3);
        BYTE modeVal = (fanMode == 2) ? CMRGBController::MODE_BREATHE
                                        : CMRGBController::MODE_STATIC;
        const char* modeStr = (fanMode == 2) ? "breathe" : "static";
        ctrl.SetChannel(CMRGBController::CH_FAN, modeVal,
                        CMRGBController::BrightnessToByte(modeStr, bright),
                        g_state.fanR, g_state.fanG, g_state.fanB,
                        CMRGBController::SpeedToByte(modeStr, speed));
        fanChannel = CMRGBController::CH_FAN;
        mirageHz[0] = GetEditInt(g_state.hFanMirageR, 0);
        mirageHz[1] = GetEditInt(g_state.hFanMirageG, 0);
        mirageHz[2] = GetEditInt(g_state.hFanMirageB, 0);
    }

    // Ring
    int ringMode = (int)SendMessage(g_state.hRingMode, CB_GETCURSEL, 0, 0);
    if (ringMode > 0) {
        int bright = GetEditInt(g_state.hRingBright, 3);
        int speed  = GetEditInt(g_state.hRingSpeed, 3);

        static const char* ringModeStrs[] = {
            "", "static", "breathe", "swirl", "rainbow", "cycle"
        };
        static const BYTE ringChannelMap[] = {
            0, // Off (unused)
            CMRGBController::CH_R_STATIC,
            CMRGBController::CH_R_BREATHE,
            CMRGBController::CH_R_SWIRL,
            CMRGBController::CH_R_RAINBOW,
            CMRGBController::CH_R_CYCLE
        };
        static const BYTE ringModeMap[] = {
            0,
            CMRGBController::MODE_R_DEFAULT,
            CMRGBController::MODE_R_DEFAULT,
            CMRGBController::MODE_R_DEFAULT,
            CMRGBController::MODE_R_RAINBOW,
            CMRGBController::MODE_CYCLE
        };

        BYTE ch = ringChannelMap[ringMode];
        BYTE md = ringModeMap[ringMode];
        ctrl.SetChannel(ch, md,
                        CMRGBController::BrightnessToByte(ringModeStrs[ringMode], bright),
                        g_state.ringR, g_state.ringG, g_state.ringB,
                        CMRGBController::SpeedToByte(ringModeStrs[ringMode], speed));
        for (int i = 0; i < 15; ++i) ringChannels[i] = ch;
    }

    ctrl.EnableMirage(mirageHz[0], mirageHz[1], mirageHz[2]);
    ctrl.AssignLEDsToChannels(logoChannel, fanChannel, ringChannels, 15);
    ctrl.Apply();
}

static void OnDisable(HWND hWnd) {
    CMRGBController ctrl;
    if (!ctrl.Open()) {
        MessageBox(hWnd, "Failed to open Wraith Prism device.", "Error", MB_ICONERROR);
        return;
    }
    BYTE off = CMRGBController::CH_OFF;
    BYTE ring[15];
    for (int i = 0; i < 15; ++i) ring[i] = off;
    ctrl.AssignLEDsToChannels(off, off, ring, 15);
    ctrl.Apply();
}

static void OnRestore(HWND hWnd) {
    CMRGBController ctrl;
    if (!ctrl.Open()) {
        MessageBox(hWnd, "Failed to open Wraith Prism device.", "Error", MB_ICONERROR);
        return;
    }
    ctrl.Restore();
}

// ---------------------------------------------------------------------------
// Main window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Init common controls
        INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES};
        InitCommonControlsEx(&icex);

        // Tab control
        g_state.hTab = CreateWindow(WC_TABCONTROL, "", WS_VISIBLE | WS_CHILD | TCS_FIXEDWIDTH,
                                    10, 10, 360, 240, hWnd, (HMENU)ID_TAB,
                                    GetModuleHandle(NULL), NULL);

        TCITEM tie = {0};
        tie.mask = TCIF_TEXT;
        tie.pszText = "Logo";
        TabCtrl_InsertItem(g_state.hTab, 0, &tie);
        tie.pszText = "Fan";
        TabCtrl_InsertItem(g_state.hTab, 1, &tie);
        tie.pszText = "Ring";
        TabCtrl_InsertItem(g_state.hTab, 2, &tie);

        CreateLogoPage(g_state.hTab);
        CreateFanPage(g_state.hTab);
        CreateRingPage(g_state.hTab);

        // Buttons
        g_state.hApply   = CreateButton(hWnd, "Apply",   ID_APPLY,   50,  260, 80, 30);
        g_state.hDisable = CreateButton(hWnd, "Disable", ID_DISABLE, 140, 260, 80, 30);
        g_state.hRestore = CreateButton(hWnd, "Restore", ID_RESTORE, 230, 260, 80, 30);

        // Show first page, hide others
        ShowWindow(g_state.hLogoPage, SW_SHOW);
        ShowWindow(g_state.hFanPage,  SW_HIDE);
        ShowWindow(g_state.hRingPage, SW_HIDE);
        return 0;
    }

    case WM_NOTIFY: {
        if (((LPNMHDR)lParam)->idFrom == ID_TAB &&
            ((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_state.hTab);

            // Hide all page containers
            ShowWindow(g_state.hLogoPage, SW_HIDE);
            ShowWindow(g_state.hFanPage,  SW_HIDE);
            ShowWindow(g_state.hRingPage, SW_HIDE);

            switch (sel) {
            case 0: ShowWindow(g_state.hLogoPage, SW_SHOW); break;
            case 1: ShowWindow(g_state.hFanPage,  SW_SHOW); break;
            case 2: ShowWindow(g_state.hRingPage, SW_SHOW); break;
            }
        }
        return 0;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_BUTTON) {
            int r = 0, g = 0, b = 0;
            if (dis->CtlID == ID_LOGO_COLOR) {
                r = g_state.logoR; g = g_state.logoG; b = g_state.logoB;
            } else if (dis->CtlID == ID_FAN_COLOR) {
                r = g_state.fanR; g = g_state.fanG; b = g_state.fanB;
            } else if (dis->CtlID == ID_RING_COLOR) {
                r = g_state.ringR; g = g_state.ringG; b = g_state.ringB;
            }
            HBRUSH br = CreateSolidBrush(RGB(r, g, b));
            FillRect(dis->hDC, &dis->rcItem, br);
            DeleteObject(br);
            FrameRect(dis->hDC, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_LOGO_COLOR:
            if (HIWORD(wParam) == BN_CLICKED)
                ShowColorPicker(hWnd, g_state.logoR, g_state.logoG, g_state.logoB);
            break;
        case ID_FAN_COLOR:
            if (HIWORD(wParam) == BN_CLICKED)
                ShowColorPicker(hWnd, g_state.fanR, g_state.fanG, g_state.fanB);
            break;
        case ID_RING_COLOR:
            if (HIWORD(wParam) == BN_CLICKED)
                ShowColorPicker(hWnd, g_state.ringR, g_state.ringG, g_state.ringB);
            break;
        case ID_APPLY:
            OnApply(hWnd);
            break;
        case ID_DISABLE:
            OnDisable(hWnd);
            break;
        case ID_RESTORE:
            OnRestore(hWnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
int RunGUI(HINSTANCE hInstance) {
    const char CLASS_NAME[] = "CMRGBWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window registration failed", "Error", MB_ICONERROR);
        return 1;
    }

    HWND hWnd = CreateWindowEx(
        0, CLASS_NAME, "CM RGB Controller",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 340,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBox(NULL, "Window creation failed", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
