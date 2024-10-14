#include "ScriptId.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>

#include <Shlwapi.h>
#include <strsafe.h>

const std::string SCILanguageMarker = "Sierra Script";

LangSyntax DetermineLanguageFromFirstLine(const std::string& firstLine)
{
    LangSyntax langSniff = LangSyntaxStudio;
    size_t pos = firstLine.find(';');
    if (pos != std::string::npos)
    {
        if (firstLine.find(SCILanguageMarker) != std::string::npos)
        {
            langSniff = LangSyntaxSCI;
        }
    }
    return langSniff;
}

LangSyntax DetermineFileLanguage(const std::string& filename)
{
    // Sniff the file
    LangSyntax langSniff;
    std::ifstream file(filename);
    std::string line;
    if (std::getline(file, line))
    {
        langSniff = ::DetermineLanguageFromFirstLine(line);
    }
    else
    {
        // This can happen if the file doesn't exist.
        langSniff = LangSyntaxSCI;
    }
    return langSniff;
}

ScriptId ScriptId::FromFullFileName(const std::string& filename)
{
    ScriptId id;
    id._Init(filename.c_str());
    return id;
}

ScriptId::ScriptId() : _language(LangSyntaxUnknown) { _wScriptNum = InvalidResourceNumber; };

ScriptId::ScriptId(const char* pszFileName, const char* pszFolder) : _language(LangSyntaxUnknown)
{
    assert(std::strchr(pszFileName, '\\') == nullptr); // Ensure file and path are not mixed up.
    _strFileName = pszFileName;
    _strFolder = pszFolder;
    _MakeLower();
    _wScriptNum = InvalidResourceNumber;
}

ScriptId ScriptId::WithFullPath(const std::string& fullPath) const
{
    ScriptId new_id;
    new_id._Init(fullPath.c_str(), GetResourceNumber());
    return new_id;
}

void ScriptId::_DetermineLanguage()
{
    if (_language == LangSyntaxUnknown)
    {
        _language = DetermineFileLanguage(GetFullPath());
    }
}

void ScriptId::SetResourceNumber(WORD wScriptNum)
{
    assert((_wScriptNum == InvalidResourceNumber) || (_wScriptNum == wScriptNum));
    _wScriptNum = wScriptNum;
}


void ScriptId::SetLanguage(LangSyntax lang) { _language = lang; }

bool ScriptId::IsNone() const { return _strFileName.empty(); }
const std::string& ScriptId::GetFileName() const { return _strFileName; }
const std::string& ScriptId::GetFolder() const { return _strFolder; }
const std::string& ScriptId::GetFileNameOrig() const { return _strFileNameOrig; }

//
// ScriptId implementation
// (unique identifier for a script)
//
std::string ScriptId::GetTitle() const
{
    TCHAR szFileName[MAX_PATH];
    StringCchCopy(szFileName, ARRAYSIZE(szFileName), _strFileNameOrig.c_str());
    PTSTR pszExt = PathFindExtension(szFileName);
    *pszExt = 0; // Chop off file extension
    return szFileName;
}
std::string ScriptId::GetTitleLower() const
{
    TCHAR szFileName[MAX_PATH];
    StringCchCopy(szFileName, ARRAYSIZE(szFileName), _strFileName.c_str());
    PTSTR pszExt = PathFindExtension(szFileName);
    *pszExt = 0; // Chop off file extension
    return szFileName;
}

std::string ScriptId::GetFullPath() const
{
    std::string fullPath = _strFolder;
    fullPath += "\\";
    fullPath += _strFileNameOrig;
    return fullPath;
}

bool ScriptId::IsHeader() const
{
    const char* pszExt = PathFindExtension(_strFileName.c_str());
    return (strcmp(".sh", pszExt) == 0) ||
        (strcmp(".shm", pszExt) == 0);
    // shp not technically a header, since we support having stuff other than defines in it.
    //(strcmp(TEXT(".shp"), pszExt) == 0);
}

LangSyntax ScriptId::Language() const
{
    // It's ok if language is unknown if the filename is empty.
    assert(_language != LangSyntaxUnknown || _strFileNameOrig.empty());
    return _language;
}

ScriptId::ScriptId(std::string strFolder, std::string strFileName, std::string strFileNameOrig, uint16_t wScriptNum,
                   LangSyntax language)
    : _strFolder(std::move(strFolder)), _strFileName(std::move(strFileName)),
      _strFileNameOrig(std::move(strFileNameOrig)), _wScriptNum(wScriptNum), _language(language)
{
}

void ScriptId::_MakeLower()
{
    std::transform(_strFolder.begin(), _strFolder.end(), _strFolder.begin(), tolower);
    std::transform(_strFileName.begin(), _strFileName.end(), _strFileName.begin(), tolower);
}

void ScriptId::_Init(const char* pszFullFileName, uint16_t wScriptNum)
{
    _wScriptNum = wScriptNum;
    if (pszFullFileName)
    {
        //path fullPath(pszFullFileName);
        //_strFileName = fullPath.filename();
        //_strFolder = fullPath.parent_path();
        // Sigh Microsoft... std::tr2::sys doesn't work with UNC shares...
        std::string str = pszFullFileName;
        int iIndexBS = str.rfind('\\');
        _strFolder = str.substr(0, iIndexBS);
        _strFileName = str.substr(iIndexBS + 1);

        _strFileNameOrig = _strFileName;
        _MakeLower();
        _DetermineLanguage();
    }
}
