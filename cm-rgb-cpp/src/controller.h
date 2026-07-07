#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

class CMRGBController {
public:
    static const WORD VENDOR_ID = 0x2516;
    static const WORD PRODUCT_ID = 0x0051;
    static const BYTE IFACE_NUM = 1;

    enum LedChannel : BYTE {
        CH_R_STATIC  = 0x00,
        CH_R_BREATHE = 0x01,
        CH_R_CYCLE   = 0x02,
        CH_LOGO      = 0x05,
        CH_FAN       = 0x06,
        CH_R_RAINBOW = 0x07,
        CH_R_SWIRL   = 0x0A,
        CH_OFF       = 0xFE
    };

    enum LedMode : BYTE {
        MODE_OFF       = 0x00,
        MODE_STATIC    = 0x01,
        MODE_CYCLE     = 0x02,
        MODE_BREATHE   = 0x03,
        MODE_R_RAINBOW = 0x05,
        MODE_R_SWIRL   = 0x4A,
        MODE_R_DEFAULT = 0xFF
    };

    CMRGBController();
    ~CMRGBController();

    bool Open();
    void Close();
    bool IsOpen() const { return m_hDevice != INVALID_HANDLE_VALUE; }

    // High-level commands
    void PowerOn();
    void PowerOff();
    void Restore();
    void Apply();
    void Save();
    void Load();

    void SetChannel(BYTE channel, BYTE mode, BYTE brightness,
                    BYTE r, BYTE g, BYTE b,
                    BYTE speed = 0xFF, BYTE colorSource = 0x20);
    void AssignLEDsToChannels(BYTE logo, BYTE fan,
                              const BYTE* ring, int ringCount);
    void EnableMirage(int rHz, int gHz, int bHz);
    void DisableMirage();
    std::string GetVersion();

    // Conversion helpers (public - used by CLI, GUI, and Monitor)
    static BYTE BrightnessToByte(const char* mode, int brightness);
    static BYTE SpeedToByte(const char* mode, int speed);

    // Packet structure for HID communication
    struct Packet {
        BYTE data[65];
        Packet(BYTE fill = 0);
        void SetArgs(const std::vector<BYTE>& args);
    };

private:
    HANDLE m_hDevice;

    static const int PACKET_SIZE = 65;
    static const int READ_SIZE   = 64;

    std::vector<BYTE> SendPacket(const Packet& pkt);
    bool FindDevice();

    static void HzToBytes(int hz, BYTE out[3]);
};
