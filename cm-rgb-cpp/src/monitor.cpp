// Monitor mode - displays CPU utilization on ring LEDs
// Uses Win32 API only (XP compatible)
// CPU % via GetSystemTimes
// Temperature via WMI (MSAcpi_ThermalZoneTemperature)
// CPU frequency via registry

#include "monitor.h"
#include "controller.h"
#include <windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// ---------------------------------------------------------------------------
// Argument struct (from main.cpp)
// ---------------------------------------------------------------------------
struct Args {
    std::string program;
    std::vector<std::string> positional;
    std::vector<std::pair<std::string, std::string>> options;
};

static std::string GetOpt(const Args& a, const char* name, const char* def = nullptr) {
    for (auto& opt : a.options) {
        if (opt.first == name) return opt.second;
    }
    return def ? std::string(def) : std::string();
}

static int GetOptInt(const Args& a, const char* name, int def) {
    std::string v = GetOpt(a, name);
    return v.empty() ? def : atoi(v.c_str());
}

static double GetOptDouble(const Args& a, const char* name, double def) {
    std::string v = GetOpt(a, name);
    return v.empty() ? def : atof(v.c_str());
}

static bool ParseColor(const std::string& hex, BYTE& r, BYTE& g, BYTE& b) {
    std::string h = hex;
    if (h.empty()) return false;
    if (h[0] == '#') h = h.substr(1);
    if (h.size() != 6) return false;
    unsigned int rgb;
    if (sscanf_s(h.c_str(), "%06x", &rgb) != 1) return false;
    r = (rgb >> 16) & 0xFF;
    g = (rgb >> 8) & 0xFF;
    b = rgb & 0xFF;
    return true;
}

// ---------------------------------------------------------------------------
// CPU utilization via GetSystemTimes (XP compatible)
// ---------------------------------------------------------------------------
class CPULoad {
public:
    CPULoad() : m_prevIdle(0), m_prevKernel(0), m_prevUser(0) {}

    double GetPercent() {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user))
            return 0.0;

        ULONGLONG idleNow   = FileTimeToUll(idle);
        ULONGLONG kernelNow = FileTimeToUll(kernel);
        ULONGLONG userNow   = FileTimeToUll(user);

        ULONGLONG totalNow = kernelNow + userNow;
        ULONGLONG totalPrev = m_prevKernel + m_prevUser;

        double percent = 0.0;
        if (totalNow > totalPrev) {
            ULONGLONG totalDiff = totalNow - totalPrev;
            ULONGLONG idleDiff  = idleNow - m_prevIdle;
            percent = 100.0 - (idleDiff * 100.0 / totalDiff);
        }

        m_prevIdle   = idleNow;
        m_prevKernel = kernelNow;
        m_prevUser   = userNow;

        return percent;
    }

private:
    ULONGLONG m_prevIdle, m_prevKernel, m_prevUser;

    static ULONGLONG FileTimeToUll(const FILETIME& ft) {
        return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    }
};

// ---------------------------------------------------------------------------
// CPU temperature via WMI
// ---------------------------------------------------------------------------
class CPUTemperature {
public:
    CPUTemperature() : m_initialized(false), m_pSvc(NULL) {}

    ~CPUTemperature() {
        if (m_pSvc) m_pSvc->Release();
        CoUninitialize();
    }

    bool Init() {
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hr)) return false;

        hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
                                  RPC_C_AUTHN_LEVEL_DEFAULT,
                                  RPC_C_IMP_LEVEL_IMPERSONATE,
                                  NULL, EOAC_NONE, NULL);
        if (FAILED(hr) && hr != RPC_E_TOO_LATE) return false;

        IWbemLocator* pLoc = NULL;
        hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                              IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hr)) return false;

        hr = pLoc->ConnectServer(_bstr_t(L"root\\wmi"), NULL, NULL, 0, 0, 0, 0, &m_pSvc);
        pLoc->Release();
        if (FAILED(hr)) return false;

        hr = CoSetProxyBlanket(m_pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                NULL, RPC_C_AUTHN_LEVEL_CALL,
                                RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        if (FAILED(hr)) return false;

        m_initialized = true;
        return true;
    }

    bool GetTemperature(double& celsius) {
        if (!m_initialized) return false;

        IEnumWbemClassObject* pEnum = NULL;
        HRESULT hr = m_pSvc->ExecQuery(
            _bstr_t("WQL"),
            _bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL, &pEnum);

        if (FAILED(hr)) return false;

        IWbemClassObject* pObj = NULL;
        ULONG ret = 0;
        hr = pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret);
        pEnum->Release();

        if (FAILED(hr) || ret == 0) return false;

        VARIANT vtProp;
        VariantInit(&vtProp);
        hr = pObj->Get(L"CurrentTemperature", 0, &vtProp, 0, 0);
        pObj->Release();

        if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
            // Temperature is in tenths of Kelvin
            celsius = (vtProp.lVal / 10.0) - 273.15;
            VariantClear(&vtProp);
            return true;
        }

        VariantClear(&vtProp);
        return false;
    }

