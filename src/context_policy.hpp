#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
namespace krmemory {
inline std::string normalize_exe(std::string_view v) {
    auto start = v.find_last_of("/\\");
    if(start != std::string_view::npos) v.remove_prefix(start+1);
    std::string s(v);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return s;
}
inline bool is_sensitive(std::string_view title) {
    std::string t(title);
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
    for(auto word: {"password", "passwort", "kennwort", "private browsing", "incognito", "inkognito", "banking", "otp", "2fa", "secret", "geheim", "credential", "anmeldung"}) {
        if(t.find(word)!=std::string::npos) return true;
    }
    return false;
}
inline bool permitted(std::string_view exe, std::string_view title, const std::vector<std::string>& allow) {
    if(title.empty() || is_sensitive(title)) return false;
    const auto name = normalize_exe(exe);
    if(name.empty()) return false;
    return std::any_of(allow.begin(),allow.end(),[&](const auto& value){return normalize_exe(value)==name;});
}
inline std::string truncate_utf8(std::string text, std::size_t limit=240) {
    if(text.size()<=limit) return text;
    text.resize(limit);
    while(!text.empty() && (static_cast<unsigned char>(text.back())&0xC0)==0x80) text.pop_back();
    return text;
}
}
