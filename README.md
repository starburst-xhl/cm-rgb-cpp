# cm-rgb-cpp

> Forked from [gfduszynski/cm-rgb](https://github.com/gfduszynski/cm-rgb) — original Python project for Linux/macOS.
>
> 本 Fork 的目的是将原项目移植到 **C++**，使其能在 **Windows XP** 及更高版本上以单文件零依赖的方式运行。

Control your AMD Wraith Prism RGB on Windows XP+.

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

> **VS2022 用户注意**：需要额外安装 **"C++ Windows XP Support"** 组件。
> 打开 Visual Studio Installer → 修改 VS2022 → 单个组件 → 搜索 `v141_xp` →
> 勾选 **"C++ Windows XP Support for VS 2017 (v141) tools [Deprecated]"** 安装即可。
>
> **建议编译 32 位 (x86) 版本**，因为 64 位 Windows XP 比较少见。
> 在 "Developer Command Prompt for VS 2022" 中默认就是 x86 编译。

### Build with build.bat

```cmd
cd cm-rgb-cpp
build              # Release build (XP compatible)
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

## License

MIT
