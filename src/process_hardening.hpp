#pragma once
namespace krmemory {
struct ProcessHardeningResult {bool dll_search{};bool extension_points_disabled{};bool remote_images_blocked{};bool low_integrity_images_blocked{};bool prefer_system32{};};
// Compatible with Media Foundation, WinRT and OpenCL. This is defense in
// depth, not an AppContainer boundary from other same-user processes.
ProcessHardeningResult apply_process_hardening() noexcept;
}