private:
    bool m_initialized;
    IWbemServices* m_pSvc;
};

// ---------------------------------------------------------------------------
// CPU frequency via registry
// ---------------------------------------------------------------------------
static int GetCPUFrequencyMHz() {
    HKEY hKey;
    LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                             "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                             0, KEY_READ, &hKey);
    if (ret != ERROR_SUCCESS) return 0;

    DWORD mhz = 0;
    DWORD size = sizeof(mhz);
    ret = RegQueryValueEx(hKey, "~MHz", NULL, NULL, (LPBYTE)&mhz, &size);
    RegCloseKey(hKey);

    return (int)mhz;
}

// ---------------------------------------------------------------------------
// Color interpolation
// ---------------------------------------------------------------------------
static void InterpolateColor(double t,
                              BYTE r1, BYTE g1, BYTE b1,
                              BYTE r2, BYTE g2, BYTE b2,
                              BYTE& r, BYTE& g, BYTE& b) {
    t = std::max(0.0, std::min(1.0, t));
    r = (BYTE)(r1 * (1.0 - t) + r2 * t);
    g = (BYTE)(g1 * (1.0 - t) + g2 * t);
    b = (BYTE)(b1 * (1.0 - t) + b2 * t);
}

// ---------------------------------------------------------------------------
// Monitor main
// ---------------------------------------------------------------------------
int RunMonitor(Args& args) {
    // Parse options
    std::string bgColor   = GetOpt(args, "bg-color", "#00FFFF");
    std::string cpuColor  = GetOpt(args, "cpu-color", "#FFA500");
    int brightness        = GetOptInt(args, "brightness", 4);
    double interval       = GetOptDouble(args, "interval", 0.2);
    bool verbose          = !GetOpt(args, "verbose", "").empty();
    bool showTemp         = !GetOpt(args, "show-temp", "").empty();
    bool showFreq         = !GetOpt(args, "show-cpu-frequency", "").empty();
    double smoothing      = GetOptDouble(args, "smoothing", 0.8);

    std::string tLowColorStr  = GetOpt(args, "temp-low-color", "#00FFFF");
    std::string tHighColorStr = GetOpt(args, "temp-high-color", "#FFA500");
    std::string fLowColorStr  = GetOpt(args, "freq-low-color", "#00FFFF");
    std::string fHighColorStr = GetOpt(args, "freq-high-color", "#FFA500");

    double tempLow  = GetOptDouble(args, "temp-low", 50.0);
    double tempHigh = GetOptDouble(args, "temp-high", 80.0);

    bool mirage     = !GetOpt(args, "mirage", "").empty();
    std::string mirageFactorsStr = GetOpt(args, "mirage-factors", "7.0,7.0,7.0");

    // Parse colors
    BYTE bgR, bgG, bgB, cpuR, cpug, cpuB;
    if (!ParseColor(bgColor, bgR, bgG, bgB)) {
        std::cerr << "Invalid bg-color" << std::endl;
        return 1;
    }
    if (!ParseColor(cpuColor, cpuR, cpug, cpuB)) {
        std::cerr << "Invalid cpu-color" << std::endl;
        return 1;
    }

    BYTE tLowR, tLowG, tLowB, tHighR, tHighG, tHighB;
    BYTE fLowR, fLowG, fLowB, fHighR, fHighG, fHighB;
    ParseColor(tLowColorStr,  tLowR,  tLowG,  tLowB);
    ParseColor(tHighColorStr, tHighR, tHighG, tHighB);
    ParseColor(fLowColorStr,  fLowR,  fLowG,  fLowB);
    ParseColor(fHighColorStr, fHighR, fHighG, fHighB);

    // Mirage factors
    double mirageFactors[3] = {7.0, 7.0, 7.0};
    if (mirage) {
        sscanf_s(mirageFactorsStr.c_str(), "%lf,%lf,%lf",
                 &mirageFactors[0], &mirageFactors[1], &mirageFactors[2]);
        if (verbose) {
            std::cout << "Mirage factors: "
                      << mirageFactors[0] << ", "
                      << mirageFactors[1] << ", "
                      << mirageFactors[2] << std::endl;
        }
    }

    // Open controller
    CMRGBController ctrl;
    if (!ctrl.Open()) {
        std::cerr << "Failed to open Wraith Prism device." << std::endl;
        return 1;
    }

    // Setup background and CPU channels
    BYTE b = CMRGBController::BrightnessToByte("static", brightness);
    ctrl.SetChannel(CMRGBController::CH_R_STATIC, CMRGBController::MODE_R_DEFAULT,
                    b, bgR, bgG, bgB);
    ctrl.SetChannel(CMRGBController::CH_R_SWIRL, CMRGBController::MODE_R_DEFAULT,
                    b, cpuR, cpug, cpuB, 0x60);
    ctrl.Apply();

    // Initialize sensors
    CPULoad cpuLoad;
    CPUTemperature cpuTemp;
    if (showTemp) {
        if (!cpuTemp.Init()) {
            std::cerr << "WMI temperature sensor init failed." << std::endl;
            showTemp = false;
        }
    }

    int maxFreqMHz = GetCPUFrequencyMHz();
    if (verbose && maxFreqMHz > 0) {
        std::cout << "Max CPU frequency: " << maxFreqMHz << " MHz" << std::endl;
    }

    // Smoothing state
    double smoothedTemp = 45.0;
    double smoothedFreq = (double)maxFreqMHz;

    // Main loop
    while (true) {
        // CPU utilization
        double cpu = cpuLoad.GetPercent();
        int cpuLEDs = (int)round(cpu * 15.0 / 100.0);
        int bgLEDs = 15 - cpuLEDs;

        BYTE ring[15];
        int idx = 0;
        for (int i = 0; i < cpuLEDs; ++i)
            ring[idx++] = CMRGBController::CH_R_SWIRL;
        for (int i = 0; i < bgLEDs; ++i)
            ring[idx++] = CMRGBController::CH_R_STATIC;

        // Shift by -8 for visual alignment
        int shift = 8;
        BYTE shifted[15];
        for (int i = 0; i < 15; ++i)
            shifted[(i + shift) % 15] = ring[i];

        // Temperature on fan LED
        if (showTemp) {
            double t;
            if (cpuTemp.GetTemperature(t)) {
                smoothedTemp = smoothing * smoothedTemp + (1.0 - smoothing) * t;
                double interp = std::max(0.0, std::min(1.0, (smoothedTemp - tempLow) / (tempHigh - tempLow)));
                BYTE tr, tg, tb;
                InterpolateColor(interp, tLowR, tLowG, tLowB, tHighR, tHighG, tHighB, tr, tg, tb);
                ctrl.SetChannel(CMRGBController::CH_FAN, CMRGBController::MODE_STATIC,
                                b, tr, tg, tb);
                if (verbose)
                    std::cout << "Temp: " << t << " C" << std::endl;
            }
        }

        // CPU frequency on logo LED
        if (showFreq && maxFreqMHz > 0) {
            double freq = (double)GetCPUFrequencyMHz();
            smoothedFreq = smoothing * smoothedFreq + (1.0 - smoothing) * freq;
            double interp = std::max(0.0, std::min(1.0, smoothedFreq / maxFreqMHz));
            BYTE fr, fg, fb;
            InterpolateColor(interp, fLowR, fLowG, fLowB, fHighR, fHighG, fHighB, fr, fg, fb);
            ctrl.SetChannel(CMRGBController::CH_LOGO, CMRGBController::MODE_STATIC,
                            b, fr, fg, fb);
            if (verbose)
                std::cout << "Freq: " << (int)freq << " MHz" << std::endl;
        }

        // Mirage
        if (mirage) {
            // On XP, fan speed reading is limited. Use CPU load as proxy.
            int fanHz = (int)(cpu * mirageFactors[0]);
            int fanGz = (int)(cpu * mirageFactors[1]);
            int fanBz = (int)(cpu * mirageFactors[2]);
            ctrl.EnableMirage(fanHz, fanGz, fanBz);
        }

        // Apply
        ctrl.AssignLEDsToChannels(
            showFreq ? CMRGBController::CH_LOGO : CMRGBController::CH_OFF,
            showTemp ? CMRGBController::CH_FAN : CMRGBController::CH_OFF,
            shifted, 15);
        ctrl.Apply();

        if (verbose)
            std::cout << "CPU: " << cpu << "%  LEDs: " << cpuLEDs << std::endl;

        Sleep((DWORD)(interval * 1000));
    }

    return 0;
}
