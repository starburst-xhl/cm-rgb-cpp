#include "controller.h"
#include <hidsdi.h>
#include <setupapi.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

// ---------------------------------------------------------------------------
// Packet
// ---------------------------------------------------------------------------
CMRGBController::Packet::Packet(BYTE fill) {
    memset(data, fill, PACKET_SIZE);
    data[0] = 0; // Report ID
}

void CMRGBController::Packet::SetArgs(const std::vector<BYTE>& args) {
    for (size_t i = 0; i < args.size() && i < PACKET_SIZE - 1; ++i)
        data[i + 1] = args[i];
}

// ---------------------------------------------------------------------------
// Static packet helpers
// ---------------------------------------------------------------------------
static CMRGBController::Packet MakePacket(const std::vector<BYTE>& args) {
    CMRGBController::Packet pkt(0);
    pkt.SetArgs(args);
    return pkt;
}

#define P_POWER_ON   MakePacket({0x41, 0x80})
#define P_POWER_OFF  MakePacket({0x41, 0x03})
#define P_RESTORE    MakePacket({0x41})
#define P_LED_LOAD   MakePacket({0x50})
#define P_LED_SAVE   MakePacket({0x50, 0x55})
#define P_APPLY      MakePacket({0x51, 0x28, 0x00, 0x00, 0xe0})
#define P_MAGIC_2    MakePacket({0x51, 0x96})
#define P_GET_VER    MakePacket({0x12, 0x20})

// ---------------------------------------------------------------------------
// CMRGBController
// ---------------------------------------------------------------------------
CMRGBController::CMRGBController()
    : m_hDevice(INVALID_HANDLE_VALUE) {
}

CMRGBController::~CMRGBController() {
    Close();
}

bool CMRGBController::FindDevice() {
    // Enumerate HID devices matching our VID/PID
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(
        &hidGuid, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (devInfo == INVALID_HANDLE_VALUE)
        return false;

    SP_DEVICE_INTERFACE_DATA devInterfaceData = {0};
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bool found = false;

    for (DWORD i = 0; ; ++i) {
        if (!SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, i, &devInterfaceData))
            break;

        // Get required size
        DWORD reqSize = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData, NULL, 0, &reqSize, NULL);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            continue;

        std::vector<BYTE> buffer(reqSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(buffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData,
                                              detailData, reqSize, NULL, NULL))
            continue;

        // Open the device to check VID/PID
        HANDLE hDev = CreateFile(
            detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);

        if (hDev == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attrib = {0};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(hDev, &attrib)) {
            if (attrib.VendorID == VENDOR_ID && attrib.ProductID == PRODUCT_ID) {
                // Check interface number
                // For simplicity, we just take the first match
                m_hDevice = hDev;
                found = true;
                break;
            }
        }

        CloseHandle(hDev);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return found;
}

bool CMRGBController::Open() {
    if (IsOpen())
        return true;

    if (!FindDevice()) {
        std::cerr << "No Wraith Prism device found." << std::endl;
        return false;
    }

    // Initialize controller
    PowerOn();
    SendPacket(P_MAGIC_2);
    Apply();

    return true;
}

void CMRGBController::Close() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

std::vector<BYTE> CMRGBController::SendPacket(const Packet& pkt) {
    if (!IsOpen())
        return {};

    DWORD written = 0;
    if (!WriteFile(m_hDevice, pkt.data, PACKET_SIZE, &written, NULL)) {
        std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
        return {};
    }

    BYTE buf[READ_SIZE] = {0};
    DWORD read = 0;
    if (!ReadFile(m_hDevice, buf, READ_SIZE, &read, NULL)) {
        std::cerr << "ReadFile failed: " << GetLastError() << std::endl;
        return {};
    }

    return std::vector<BYTE>(buf, buf + read);
}

// ---------------------------------------------------------------------------
// High-level commands
// ---------------------------------------------------------------------------
void CMRGBController::PowerOn()  { SendPacket(P_POWER_ON); }
void CMRGBController::PowerOff() { SendPacket(P_POWER_OFF); }
void CMRGBController::Apply()    { SendPacket(P_APPLY); }
void CMRGBController::Save()     { SendPacket(P_LED_SAVE); }
void CMRGBController::Load()     { SendPacket(P_LED_LOAD); }

void CMRGBController::Restore() {
    DisableMirage();
    Load();
    PowerOff();
    SendPacket(P_RESTORE);
    Apply();
}

