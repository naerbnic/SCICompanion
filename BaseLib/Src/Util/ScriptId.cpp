#include "ScriptId.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>

#include <Shlwapi.h>
#include <strsafe.h>

const std::string SCILanguageMarker = "Sierra Script";

LangSyntax _DetermineLanguage(const std::string& firstLine)
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

ScriptId::ScriptId() : _language(LangSyntaxUnknown) { _wScriptNum = InvalidResourceNumber; };

ScriptId::ScriptId(const std::string& fullPath) : _language(LangSyntaxUnknown)
{
    _Init(fullPath.c_str());
}
ScriptId::ScriptId(const char* pszFullFileName) : _language(LangSyntaxUnknown)
{
    _Init(pszFullFileName);
}
ScriptId::ScriptId(const char* pszFileName, const char* pszFolder) : _language(LangSyntaxUnknown)
{
    assert(std::strchr(pszFileName, '\\') == nullptr); // Ensure file and path are not mixed up.
    _strFileName = pszFileName;
    _strFolder = pszFolder;
    _MakeLower();
    _wScriptNum = InvalidResourceNumber;
}

ScriptId::ScriptId(const ScriptId& src)
{
    _strFolder = src.GetFolder();
    _strFileName = src.GetFileName();
    _strFileNameOrig = src._strFileNameOrig;
    _MakeLower();
    _wScriptNum = src.GetResourceNumber();
    _language = src._language;
}

ScriptId& ScriptId::operator=(const ScriptId& src)
{
    _strFolder = src.GetFolder();
    _strFileName = src.GetFileName();
    _strFileNameOrig = src._strFileNameOrig;
    _MakeLower();
    _wScriptNum = src.GetResourceNumber();
    _language = src._language;
    return(*this);
}

// Set the path w/o changing the resource number.
void ScriptId::SetFullPath(const std::string& fullPath)
{
    _Init(fullPath.c_str(), GetResourceNumber());
}

void ScriptId::_DetermineLanguage()
{
    if (_language == LangSyntaxUnknown)
    {
        // Sniff the file
        LangSyntax langSniff = LangSyntaxUnknown;
        std::ifstream file(GetFullPath());
        std::string line;
        if (std::getline(file, line))
        {
            langSniff = ::_DetermineLanguage(line);
        }
        else
        {
            // This can happen if the file doesn't exist.
            langSniff = LangSyntaxSCI;
        }
        _language = langSniff;
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
