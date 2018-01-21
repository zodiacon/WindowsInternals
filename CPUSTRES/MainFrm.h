
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "ChildView.h"

class CMainFrame : public CFrameWnd {

public:
	CMainFrame();
protected:
	DECLARE_DYNAMIC(CMainFrame)

	// Attributes
public:

	// Operations
public:

	// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

	// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	static PCWSTR PriorityClassToString(int priority);
	static int PriorityClassToID(int priority);

protected:  // control bar embedded members
	CDialogBar  m_wndDlgBar;
	CChildView	m_wndView;
	CStatusBar m_StatusBar;

	// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	DECLARE_MESSAGE_MAP()

	void SetProcessAffinityText();

	afx_msg void OnOptionsAlwaysontop();
	afx_msg void OnUpdateOptionsAlwaysontop(CCmdUI *pCmdUI);
	afx_msg void OnProcessAffinity();
	void OnChangePriorityClass(UINT id);
	void OnUpdateChangePriorityClass(CCmdUI* pCmdUI);
	void SetProcessPriorityClassText();
};


