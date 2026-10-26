#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "json_value.hpp"
#include "memory_service_rpc.hpp"
#include "windows_explorer_ui.hpp"
#include "windows_theme.hpp"

namespace krmemory { namespace {
constexpr int ID_QUERY=4101,ID_SOURCE=4102,ID_COLLECTION=4103,ID_SEARCH=4104,ID_LIST=4105,ID_DETAIL=4106,ID_IMPORT=4107,ID_EXPORT=4108,ID_DELETE=4109,ID_RELATIONS=4110,ID_REFRESH=4111,ID_CANCEL_IMPORT=4112,ID_STATUS=4113;
constexpr UINT WM_RESULTS=WM_APP+51,WM_ACTION=WM_APP+52;
enum class ActionKind {generic,import,relations,detail,cancel};
struct Record {std::string signature,table;std::wstring title,detail;};
struct Result {unsigned generation{};bool ok{};std::wstring error;std::vector<Record> records;};
struct ActionResult {ActionKind kind{};bool ok{},refresh{},show_message{};std::wstring success,failure,detail;std::string job;};
struct State {HWND query{},source{},collection{},list{},detail{},search{},cancel{},status{};std::atomic<unsigned> generation{0};std::vector<Record> records;std::string import_job;std::shared_ptr<std::atomic_bool> alive{std::make_shared<std::atomic_bool>(true)};};
std::atomic<unsigned long long> counter{1};

std::wstring wide(std::string_view text){if(text.empty())return{};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);if(n<=0)return L"�";std::wstring out(std::size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),out.data(),n);return out;}
std::string utf8(HWND control){int n=GetWindowTextLengthW(control);std::wstring value(std::size_t(n)+1,L'\0');GetWindowTextW(control,value.data(),n+1);int bytes=WideCharToMultiByte(CP_UTF8,0,value.data(),n,nullptr,0,nullptr,nullptr);std::string out(std::size_t(bytes),'\0');if(bytes)WideCharToMultiByte(CP_UTF8,0,value.data(),n,out.data(),bytes,nullptr,nullptr);return out;}
std::string path_utf8(std::wstring_view value){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);std::string out(std::size_t(std::max(0,n)),'\0');if(n>0)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n,nullptr,nullptr);return out;}
std::string field(const json::Value& item,std::string_view key){auto fields=item.get("fields");auto value=fields?fields->get(key):nullptr;return value&&value->string()?*value->string():std::string{};}
std::wstring record_detail(const json::Value& item){auto signature=item.get("signature"),table=item.get("table");std::wstring detail=L"Signatur: "+wide(signature&&signature->string()?*signature->string():std::string{})+L"\r\nTabelle: "+wide(table&&table->string()?*table->string():std::string{});if(auto fields=item.get("fields");fields&&fields->is_object())for(const auto& [key,value]:fields->object())if(value.string())detail+=L"\r\n\r\n"+wide(key)+L":\r\n"+wide(*value.string());return detail;}
template<class T>void post(HWND hwnd,UINT message,std::unique_ptr<T> payload,const std::shared_ptr<std::atomic_bool>& alive){if(alive->load()&&PostMessageW(hwnd,message,0,reinterpret_cast<LPARAM>(payload.get())))payload.release();}

void query(HWND hwnd,State* state){auto text=utf8(state->query),source=utf8(state->source),collection=utf8(state->collection);auto generation=++state->generation;auto alive=state->alive;EnableWindow(state->search,FALSE);SetWindowTextW(state->status,L"Gedächtnis wird durchsucht …");std::thread([hwnd,generation,alive,text=std::move(text),source=std::move(source),collection=std::move(collection)]{
    auto result=std::make_unique<Result>();result->generation=generation;std::string response;auto id="gui-explorer-"+std::to_string(counter++);auto params="{\"query\":"+service_rpc::quote(text)+",\"source\":"+service_rpc::quote(source)+",\"collection\":"+service_rpc::quote(collection)+",\"limit\":500,\"summary\":true}";result->ok=service_rpc::call(id,text.empty()?"memory.list":"memory.search",params,response,15000);
    if(result->ok)try{auto root=json::parse(response);auto records=root.get("records");if(!records||!records->array())throw std::runtime_error("records missing");for(const auto& item:*records->array()){auto signature=item.get("signature"),table=item.get("table");if(!signature||!signature->string()||!table||!table->string())continue;Record record;record.signature=*signature->string();record.table=*table->string();auto title=field(item,"title");if(title.empty())title=field(item,"question");record.title=wide(title.empty()?record.table:title);record.detail=record_detail(item);result->records.push_back(std::move(record));}}catch(...){result->ok=false;}
    if(!result->ok)result->error=L"Die Gedächtnissuche ist fehlgeschlagen oder der Dienst ist nicht erreichbar.";post(hwnd,WM_RESULTS,std::move(result),alive);
}).detach();}

