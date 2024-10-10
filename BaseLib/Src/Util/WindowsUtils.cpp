#include "WindowsUtils.h"
#include <shlwapi.h>
#include <absl/strings/str_format.h>

std::string GetExeSubFolder(const char* subFolder)
{
    std::string templateFolder;
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(nullptr, szPath, ARRAYSIZE(szPath)))
    {
        PSTR pszFileName = PathFindFileName(szPath);
        *pszFileName = 0; // null it
        templateFolder = absl::StrFormat("%s%s", szPath, subFolder);
    }
    return templateFolder;
}
