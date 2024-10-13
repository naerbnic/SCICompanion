#pragma once

#include <string>

enum LangSyntax
{
    // Don't change these values, they are used to index into comboboxes.
    LangSyntaxUnknown = 2,
    LangSyntaxStudio = 1,
    LangSyntaxSCI = 0,
};

extern const std::string SCILanguageMarker;

LangSyntax _DetermineLanguage(const std::string& firstLine);

static const uint16_t InvalidResourceNumber = 0xffff;

//
// A script description consists of:
// 1) full folder name
// 2) script title
// 2) script file name
//
class ScriptId
{
public:
    ScriptId();
    ScriptId(const std::string& fullPath);
    ScriptId(const char* pszFullFileName);
    ScriptId(const char* pszFileName, const char* pszFolder);
    ScriptId(const ScriptId& src);
    ScriptId& operator=(const ScriptId& src);

    void SetLanguage(LangSyntax lang);

    bool IsNone() const;
    const std::string& GetFileName() const;
    const std::string& GetFolder() const;
    const std::string& GetFileNameOrig() const;

    // e.g. for Main.sc, it returns Main.  For keys.sh, it returns keys
    std::string GetTitle() const;
    // e.g. for Main.sc, it returns main.  For keys.sh, it returns keys
    std::string GetTitleLower() const;

    // Returns the complete path, for loading/saving, etc...
    std::string GetFullPath() const;

    // Set the path w/o changing the resource number.
    void SetFullPath(const std::string& fullPath);

    // Script resource number
    uint16_t GetResourceNumber() const { return _wScriptNum; }
    void SetResourceNumber(uint16_t wScriptNum);

    // Is this a header, or a script file?
    bool IsHeader() const;
    LangSyntax Language() const;

    friend bool operator<(const ScriptId& script1, const ScriptId& script2);

private:
    void _MakeLower();
    void _Init(const char* pszFullFileName, uint16_t wScriptNum = InvalidResourceNumber);
    void _DetermineLanguage();

    std::string _strFolder;
    std::string _strFileName;
    std::string _strFileNameOrig;   // Not lower-cased
    uint16_t _wScriptNum;
    LangSyntax _language;
};