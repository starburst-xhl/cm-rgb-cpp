# cm-rgb-cpp

> Forked from [gfduszynski/cm-rgb](https://github.com/gfduszynski/cm-rgb) — original Python project for Linux/macOS.
>
> 本 Fork 的目的是将原项目移植到 **C++**，使其能在 **Windows XP** 及更高版本上以单文件零依赖的方式运行。

Control your AMD Wraith Prism RGB on Windows XP+, Linux and macOS.

---

## Project Structure

```
cm-rgb-cpp/
├── python/                  # Original Python code (from upstream)
│   ├── cm_rgb/              # Python package
│   │   ├── __init__.py
│   │   └── ctrl.py
│   ├── scripts/             # Python scripts
│   │   ├── cm-rgb-cli
│   │   ├── cm-rgb-gui
│   │   └── cm-rgb-monitor
│   └── setup.py
├── cm-rgb-cpp/              # C++ port (this project)
│   ├── CMakeLists.txt       # CMake build
│   ├── build.bat            # Direct build script
│   └── src/
│       ├── main.cpp         # Entry point + CLI
│       ├── controller.h/cpp # HID controller (Win32 API)
│       ├── gui.h/cpp        # Win32 GUI (no dependencies)
│       └── monitor.h/cpp    # CPU monitor (WMI + Win32)
├── .gitignore
├── LICENSE
└── README.md
```

## Build

### Requirements

- Visual Studio C++ compiler (2008 or later)
- Windows SDK (included with Visual Studio)

### Build with build.bat

```cmd
cd cm-rgb-cpp
build              # Release build
build debug        # Debug build
build clean        # Clean artifacts
```

### Build with CMake

```cmd
cd cm-rgb-cpp
cmake -B build
cmake --build build
```

## Usage

### CLI

```
cm-rgb-cpp set logo --color=#00FFFF --mode=static --brightness=3
cm-rgb-cpp set fan  --color=#FF0000 --mode=breathe --brightness=3 --speed=3
cm-rgb-cpp set ring --color=#00FF00 --mode=swirl  --brightness=3 --speed=3
cm-rgb-cpp set save
cm-rgb-cpp restore
cm-rgb-cpp version
```

### GUI

```
cm-rgb-cpp gui
```

### Monitor

```
cm-rgb-cpp monitor [--show-temp] [--show-cpu-frequency]
```

## Features

- **CLI** — Full control of all LED zones (logo, fan, ring)
- **GUI** — Native Win32 window with tabbed interface and color picker
- **Monitor** — Real-time CPU utilization display on ring LEDs, temperature on fan LED, frequency on logo LED
- **Zero dependencies** — Single `.exe` file, no runtime required
- **Windows XP+** — Compatible with Windows XP and all later versions

## Upstream

The original Python project by [gfduszynski](https://github.com/gfduszynski/cm-rgb) supports Linux and macOS.

```bash
pip install cm-rgb
```

## License

MIT
