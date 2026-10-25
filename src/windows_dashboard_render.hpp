#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstddef>
namespace krmemory {
struct DashboardChromeLayout {int sidebar{};RECT status{},manual{},search{},results{},graph{},activity{},agents{},history{};};
struct DashboardTelemetry {bool service_ready{},capturing{},autostart{};std::size_t attractors{},records{},relations{},folders{},agents{};double stability{},noise{};};
class DashboardRenderer {
public:
    DashboardRenderer();~DashboardRenderer();DashboardRenderer(const DashboardRenderer&)=delete;DashboardRenderer& operator=(const DashboardRenderer&)=delete;
    void paint(HWND window,const DashboardChromeLayout& layout,const DashboardTelemetry& telemetry);
    void resize(unsigned width,unsigned height);void discard();
private:struct Impl;Impl* impl_{};
};
}
#endif
