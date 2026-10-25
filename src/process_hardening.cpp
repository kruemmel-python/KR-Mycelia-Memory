#include "process_hardening.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
namespace krmemory {
ProcessHardeningResult apply_process_hardening() noexcept {
    ProcessHardeningResult result;
    result.dll_search=SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS)!=FALSE;
    SetDllDirectoryW(L"");
    using SetPolicy=BOOL(WINAPI*)(PROCESS_MITIGATION_POLICY,PVOID,SIZE_T);using GetPolicy=BOOL(WINAPI*)(HANDLE,PROCESS_MITIGATION_POLICY,PVOID,SIZE_T);
    auto kernel=GetModuleHandleW(L"kernel32.dll");auto set_policy=reinterpret_cast<SetPolicy>(GetProcAddress(kernel,"SetProcessMitigationPolicy"));auto get_policy=reinterpret_cast<GetPolicy>(GetProcAddress(kernel,"GetProcessMitigationPolicy"));
    if(!set_policy||!get_policy)return result;
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY extensions{};extensions.DisableExtensionPoints=1;set_policy(ProcessExtensionPointDisablePolicy,&extensions,sizeof(extensions));
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY images{};images.NoRemoteImages=1;images.NoLowMandatoryLabelImages=1;images.PreferSystem32Images=1;set_policy(ProcessImageLoadPolicy,&images,sizeof(images));
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY actual_extensions{};PROCESS_MITIGATION_IMAGE_LOAD_POLICY actual_images{};
    if(get_policy(GetCurrentProcess(),ProcessExtensionPointDisablePolicy,&actual_extensions,sizeof(actual_extensions)))result.extension_points_disabled=actual_extensions.DisableExtensionPoints!=0;
    if(get_policy(GetCurrentProcess(),ProcessImageLoadPolicy,&actual_images,sizeof(actual_images))){result.remote_images_blocked=actual_images.NoRemoteImages!=0;result.low_integrity_images_blocked=actual_images.NoLowMandatoryLabelImages!=0;result.prefer_system32=actual_images.PreferSystem32Images!=0;}
    return result;
}
}
#else
namespace krmemory {ProcessHardeningResult apply_process_hardening() noexcept{return{};}}
#endif
