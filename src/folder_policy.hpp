#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace krmemory {inline bool permitted_folder(const std::filesystem::path& candidate,const std::vector<std::filesystem::path>& allowed){std::error_code ec;auto c=std::filesystem::weakly_canonical(candidate,ec);if(ec)return false;auto lower=[](std::wstring v){for(auto& x:v)x=std::towlower(x);return v;};auto text=lower(c.wstring());for(const auto& blocked:{L"\\password",L"\\credential",L"\\secret",L"\\appdata\\local\\microsoft\\credentials"})if(text.find(blocked)!=std::wstring::npos)return false;for(const auto& root:allowed){auto r=std::filesystem::weakly_canonical(root,ec);if(ec)continue;auto rs=lower(r.wstring());if(text.size()>=rs.size()&&text.compare(0,rs.size(),rs)==0&&(text.size()==rs.size()||text[rs.size()]==L'\\'))return true;}return false;}}