void CMRGBController::SetChannel(BYTE channel, BYTE mode, BYTE brightness,
                                  BYTE r, BYTE g, BYTE b,
                                  BYTE speed, BYTE colorSource) {
    Packet pkt(0xFF);
    pkt.SetArgs({0x51, 0x2C, 0x01, 0x00, channel, speed, colorSource,
                 mode, 0xFF, brightness, r, g, b, 0x00, 0x00, 0x00});
    SendPacket(pkt);
}

void CMRGBController::AssignLEDsToChannels(BYTE logo, BYTE fan,
                                            const BYTE* ring, int ringCount) {
    Packet pkt(0x00);
    std::vector<BYTE> args = {0x51, 0xA0, 0x01, 0x00, 0x00, 0x03, 0x00, 0x00, logo, fan};
    for (int i = 0; i < 15; ++i) {
        if (i < ringCount)
            args.push_back(ring[i]);
        else
            args.push_back(CH_OFF);
    }
    pkt.SetArgs(args);
    SendPacket(pkt);
}

void CMRGBController::HzToBytes(int hz, BYTE out[3]) {
    if (hz == 0) {
        out[0] = 0x00;
        out[1] = 0xFF;
        out[2] = 0x4A;
        return;
    }
    double v = 187498.0 / hz;
    int vMul = static_cast<int>(std::floor(v / 256.0));
    double vRem = v / (vMul + 1);
    out[0] = static_cast<BYTE>(std::min(vMul, 255));
    out[1] = static_cast<BYTE>(std::floor(fmod(vRem, 1.0) * 256));
    out[2] = static_cast<BYTE>(std::floor(vRem));
}

void CMRGBController::EnableMirage(int rHz, int gHz, int bHz) {
    BYTE rBytes[3], gBytes[3], bBytes[3];
    HzToBytes(rHz, rBytes);
    HzToBytes(gHz, gBytes);
    HzToBytes(bHz, bBytes);

    Packet pkt(0);
    pkt.SetArgs({
        0x51, 0x71, 0x00, 0x00,
        0x01, 0x00, 0xFF, 0x4A,
        0x02, rBytes[0], rBytes[1], rBytes[2],
        0x03, gBytes[0], gBytes[1], gBytes[2],
        0x04, bBytes[0], bBytes[1], bBytes[2]
    });
    SendPacket(pkt);
}

void CMRGBController::DisableMirage() {
    EnableMirage(0, 0, 0);
}

// ---------------------------------------------------------------------------
// Brightness and speed conversion tables
// ---------------------------------------------------------------------------
BYTE CMRGBController::BrightnessToByte(const char* mode, int brightness) {
    int idx = std::max(1, std::min(5, brightness)) - 1;
    if (strcmp(mode, "cycle") == 0) {
        static const BYTE table[] = {0x10, 0x20, 0x40, 0x60, 0x7F};
        return table[idx];
    } else if (strcmp(mode, "rainbow") == 0) {
        static const BYTE table[] = {0x16, 0x33, 0x66, 0x88, 0xFF};
        return table[idx];
    } else {
        static const BYTE table[] = {0x33, 0x66, 0x99, 0xCC, 0xFF};
        return table[idx];
    }
}

BYTE CMRGBController::SpeedToByte(const char* mode, int speed) {
    int idx = std::max(1, std::min(5, speed)) - 1;
    if (strcmp(mode, "cycle") == 0 || strcmp(mode, "rainbow") == 0) {
        static const BYTE table[] = {0x72, 0x68, 0x64, 0x62, 0x61};
        return table[idx];
    } else if (strcmp(mode, "swirl") == 0) {
        static const BYTE table[] = {0x90, 0x85, 0x77, 0x74, 0x70};
        return table[idx];
    } else {
        static const BYTE table[] = {0x3C, 0x34, 0x2C, 0x20, 0x18};
        return table[idx];
    }
}

std::string CMRGBController::GetVersion() {
    auto reply = SendPacket(P_GET_VER);
    if (reply.empty())
        return "unknown";

    // Reply contains version string as 16-bit (UTF-16LE) characters starting at offset 0x08
    // Low byte at even offsets, high byte at odd offsets (typically 0x00 for ASCII)
    char buf[16] = {0};
    int idx = 0;
    for (int i = 0; i < 16 && i + 9 < (int)reply.size(); i += 2) {
        // Combine low and high bytes into a 16-bit character
        wchar_t ch = (reply[i + 9] << 8) | reply[i + 8];
        if (ch == 0)
            break;
        // For ASCII-range characters, just take the low byte
        buf[idx++] = (char)reply[i + 8];
    }
    return std::string(buf);
}
