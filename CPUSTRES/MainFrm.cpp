
// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "CPUStressEx.h"

#include "MainFrm.h"
#include "AffinityDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_COMMAND(ID_OPTIONS_ALWAYSONTOP, &CMainFrame::OnOptionsAlwaysontop)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_ALWAYSONTOP, &CMainFrame::OnUpdateOptionsAlwaysontop)
	ON_COMMAND(ID_PROCESS_AFFINITY, &CMainFrame::OnProcessAffinity)
	ON_COMMAND_RANGE(ID_PRIORITYCLASS_IDLE, ID_PRIORITYCLASS_REALTIME, OnChangePriorityClass)
	ON_UPDATE_COMMAND_UI_RANGE(ID_PRIORITYCLASS_IDLE, ID_PRIORITYCLASS_REALTIME, OnUpdateChangePriorityClass)
END_MESSAGE_MAP()

// CMainFrame construction/destruction

CMainFrame::CMainFrame() {
	// TODO: add member initialization code here
}

CMainFrame::~CMainFrame() {}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	// increase chance of UI thread to be responsive
	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	if(CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if(!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW, CRect(), this, AFX_IDW_PANE_FIRST)) {
		TRACE0("Failed to create view window\n");
		return -1;
	}

	if(!m_wndDlgBar.Create(this, IDR_MAINFRAME, CBRS_ALIGN_TOP, AFX_IDW_DIALOGBAR)) {
		TRACE0("Failed to create dialogbar\n");
		return -1;
	}

	m_StatusBar.Create(this);
	UINT indicators[] = { ID_STATUS_THREAD, ID_STATUS_PROCESSCPU };

	m_StatusBar.SetIndicators(indicators, _countof(indicators));

	SetProcessAffinityText();
	SetProcessPriorityClassText();

	// main icon window

	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);

	CString title;
	title.LoadStringW(IDR_MAINFRAME);
	CString pid;
	pid.Format(L"%u", ::GetCurrentProcessId());
	title.Replace(L"$", pid);
	SetWindowText(title);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
	if(!CFrameWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const {
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const {
	CFrameWnd::Dump(dc);
}
#endif //_DEBUG


// CMainFrame message handlers

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/) {
	// forward focus to the view window
	m_wndView.SetFocus();
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
	// let the view have first crack at the command

	if(m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling

	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CMainFrame::OnOptionsAlwaysontop() {
	bool onTop = (GetExStyle() & WS_EX_TOPMOST) > 0;
	::SetWindowPos(GetSafeHwnd(), onTop ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}


void CMainFrame::OnUpdateOptionsAlwaysontop(CCmdUI *pCmdUI) {
	pCmdUI->SetCheck((GetExStyle() & WS_EX_TOPMOST) > 0);
}


void CMainFrame::OnProcessAffinity() {
	CAffinityDlg dlg(L"Set Process Affinity");
	if(dlg.DoModal() == IDOK) {
		if(!::SetProcessAffinityMask(::GetCurrentProcess(), dlg.GetSelectedAffinity())) {
			AfxMessageBox(L"Failed to set process affinity mask");
			return;
		}
		SetProcessAffinityText();
	}
}

void CMainFrame::SetProcessAffinityText() {
	DWORD_PTR processAffinity, systemAffinity;
	::GetProcessAffinityMask(::GetCurrentProcess(), &processAffinity, &systemAffinity);
	WCHAR buffer[65];
	_ui64tow_s(processAffinity, buffer, 65, 2);
	m_wndDlgBar.GetDlgItem(IDC_PROCESS_AFFINITY)->SetWindowText(CString(L"Process Affinity: ") + buffer);
}

void CMainFrame::SetProcessPriorityClassText() {
	int priority = ::GetPriorityClass(::GetCurrentProcess());
	m_wndDlgBar.GetDlgItem(IDC_PRIORITYCLASS)->SetWindowText(CString(L"Process Priority Class: ") + PriorityClassToString(priority));
}

PCWSTR CMainFrame::PriorityClassToString(int priority) {
	switch(priority) {
	case NORMAL_PRIORITY_CLASS:
		return L"Normal";
	case IDLE_PRIORITY_CLASS:
		return L"Idle";
	case BELOW_NORMAL_PRIORITY_CLASS:
		return L"Below Normal";
	case ABOVE_NORMAL_PRIORITY_CLASS:
		return L"Above Normal";
	case HIGH_PRIORITY_CLASS:
		return L"High";
	case REALTIME_PRIORITY_CLASS:
		return L"Realtime";
	}
	return nullptr;
}

void CMainFrame::OnChangePriorityClass(UINT id) {
	static int priorities[] = {
		IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS,
		ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS, REALTIME_PRIORITY_CLASS
	};
	::SetPriorityClass(::GetCurrentProcess(), priorities[id - ID_PRIORITYCLASS_IDLE]);
	SetProcessPriorityClassText();
}

void CMainFrame::OnUpdateChangePriorityClass(CCmdUI* pCmdUI) {
	pCmdUI->SetRadio(pCmdUI->m_nID == PriorityClassToID(::GetPriorityClass(::GetCurrentProcess())));
}

int CMainFrame::PriorityClassToID(int priority) {
	switch(priority) {
	case NORMAL_PRIORITY_CLASS:
		return ID_PRIORITYCLASS_NORMAL;
	case IDLE_PRIORITY_CLASS:
		return ID_PRIORITYCLASS_IDLE;
	case BELOW_NORMAL_PRIORITY_CLASS:
		return ID_PRIORITYCLASS_BELOWNORMAL;
	case ABOVE_NORMAL_PRIORITY_CLASS:
		return ID_PRIORITYCLASS_ABOVENORMAL;
	case HIGH_PRIORITY_CLASS:
		return ID_PRIORITYCLASS_HIGH;
	case REALTIME_PRIORITY_CLASS:
		return ID_PRIORITYCLASS_REALTIME;
	}
	return 0;
}
