#include "path_util.h"
#include <shlwapi.h>
#include <vector>

namespace path_util {

std::wstring GetSystemDirectory() {
    std::vector<wchar_t> buffer(MAX_PATH);
    UINT size = ::GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    
    if (size > buffer.size()) {
        buffer.resize(size);
        size = ::GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    }
    
    if (size == 0) {
        return L"";
    }
    
    return std::wstring(buffer.data(), size);
}

std::wstring JoinPath(const std::wstring& path1, const std::wstring& path2) {
    std::vector<wchar_t> buffer(MAX_PATH);
    
    wcscpy_s(buffer.data(), buffer.size(), path1.c_str());
    PathAppendW(buffer.data(), path2.c_str());
    
    return std::wstring(buffer.data());
}

bool FileExists(const std::wstring& path) {
    return PathFileExistsW(path.c_str()) == TRUE;
}

std::wstring GetShellPath() {
    // Try to find cmd.exe in System32 directory
    std::wstring systemDir = GetSystemDirectory();
    if (systemDir.empty()) {
        return L"cmd.exe";
    }
    
    std::wstring cmdPath = JoinPath(systemDir, L"cmd.exe");
    if (FileExists(cmdPath)) {
        return cmdPath;
    }
    
    // Fallback to just cmd.exe, letting the system find it
    return L"cmd.exe";
}

}  // namespace path_util