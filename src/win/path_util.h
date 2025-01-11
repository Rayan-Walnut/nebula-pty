#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <string>
#include <windows.h>

namespace path_util {

std::wstring GetSystemDirectory();
std::wstring JoinPath(const std::wstring& path1, const std::wstring& path2);
bool FileExists(const std::wstring& path);
std::wstring GetShellPath();

}  // namespace path_util

#endif // PATH_UTIL_H