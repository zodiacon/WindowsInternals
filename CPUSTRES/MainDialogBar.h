#pragma once

class CChildView;

// CMainDialogBar

class CMainDialogBar : public CDialogBar
{
	DECLARE_DYNAMIC(CMainDialogBar)

public:
	CMainDialogBar();
	virtual ~CMainDialogBar();

	void InitControls(CChildView*);

protected:
	CChildView* m_pView;

	void OnSetActivityLevel(UINT id);
	void OnUpdateSetActivityLevel(CCmdUI* pCmdUI);

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};


