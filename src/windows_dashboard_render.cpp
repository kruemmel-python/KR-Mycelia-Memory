#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <algorithm>
#include <string>
#include "windows_dashboard_render.hpp"
#include "windows_theme.hpp"

namespace krmemory {namespace {
template<class T>void release(T*& value){if(value){value->Release();value=nullptr;}}
D2D1_COLOR_F color(COLORREF value,float alpha=1.f){return D2D1::ColorF(GetRValue(value)/255.f,GetGValue(value)/255.f,GetBValue(value)/255.f,alpha);}
D2D1_RECT_F rect(const RECT& value){return D2D1::RectF(float(value.left),float(value.top),float(value.right),float(value.bottom));}
UINT window_dpi(HWND window){if(auto fn=reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow")))return fn(window);return 96;}
}
#define GetDpiForWindow window_dpi
struct DashboardRenderer::Impl {ID2D1Factory* factory{};ID2D1HwndRenderTarget* target{};IDWriteFactory* write{};IDWriteTextFormat* title{};IDWriteTextFormat* small{};IDWriteTextFormat* mono{};int font_key{-1};~Impl(){release(mono);release(small);release(title);release(write);release(target);release(factory);}};
DashboardRenderer::DashboardRenderer():impl_(new Impl){}
DashboardRenderer::~DashboardRenderer(){delete impl_;}
void DashboardRenderer::discard(){release(impl_->target);}
void DashboardRenderer::resize(unsigned width,unsigned height){if(impl_->target){impl_->target->SetDpi(96.f,96.f);impl_->target->Resize(D2D1::SizeU(std::max(1u,width),std::max(1u,height)));}}
void DashboardRenderer::paint(HWND window,const DashboardChromeLayout& layout,const DashboardTelemetry& telemetry){
    PAINTSTRUCT ps{};BeginPaint(window,&ps);
    if(!impl_->factory)D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&impl_->factory);
    if(!impl_->write)DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&impl_->write));
    auto settings=appearance_settings();auto font_index=std::clamp(settings.font_sizes[static_cast<int>(UiArea::dashboard)],0,3);auto dpi=window_dpi(window);auto font_key=int(dpi)*10+font_index;
    if(impl_->write&&impl_->font_key!=font_key){
        release(impl_->mono);release(impl_->small);release(impl_->title);impl_->font_key=font_key;
        constexpr float factors[]={.90f,1.f,1.25f,1.50f};auto scale=float(dpi)/96.f*factors[font_index];
        impl_->write->CreateTextFormat(L"Segoe UI Variable Display",nullptr,DWRITE_FONT_WEIGHT_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,15.f*scale,L"de-DE",&impl_->title);
        impl_->write->CreateTextFormat(L"Segoe UI Variable Text",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,11.f*scale,L"de-DE",&impl_->small);
        impl_->write->CreateTextFormat(L"Cascadia Mono",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,10.f*scale,L"de-DE",&impl_->mono);
    }
    if(!impl_->target&&impl_->factory){
        RECT r{};GetClientRect(window,&r);
        auto properties=D2D1::RenderTargetProperties();properties.dpiX=96.f;properties.dpiY=96.f;
        impl_->factory->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(window,D2D1::SizeU(std::max(1L,r.right),std::max(1L,r.bottom))),&impl_->target);
    }
    if(!impl_->target){EndPaint(window,&ps);return;}
    impl_->target->SetDpi(96.f,96.f);
    auto p=current_ui_palette(UiArea::dashboard);
    ID2D1SolidColorBrush *background{},*sidebar{},*surface{},*subtle{},*border{},*text{},*muted{},*accent{},*green{},*shadow{};
    impl_->target->CreateSolidColorBrush(color(p.background),&background);impl_->target->CreateSolidColorBrush(color(p.sidebar),&sidebar);impl_->target->CreateSolidColorBrush(color(p.surface,.96f),&surface);impl_->target->CreateSolidColorBrush(color(p.subtle),&subtle);impl_->target->CreateSolidColorBrush(color(p.border),&border);impl_->target->CreateSolidColorBrush(color(p.text),&text);impl_->target->CreateSolidColorBrush(color(p.muted),&muted);impl_->target->CreateSolidColorBrush(color(p.accent),&accent);impl_->target->CreateSolidColorBrush(color(RGB(52,211,153)),&green);impl_->target->CreateSolidColorBrush(color(RGB(0,0,0),p.dark?.28f:.10f),&shadow);
    impl_->target->BeginDraw();impl_->target->Clear(color(p.background));RECT client{};GetClientRect(window,&client);auto ui=float(dpi)/96.f;auto u=[ui](float value){return value*ui;};constexpr float typography[]={.90f,1.f,1.25f,1.50f};auto type_scale=typography[font_index];impl_->target->FillRectangle({0,0,float(layout.sidebar),float(client.bottom)},sidebar);impl_->target->DrawLine({float(layout.sidebar)-.5f,0},{float(layout.sidebar)-.5f,float(client.bottom)},border,1.f);
    auto card=[&](const RECT& r){if(r.right<=r.left||r.bottom<=r.top)return;auto rr=rect(r);auto radius=u(16);auto shadow_rect=D2D1::RoundedRect({rr.left+u(2),rr.top+u(5),rr.right+u(2),rr.bottom+u(5)},radius,radius);impl_->target->FillRoundedRectangle(shadow_rect,shadow);impl_->target->FillRoundedRectangle(D2D1::RoundedRect(rr,radius,radius),surface);impl_->target->DrawRoundedRectangle(D2D1::RoundedRect(rr,radius,radius),border,1.f);};for(auto r:{layout.status,layout.manual,layout.search,layout.results,layout.graph,layout.activity,layout.agents,layout.history})card(r);
    if(impl_->title){impl_->target->DrawTextW(L"KR MYCELIA",10,impl_->title,{u(22),u(24),float(layout.sidebar)-u(18),u(48+10*type_scale)},text);impl_->target->DrawTextW(L"MEMORY EDITION",14,impl_->small,{u(22),u(52+5*type_scale),float(layout.sidebar)-u(18),u(78+10*type_scale)},accent);}if(impl_->small){impl_->target->DrawTextW(L"ZENTRAL",7,impl_->small,{u(22),u(91),float(layout.sidebar)-u(18),u(121)},muted);impl_->target->DrawTextW(L"KI & AGENTEN",12,impl_->small,{u(22),u(309),float(layout.sidebar)-u(18),u(338)},muted);impl_->target->DrawTextW(L"SYSTEM & DATEN",14,impl_->small,{u(22),u(455),float(layout.sidebar)-u(18),u(484)},muted);auto footer=u(58+18*std::max(0,font_index-1));impl_->target->DrawTextW(L"DPAPI GESICHERT",15,impl_->small,{u(22),float(client.bottom)-footer,float(layout.sidebar)-u(18),float(client.bottom)-footer+u(28)},green);auto footer_text=font_index>=2?L"memory.dpapi  ·  2 Backups":L"memory.dpapi  ·  bak1 / bak2";impl_->target->DrawTextW(footer_text,UINT32(wcslen(footer_text)),impl_->mono,{u(22),float(client.bottom)-footer+u(27),float(layout.sidebar)-u(18),float(client.bottom)-u(10)},muted);}
    auto status_text=std::wstring(telemetry.service_ready?L"●  Named Pipe verbunden":L"○  Dienst nicht erreichbar");auto status_top=float(layout.status.top)+u(10),metric_top=float(layout.status.top)+u(34+7*std::max(0,font_index-1)),metric_h=u(24+5*std::max(0,font_index-1));if(impl_->small)impl_->target->DrawTextW(status_text.c_str(),UINT32(status_text.size()),impl_->small,{float(layout.status.left)+u(18),status_top,float(layout.status.left)+u(320),metric_top},telemetry.service_ready?green:muted);auto metric=[&](int column,const std::wstring& line){auto x=float(layout.status.left)+u(18+column*126);impl_->target->FillRoundedRectangle(D2D1::RoundedRect({x,metric_top,x+u(118),metric_top+metric_h},u(7),u(7)),subtle);if(impl_->mono)impl_->target->DrawTextW(line.c_str(),UINT32(line.size()),impl_->mono,{x+u(7),metric_top+u(3),x+u(114),metric_top+metric_h-u(2)},text);};metric(0,L"Attraktoren: "+std::to_wstring(telemetry.attractors));metric(1,L"Stabilität: "+std::to_wstring(int(telemetry.stability*100))+L"%");metric(2,L"Rauschen: "+std::to_wstring(telemetry.noise).substr(0,5));metric(3,L"Ordner: "+std::to_wstring(telemetry.folders));
    auto heading=[&](const RECT& r,const wchar_t* value,bool divider=true){if(r.right<=r.left||r.bottom<=r.top)return;auto line_y=float(r.top)+u(38+8*std::max(0,font_index-1));if(impl_->small)impl_->target->DrawTextW(value,UINT32(wcslen(value)),impl_->small,{float(r.left)+u(16),float(r.top)+u(9),float(r.right)-u(12),line_y-u(4)},text);if(divider)impl_->target->DrawLine({float(r.left)+u(14),line_y},{float(r.right)-u(14),line_y},border,1.f);};heading(layout.manual,L"MANUELLE ERINNERUNG",false);heading(layout.search,L"ASSOZIATIVE SUCHE",false);heading(layout.results,L"Assoziative Treffer & DAD-Erinnerungen");heading(layout.graph,L"Assoziatives Beziehungsnetzwerk  ·  Live DAD Graph");heading(layout.activity,L"CHRONOLOGISCHE AKTIVITÄT");heading(layout.agents,L"ANGESCHLOSSENE KI-AGENTEN (MCP)");heading(layout.history,L"SUCHHISTORIE & DAD-METRIKEN");
    auto result=impl_->target->EndDraw();if(result==D2DERR_RECREATE_TARGET)discard();for(auto* b:{shadow,green,accent,muted,text,border,subtle,surface,sidebar,background})release(b);EndPaint(window,&ps);}
}
#endif
