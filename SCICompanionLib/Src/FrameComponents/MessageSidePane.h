#pragma once

#include "NonViewClient.h"
#include "ExtDialogFwdCmd.h"
#include "NoFlickerStatic.h"
#include "MessageEditorListCtrl.h"

class CMessageDoc;
struct TextComponent;

class MessageSidePane : public CExtDialogFwdCmd, public INonViewClient
{
public:
    MessageSidePane(CWnd* pParent = NULL);   // standard constructor
    virtual ~MessageSidePane();

    // Dialog Data
    enum
    {
        IDD = IDD_MESSAGETOOLBOX,
    };

    void SetDocument(CDocument *pDoc);
    CMessageDoc *GetDocument() { return _pDoc; }

    // INonViewClient
    void UpdateNonView(CObject *pObject) override;

    virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    afx_msg void OnGotoTalkers();
    afx_msg void OnGotoVerbs();
       

    DECLARE_MESSAGE_MAP()

    BOOL OnEraseBkgnd(CDC *pDC);

private:
    void _Update();

    CMessageDoc *_pDoc;

    MessageEditorListCtrl m_wndVerbs;
    MessageEditorListCtrl m_wndTalkers;

    // Visual
    CExtLabel m_wndLabel1;
    CExtLabel m_wndLabel2;
    CExtButton m_wndButton1;
    CExtButton m_wndButton2;

    HACCEL _hAccel;
public:
    afx_msg void OnBnClickedButtonaddverb();
    afx_msg void OnBnClickedButtonaddtalker();
};
