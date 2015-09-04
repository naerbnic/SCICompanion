#pragma once

class CCrystalScriptStream::const_iterator;
class CScriptStreamLimiter;
class SyntaxContext;;

class AutoCompleteChoice
{
public:
    AutoCompleteChoice() { _iIcon = 0; }
    AutoCompleteChoice(PCTSTR psz, int iIcon) { _strText = psz; _iIcon = iIcon; }
    const std::string &GetText() const { return _strText; }
    int GetIcon() const { return _iIcon; }

private:
    std::string _strText;
    int _iIcon;
};

enum ACType
{
    AC_Normal,
    AC_ShowOnFirst_Empty,
    AC_ShowOnFirst_Shown,
};


class AutoCompleteResult
{
public:
    AutoCompleteResult() { Reset(); }
    ~AutoCompleteResult() {}
    void Add(PCTSTR psz, int iIcon) { choices.push_back(AutoCompleteChoice(psz, iIcon)); }
    void AddOnFirstLetter(PCTSTR pszLetters, PCTSTR pszText, int iIcon)
    {
        fACType = pszLetters[0] ? AC_ShowOnFirst_Shown : AC_ShowOnFirst_Empty;
        Add(pszText, iIcon);
    }
    void Clear() { choices.clear(); strMethod.clear(); strParams.clear(); fACType = AC_Normal; }
    void Reset() { dwFailedPos = 0; fResultsChanged = FALSE; Clear(); fACType = AC_Normal; }
    BOOL HasMethodTip() { return !strMethod.empty(); }

    DWORD dwFailedPos;
    BOOL fResultsChanged;
    std::string strFindText;
    std::vector<AutoCompleteChoice> choices;
    ACType fACType;
    CPoint OriginalLimit;
    
    // method info tips
    std::string strMethod;
    std::vector<std::string> strParams;
};

class SyntaxContext; // fwd decl

class AutoCompleteThread2
{
public:
    // TODO: Crit sec to synchronize access to things
    AutoCompleteThread2();
    ~AutoCompleteThread2();

    void InitializeForScript(CCrystalTextBuffer *buffer);
    void StartAutoComplete(CPoint pt, HWND hwnd, UINT message);
    std::unique_ptr<AutoCompleteResult> GetResult(int id);
    void ResetPosition();
    void Exit();

private:
    static UINT s_ThreadWorker(void *pParam);
    void _DoWork();
    CWinThread *_pThread;

    struct AutoCompleteId
    {
        int id;
        HWND hwnd;
        UINT message;
    };

    void _SetResult(std::unique_ptr<AutoCompleteResult> result, AutoCompleteId id);

    // Both
    HANDLE _hStartWork;
    AutoCompleteId _id;
    std::unique_ptr<CScriptStreamLimiter> _limiterPending;
    std::unique_ptr<CCrystalScriptStream> _streamPending;
    CRITICAL_SECTION _cs;
    HANDLE _hWaitForMoreWork;
    HANDLE _hExitThread;

    std::string _additionalCharacters;
    int _idUpdate;
    bool _fCancelCurrentParse;

    // Both
    std::unique_ptr<AutoCompleteResult> _result;
    int _resultId;
    CRITICAL_SECTION _csResult;

    // UI
    int _nextId;
    HWND _lastHWND;
    CPoint _lastPoint;
    CCrystalTextBuffer *_bufferUI;

    // background
};


