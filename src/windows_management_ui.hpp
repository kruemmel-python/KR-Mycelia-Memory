#pragma once
#ifdef _WIN32
#include <windows.h>
namespace krmemory { void show_management_window(HWND owner); }
#endif
