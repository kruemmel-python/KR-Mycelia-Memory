#include "context_policy.hpp"
#include <stdexcept>
#include <iostream>
static void check(bool ok){if(!ok)throw std::runtime_error("context policy assertion failed");}
int main() {
    using namespace krmemory;
    check(normalize_exe("C:\\WINDOWS\\NOTEPAD.EXE")=="notepad.exe");
    check(permitted("C:\\Windows\\notepad.exe","Research notes",{"notepad.exe"}));
    check(!permitted("firefox.exe","Research notes",{"notepad.exe"}));
    check(!permitted("notepad.exe","My Passwort",{"notepad.exe"}));
    check(!permitted("notepad.exe","",{"notepad.exe"}));
    check(truncate_utf8(std::string(270,'a')).size()==240);
    std::cout << "PASS: opt-in allowlist, sensitive-title exclusion, bounded text\n";
}
