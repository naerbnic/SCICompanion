/***************************************************************************
    Copyright (c) 2015 Philip Fortier

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/
// NewRoomDialog.cpp : implementation file
//

#include "stdafx.h"
#include "AppState.h"
#include "NewScriptDialog.h"
#include "resource.h"
#include "ScriptOM.h"

// CNewScriptDialog dialog

IMPLEMENT_DYNAMIC(CNewScriptDialog, CDialog)

CNewScriptDialog::CNewScriptDialog(UINT nID, CWnd* pParent) : CExtResizableDialog(nID, pParent)
{
    _scriptId.SetLanguage(appState->GetResourceMap().Helper().GetDefaultGameLanguage());
}

CNewScriptDialog::CNewScriptDialog(CWnd* pParent /*=NULL*/)
	: CExtResizableDialog(CNewScriptDialog::IDD, pParent)
{
    _scriptId.SetLanguage(appState->GetResourceMap().Helper().GetDefaultGameLanguage());
}

void CNewScriptDialog::_DiscoveredScriptName(PCTSTR pszName)
{
    // Nothing.
}

int CNewScriptDialog::_GetSuggestedScriptNumber()
{
    int iRet = 0;
    // Now find an "empty" slot. 
    // REVIEW: Need a better algorithm here.
    int lastUsed = 0;
    for (int used : _usedScriptNumbers)
    {
        if ((used > (lastUsed + 1)) && (lastUsed > _GetMinSuggestedScriptNumber()))
        {
            return lastUsed + 1;
        }
        lastUsed = used;
    }
    return iRet;
}

void CNewScriptDialog::_PrepareDialog()
{
    // Look through game.ini for a vacant room number.
    DWORD nSize = 5000;
    auto pszNameValues = std::make_unique<TCHAR[]>(nSize);
    auto const& config_store = appState->GetResourceMap().Helper().GetConfigStore();
    auto section_entries = config_store.GetSectionEntries("Script");
    _usedScriptNumbers.clear();

    for (auto const& [key, value] : section_entries)
    {
        std::string_view key_view(key);
        std::string_view value_view(value);
        // The format is
        // n000, ScriptName
        std::string number_text(key_view.substr(1));
        _DiscoveredScriptName(number_text.c_str());

        int iScript = StrToInt(number_text.c_str());

        // Take note of the script number.
        if (iScript >= 0)
        {
            // We can end up with turd entries lying around in game.ini, so check that the file actually exists:
            std::string filename = appState->GetResourceMap().Helper().GetScriptFileName(static_cast<uint16_t>(iScript));
            if (PathFileExists(filename.c_str()))
            {
                _usedScriptNumbers.insert(iScript);
            }
        }
    }
    int iSuggestedRoom = _GetSuggestedScriptNumber();

    // Suggest a room:
    TCHAR szNumber[5];
    StringCchPrintf(szNumber, ARRAYSIZE(szNumber), TEXT("%d"), iSuggestedRoom);
    m_wndEditScriptNumber.SetWindowText(szNumber);

}
void CNewScriptDialog::_AttachControls(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_SCRIPTNAME, m_wndEditScriptName);
    DDX_Control(pDX, IDC_EDITROOMNUMBER, m_wndEditScriptNumber);

    DDX_Control(pDX, IDC_STATICSCRIPTNUMBER, m_wndScriptName);
    DDX_Control(pDX, IDC_STATICSCRIPTNAME, m_wndScriptNumber);

    m_wndEditScriptNumber.SetLimitText(5);
}

void CNewScriptDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
    ShowSizeGrip(FALSE);

    // Visuals
    DDX_Control(pDX, IDOK, m_wndOk);
    DDX_Control(pDX, IDCANCEL, m_wndCancel);

    _AttachControls(pDX);
    _PrepareDialog();
}

void CNewScriptDialog::_PrepareBuffer()
{
    sci::Script script(_scriptId);
    if (appState->GetVersion().SeparateHeapResources)
    {
        // e.g. for SCI0, keep SCIStudio compatible. Otherwise, use version 2
        script.SyntaxVersion = 2;
    }

    std::stringstream ss;
    sci::SourceCodeWriter out(ss, script.Language());
    out.pszNewLine = "\r\n";
    script.OutputSourceCode(out);
    _strBuffer = ss.str();
}


BEGIN_MESSAGE_MAP(CNewScriptDialog, CDialog)
END_MESSAGE_MAP()

//
// Returns TRUE if the chosen script number is valid
//
BOOL CNewScriptDialog::_ValidateScriptNumber()
{
    BOOL fValid = FALSE;
    // Validate the script number
    CString strNumber;
    m_wndEditScriptNumber.GetWindowText(strNumber);
    _scriptId = ScriptId(); // Reset, or else it asserts if we already set a number on it.
    _scriptId.SetLanguage(appState->GetResourceMap().Helper().GetDefaultGameLanguage());
    _scriptId.SetResourceNumber(StrToInt(strNumber));
    int value = _scriptId.GetResourceNumber();
    if (contains(_usedScriptNumbers, value))
    {
        TCHAR szMessage[MAX_PATH];
        StringCchPrintf(szMessage, ARRAYSIZE(szMessage), TEXT("Script %03d already exists.  Please use another number."), _scriptId.GetResourceNumber());
        AfxMessageBox(szMessage, MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL);
        fValid = TRUE;
    }
    return fValid;
}

// CNewScriptDialog message handlers

void CNewScriptDialog::OnOK()
{
    BOOL fClose = !_ValidateScriptNumber();

    WORD nScript = _scriptId.GetResourceNumber();

    if (fClose)
    {
        // Prepare the script name.
        // (Without the .sc extension)
        CString strName;
        m_wndEditScriptName.GetWindowText(strName);
        if (!strName.IsEmpty())
        {
            StringCchCopy(_szScriptName, ARRAYSIZE(_szScriptName), (PCTSTR)strName);
            _scriptId.SetFullPath(appState->GetResourceMap().Helper().GetScriptFileName(_szScriptName));
        }
        else
        {
            fClose = FALSE;
            AfxMessageBox(TEXT("Please enter a script name."), MB_OK | MB_APPLMODAL | MB_ICONEXCLAMATION);
        }
    }

    if (fClose)
    {
        _PrepareBuffer();
        __super::OnOK();
    }
}