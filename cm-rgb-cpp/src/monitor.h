#pragma once

#include "controller.h"
#include <windows.h>

// Monitor mode: displays CPU utilization on ring LEDs
// Optionally shows temperature on fan LED and CPU frequency on logo LED
int RunMonitor(struct Args& args);