void action(HWND hwnd,State* state,ActionKind kind,std::string operation,std::string params,DWORD timeout,std::wstring success,std::wstring failure,bool refresh,bool show_message=true,std::string job={}){auto alive=state->alive;std::thread([hwnd,alive,kind,operation=std::move(operation),params=std::move(params),timeout,success=std::move(success),failure=std::move(failure),refresh,show_message,job=std::move(job)]{auto result=std::make_unique<ActionResult>();result->kind=kind;result->success=success;result->failure=failure;result->refresh=refresh;result->show_message=show_message;result->job=job;std::string response;auto id="gui-action-"+std::to_string(counter++);result->ok=service_rpc::call(id,operation,params,response,timeout);if(result->ok&&kind==ActionKind::relations)result->detail=wide(response);if(result->ok&&kind==ActionKind::detail)try{auto root=json::parse(response);auto record=root.get("record");if(!record||!record->is_object())throw std::runtime_error("record missing");result->detail=record_detail(*record);}catch(...){result->ok=false;}post(hwnd,WM_ACTION,std::move(result),alive);}).detach();}

std::string selected(State* state){auto index=SendMessageW(state->list,LB_GETCURSEL,0,0);return index==LB_ERR||std::size_t(index)>=state->records.size()?std::string{}:state->records[std::size_t(index)].signature;}
std::wstring choose_file(HWND owner,bool save){wchar_t file[32768]{};OPENFILENAMEW value{sizeof(value)};value.hwndOwner=owner;value.lpstrFile=file;value.nMaxFile=32768;value.lpstrFilter=save?L"JSON Export (*.json)\0*.json\0Alle Dateien\0*.*\0":L"Unterstützte Dokumente und Medien\0*.txt;*.md;*.json;*.csv;*.xml;*.html;*.rtf;*.docx;*.xlsx;*.pptx;*.pdf;*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.webp;*.wav;*.mp3;*.wma;*.mp4;*.m4a;*.webm;*.wmv\0Alle Dateien\0*.*\0";value.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);if(save){value.lpstrDefExt=L"json";return GetSaveFileNameW(&value)?file:L"";}return GetOpenFileNameW(&value)?file:L"";}
HWND control(HWND parent,DWORD ex,const wchar_t* type,const wchar_t* text,DWORD style,int id){return CreateWindowExW(ex,type,text,WS_CHILD|WS_VISIBLE|style,0,0,10,10,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);}
void layout(HWND hwnd,State* state){RECT rect{};GetClientRect(hwnd,&rect);UINT dpi=96;if(auto fn=reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow")))dpi=fn(hwnd);auto s=[dpi](int value){return scale_for_dpi(value,dpi);};auto index=std::clamp(appearance_settings().font_sizes[static_cast<int>(UiArea::explorer)],0,3);int extra=index*5,width=rect.right,height=rect.bottom,margin=s(16),gap=s(12),title_h=s(24+extra),top=s(62+extra),control_h=s(30+extra),button_h=s(32+extra),button_y=std::max(s(300),height-s(48+extra)),content_top=top+control_h+s(12),content_bottom=button_y-s(12+extra);auto title=GetWindow(hwnd,GW_CHILD),subtitle=title?GetWindow(title,GW_HWNDNEXT):nullptr;if(title)MoveWindow(title,margin,s(10),width-margin*2,title_h,TRUE);if(subtitle)MoveWindow(subtitle,margin,s(32)+title_h/2,width-margin*2,title_h,TRUE);int action_w=s(190+extra*2),filter_width=std::max(s(120),(width-margin*2-gap*3-action_w)/3);MoveWindow(state->query,margin,top,filter_width,control_h,TRUE);MoveWindow(state->source,margin+filter_width+gap,top,filter_width,control_h,TRUE);MoveWindow(state->collection,margin+(filter_width+gap)*2,top,filter_width,control_h,TRUE);MoveWindow(state->search,width-margin-s(178+extra),top,s(80+extra),control_h,TRUE);MoveWindow(GetDlgItem(hwnd,ID_REFRESH),width-margin-s(88),top,s(88),control_h,TRUE);int split=std::max(s(240),(width-margin*2-gap)*38/100),pane_h=std::max(s(120),content_bottom-content_top);MoveWindow(state->list,margin,content_top,split,pane_h,TRUE);MoveWindow(state->detail,margin+split+gap,content_top,std::max(s(240),width-margin*2-gap-split),pane_h,TRUE);int x=margin;for(int id:{ID_IMPORT,ID_CANCEL_IMPORT,ID_EXPORT,ID_RELATIONS}){int w=s((id==ID_CANCEL_IMPORT?145:120)+extra*2);MoveWindow(GetDlgItem(hwnd,id),x,button_y,w,button_h,TRUE);x+=w+s(10);}MoveWindow(GetDlgItem(hwnd,ID_DELETE),std::max(x,width-margin-s(175+extra*2)),button_y,s(175+extra*2),button_h,TRUE);MoveWindow(state->status,margin,button_y-s(25+extra),width-margin*2,s(22+extra),TRUE);}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE:
        state = new State;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        control(hwnd, 0, L"STATIC", L"Memory Explorer", SS_LEFT, 0);
        control(hwnd, 0, L"STATIC", L"Suche · Quelle · Collection", SS_LEFT, 0);
        state->query = control(hwnd, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_QUERY);
        state->source = control(hwnd, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_SOURCE);
        state->collection = control(hwnd, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_COLLECTION);
        state->search = control(hwnd, 0, L"BUTTON", L"Suchen", WS_TABSTOP | BS_DEFPUSHBUTTON, ID_SEARCH);
        control(hwnd, 0, L"BUTTON", L"Alle", WS_TABSTOP, ID_REFRESH);
        state->list = control(hwnd, WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, ID_LIST);
        state->detail = control(hwnd, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, ID_DETAIL);
        control(hwnd, 0, L"BUTTON", L"Importieren", WS_TABSTOP, ID_IMPORT);
        state->cancel = control(hwnd, 0, L"BUTTON", L"Import abbrechen", WS_TABSTOP | WS_DISABLED, ID_CANCEL_IMPORT);
        control(hwnd, 0, L"BUTTON", L"Exportieren", WS_TABSTOP, ID_EXPORT);
        control(hwnd, 0, L"BUTTON", L"Beziehungen", WS_TABSTOP, ID_RELATIONS);
        control(hwnd, 0, L"BUTTON", L"Erinnerung löschen", WS_TABSTOP, ID_DELETE);
        state->status = control(hwnd, 0, L"STATIC", L"Bereit", SS_LEFT, ID_STATUS);
        apply_windows_11_theme(hwnd);
        layout(hwnd, state);
        query(hwnd, state);
        return 0;
    case WM_SIZE:
        if (state) layout(hwnd, state);
        return 0;
    case WM_GETMINMAXINFO:
        {auto index=std::clamp(appearance_settings().font_sizes[static_cast<int>(UiArea::explorer)],0,3);reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {800+index*80,560+index*55};}
        return 0;
    case WM_RESULTS: {
        std::unique_ptr<Result> result(reinterpret_cast<Result*>(lp));
        if (!state || result->generation != state->generation) return 0;
        EnableWindow(state->search, TRUE);
        state->records = std::move(result->records);
        SendMessageW(state->list, LB_RESETCONTENT, 0, 0);
        for (const auto& record : state->records) {
            auto label = wide("[" + record.table + "] ") + record.title;
            SendMessageW(state->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        if (state->records.empty())
            SendMessageW(state->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(result->ok ? L"Keine Treffer." : result->error.c_str()));
        auto status = result->ok ? L"Bereit · " + std::to_wstring(state->records.size()) + L" Einträge" : result->error;
        SetWindowTextW(state->status, status.c_str());
        return 0;
    }
    case WM_ACTION: {
        std::unique_ptr<ActionResult> result(reinterpret_cast<ActionResult*>(lp));
        if (!state) return 0;
        if (result->kind == ActionKind::import && result->job == state->import_job) {
            state->import_job.clear();
            EnableWindow(state->cancel, FALSE);
        }
        if (result->kind == ActionKind::relations && result->ok) SetWindowTextW(state->detail, result->detail.c_str());
        if (result->kind == ActionKind::detail && result->ok && selected(state) == result->job) SetWindowTextW(state->detail, result->detail.c_str());
        auto status = result->kind == ActionKind::cancel
            ? (result->ok ? L"Abbruch wurde an den laufenden Import übermittelt." : L"Import konnte nicht abgebrochen werden.")
            : (result->ok ? result->success : result->failure);
        SetWindowTextW(state->status, status.c_str());
        if (result->show_message)
            MessageBoxW(hwnd, result->ok ? result->success.c_str() : result->failure.c_str(), L"KR Mycelia Memory", MB_OK | (result->ok ? MB_ICONINFORMATION : MB_ICONERROR));
        if (result->ok && result->refresh) query(hwnd, state);
        return 0;
    }
    case WM_COMMAND:
        if (!state) break;
        switch (LOWORD(wp)) {
        case ID_SEARCH:
            query(hwnd, state);
            return 0;
        case ID_REFRESH:
            SetWindowTextW(state->query, L"");
            SetWindowTextW(state->source, L"");
            SetWindowTextW(state->collection, L"");
            query(hwnd, state);
            return 0;
        case ID_LIST:
            if (HIWORD(wp) == LBN_SELCHANGE) {
                auto index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR && std::size_t(index) < state->records.size()) {
                    const auto& record=state->records[std::size_t(index)];SetWindowTextW(state->detail,L"Vollständige Erinnerung wird geladen …");
                    action(hwnd,state,ActionKind::detail,"memory.get","{\"signature\":"+service_rpc::quote(record.signature)+"}",15000,L"",L"Die Erinnerung konnte nicht geladen werden.",false,false,record.signature);
                }
            }
            return 0;
        case ID_IMPORT: {
            if (!state->import_job.empty()) return 0;
            auto path = choose_file(hwnd, false);
            if (path.empty()) return 0;
            state->import_job = "gui-import-" + std::to_string(counter++);
            EnableWindow(state->cancel, TRUE);
            SetWindowTextW(state->status, L"Datei wird nativ verarbeitet und in der DAD gespeichert …");
            auto params = "{\"path\":" + service_rpc::quote(path_utf8(path)) + ",\"job_id\":" + service_rpc::quote(state->import_job) + "}";
            action(hwnd, state, ActionKind::import, "memory.import", params, 3600000, L"Import wurde dauerhaft in der DAD gespeichert.", L"Import wurde abgebrochen oder ist fehlgeschlagen.", true, true, state->import_job);
            return 0;
        }
        case ID_CANCEL_IMPORT:
            if (!state->import_job.empty())
                action(hwnd, state, ActionKind::cancel, "memory.import.cancel", "{\"job_id\":" + service_rpc::quote(state->import_job) + "}", 5000, L"Abbruch angefordert.", L"Abbruch fehlgeschlagen.", false, false, state->import_job);
            return 0;
        case ID_EXPORT: {
            auto path = choose_file(hwnd, true);
            if (!path.empty()) action(hwnd, state, ActionKind::generic, "memory.export", "{\"path\":" + service_rpc::quote(path_utf8(path)) + "}", 30000, L"Export abgeschlossen.", L"Export fehlgeschlagen.", false);
            return 0;
        }
        case ID_DELETE: {
            auto signature = selected(state);
            if (signature.empty() || MessageBoxW(hwnd, L"Diese Erinnerung einschließlich ihrer Beziehungen dauerhaft löschen?", L"KR Mycelia Memory", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
            action(hwnd, state, ActionKind::generic, "memory.delete", "{\"signature\":" + service_rpc::quote(signature) + "}", 10000, L"Erinnerung gelöscht.", L"Löschen fehlgeschlagen.", true);
            return 0;
        }
        case ID_RELATIONS: {
            auto signature = selected(state);
            if (!signature.empty()) action(hwnd, state, ActionKind::relations, "memory.relations.list", "{\"signature\":" + service_rpc::quote(signature) + "}", 5000, L"Beziehungen geladen.", L"Beziehungen konnten nicht geladen werden.", false, false);
            return 0;
        }
        }
        break;
    case WM_SETTINGCHANGE:
        apply_windows_11_theme(hwnd);
        return 0;
    case WM_CTLCOLORSTATIC:
        return reinterpret_cast<LRESULT>(themed_control_brush(reinterpret_cast<HDC>(wp)));
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return reinterpret_cast<LRESULT>(themed_control_brush(reinterpret_cast<HDC>(wp), true));
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state) {
            state->alive->store(false);
            ++state->generation;
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        release_windows_11_theme(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

void show_explorer_window(HWND owner) {
    static bool registered{};
    auto instance = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW window_class{sizeof(window_class)};
        window_class.lpfnWndProc = proc;
        window_class.hInstance = instance;
        window_class.lpszClassName = L"KRMyceliaExplorerWindowV2";
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        registered = RegisterClassExW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    auto window = CreateWindowExW(0, L"KRMyceliaExplorerWindowV2", L"KR Mycelia Memory – Explorer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 720, owner, nullptr, instance, nullptr);
    if (window) {
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
    }
}

} // namespace krmemory
