#pragma once
#include <windows.h>

// Launch the GUI window (blocks until window is closed)
int RunGUI(HINSTANCE hInstance = GetModuleHandle(NULL));
