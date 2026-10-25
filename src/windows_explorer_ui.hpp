#pragma once
#ifdef _WIN32
#include <windows.h>
namespace krmemory { void show_explorer_window(HWND owner); }
#endif
