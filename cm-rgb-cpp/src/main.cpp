// cm-rgb - AMD Wraith Prism RGB controller for Windows XP+
// Single executable with CLI, GUI, and Monitor modes.
//
// Usage:
//   cm-rgb set logo --color=#00FFFF --mode=static --brightness=3
//   cm-rgb set fan  --color=#FF0000 --mode=breathe --brightness=3 --speed=3
//   cm-rgb set ring --color=#00FF00 --mode=swirl --brightness=3 --speed=3
//   cm-rgb set save
//   cm-rgb restore
//   cm-rgb version
//   cm-rgb gui
//   cm-rgb monitor [options]

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>

#include "controller.h"
#include "gui.h"
#include "monitor.h"

// ---------------------------------------------------------------------------
// Simple argument parser
// ---------------------------------------------------------------------------
struct Args {
    std::string program;
    std::vector<std::string> positional;
    std::vector<std::pair<std::string, std::string>> options;
};

static Args ParseArgs(int argc, char* argv[]) {
    Args a;
    if (argc > 0) a.program = argv[0];
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
            auto eq = arg.find('=');
            if (eq != std::string::npos) {
                a.options.push_back({arg.substr(2, eq - 2), arg.substr(eq + 1)});
            } else {
                a.options.push_back({arg.substr(2), ""});
            }
        } else {
            a.positional.push_back(arg);
        }
    }
    return a;
}

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

// ---------------------------------------------------------------------------
// Color parsing
// ---------------------------------------------------------------------------
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
// CLI mode tables (delegated to CMRGBController static methods)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// CLI: set command
// ---------------------------------------------------------------------------
static int CmdSet(CMRGBController& ctrl, const Args& args) {
    // We need at least one positional after "set"
    if (args.positional.size() < 2) {
        std::cerr << "Usage: cm-rgb set <logo|fan|ring> [options]" << std::endl;
        return 1;
    }

    // State
    BYTE logoChannel = CMRGBController::CH_OFF;
    BYTE fanChannel  = CMRGBController::CH_OFF;
    BYTE ringChannels[15];
    for (int i = 0; i < 15; ++i) ringChannels[i] = CMRGBController::CH_OFF;
    int mirageHz[3] = {0, 0, 0};
    bool doSave = false;

    // Process each subcommand
    for (size_t i = 1; i < args.positional.size(); ++i) {
        const std::string& sub = args.positional[i];

        if (sub == "save") {
            doSave = true;
            continue;
        }

        // Collect options for this subcommand
        // Options are passed before the next positional
        std::string mode = GetOpt(args, "mode", "static");
        std::string color = GetOpt(args, "color", "#00FFFF");
        int brightness = GetOptInt(args, "brightness", 3);
        int speed = GetOptInt(args, "speed", 3);
        int mirageR = GetOptInt(args, "mirage-red-freq", 0);
        int mirageG = GetOptInt(args, "mirage-green-freq", 0);
        int mirageB = GetOptInt(args, "mirage-blue-freq", 0);

        BYTE r, g, b;
        if (!ParseColor(color, r, g, b)) {
            std::cerr << "Invalid color: " << color << std::endl;
            return 1;
        }

        if (sub == "logo") {
            if (mode == "off") {
                logoChannel = CMRGBController::CH_OFF;
            } else {
                BYTE modeVal = (mode == "breathe") ? CMRGBController::MODE_BREATHE
                                                    : CMRGBController::MODE_STATIC;
                ctrl.SetChannel(CMRGBController::CH_LOGO, modeVal,
                                CMRGBController::BrightnessToByte(mode.c_str(), brightness),
                                r, g, b, CMRGBController::SpeedToByte(mode.c_str(), speed));
                logoChannel = CMRGBController::CH_LOGO;
            }
        } else if (sub == "fan") {
            if (mode == "off") {
                fanChannel = CMRGBController::CH_OFF;
            } else {
                BYTE modeVal = (mode == "breathe") ? CMRGBController::MODE_BREATHE
                                                    : CMRGBController::MODE_STATIC;
                ctrl.SetChannel(CMRGBController::CH_FAN, modeVal,
                                CMRGBController::BrightnessToByte(mode.c_str(), brightness),
                                r, g, b, CMRGBController::SpeedToByte(mode.c_str(), speed));
                fanChannel = CMRGBController::CH_FAN;
                mirageHz[0] = mirageR;
                mirageHz[1] = mirageG;
                mirageHz[2] = mirageB;
            }
        } else if (sub == "ring") {
            if (mode == "off") {
                for (int j = 0; j < 15; ++j) ringChannels[j] = CMRGBController::CH_OFF;
            } else {
                BYTE channel, modeVal;
                if (mode == "cycle") {
                    channel = CMRGBController::CH_R_CYCLE;
                    modeVal = CMRGBController::MODE_CYCLE;
                } else if (mode == "rainbow") {
                    channel = CMRGBController::CH_R_RAINBOW;
                    modeVal = CMRGBController::MODE_R_RAINBOW;
                } else if (mode == "static") {
                    channel = CMRGBController::CH_R_STATIC;
                    modeVal = CMRGBController::MODE_R_DEFAULT;
                } else if (mode == "breathe") {
                    channel = CMRGBController::CH_R_BREATHE;
                    modeVal = CMRGBController::MODE_R_DEFAULT;
                } else if (mode == "swirl") {
                    channel = CMRGBController::CH_R_SWIRL;
                    modeVal = CMRGBController::MODE_R_DEFAULT;
                } else {
                    std::cerr << "Unknown ring mode: " << mode << std::endl;
                    return 1;
                }
                ctrl.SetChannel(channel, modeVal,
                                CMRGBController::BrightnessToByte(mode.c_str(), brightness),
                                r, g, b, CMRGBController::SpeedToByte(mode.c_str(), speed));
                for (int j = 0; j < 15; ++j) ringChannels[j] = channel;
            }
        } else {
            std::cerr << "Unknown subcommand: " << sub << std::endl;
            return 1;
        }
    }

    // Apply
    ctrl.EnableMirage(mirageHz[0], mirageHz[1], mirageHz[2]);
    ctrl.AssignLEDsToChannels(logoChannel, fanChannel, ringChannels, 15);
    ctrl.Apply();

    if (doSave)
        ctrl.Save();

    return 0;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    Args args = ParseArgs(argc, argv);

    if (args.positional.empty()) {
        // No arguments: launch GUI
        return RunGUI();
    }

    const std::string& cmd = args.positional[0];

    if (cmd == "gui") {
        return RunGUI();
    } else if (cmd == "monitor") {
        return RunMonitor(args);
    } else if (cmd == "restore") {
        CMRGBController ctrl;
        if (!ctrl.Open()) return 1;
        ctrl.Restore();
        return 0;
    } else if (cmd == "version") {
        CMRGBController ctrl;
        if (!ctrl.Open()) return 1;
        std::cout << ctrl.GetVersion() << std::endl;
        return 0;
    } else if (cmd == "set") {
        CMRGBController ctrl;
        if (!ctrl.Open()) return 1;
        return CmdSet(ctrl, args);
    } else {
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  cm-rgb set <logo|fan|ring> [options]" << std::endl;
        std::cerr << "  cm-rgb restore" << std::endl;
        std::cerr << "  cm-rgb version" << std::endl;
        std::cerr << "  cm-rgb gui" << std::endl;
        std::cerr << "  cm-rgb monitor [options]" << std::endl;
        return 1;
    }
}
