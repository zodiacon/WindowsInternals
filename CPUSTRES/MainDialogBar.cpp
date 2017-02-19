// MainDialogBar.cpp : implementation file
//

#include "stdafx.h"
#include "CPUStressEx.h"
#include "MainDialogBar.h"
#include "ChildView.h"

// CMainDialogBar

IMPLEMENT_DYNAMIC(CMainDialogBar, CDialogBar)

CMainDialogBar::CMainDialogBar() {
}

CMainDialogBar::~CMainDialogBar() {
}


BEGIN_MESSAGE_MAP(CMainDialogBar, CDialogBar)
	ON_WM_CREATE()
	ON_COMMAND_RANGE(ID_ACTIVITYLEVEL_LOW, ID_ACTIVITYLEVEL_MAXIMUM, OnSetActivityLevel)
	ON_UPDATE_COMMAND_UI_RANGE(ID_ACTIVITYLEVEL_LOW, ID_ACTIVITYLEVEL_MAXIMUM, OnUpdateSetActivityLevel)
END_MESSAGE_MAP()

// CMainDialogBar message handlers


int CMainDialogBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	if (CDialogBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

void CMainDialogBar::OnSetActivityLevel(UINT id) {
	m_pView->OnChangeThreadActivity(id);
}

void CMainDialogBar::OnUpdateSetActivityLevel(CCmdUI* pCmdUI) {
	m_pView->OnUpdateChangeThreadActivity(pCmdUI);
}

void CMainDialogBar::InitControls(CChildView* pView) {
	m_pView = pView;

	// menu button

	auto pButton = (CMFCMenuButton*)GetDlgItem(IDC_THREAD_ACTIVITY);
	CMenu menu;
	menu.LoadMenuW(IDR_CONTEXTMENU);
	pButton->m_hMenu = menu.GetSubMenu(0)->m_hMenu;
	menu.Detach();
}

