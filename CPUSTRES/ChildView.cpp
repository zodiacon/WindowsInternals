
// ChildView.cpp : implementation of the CChildView class
//

#include "stdafx.h"
#include "CPUStressEx.h"
#include "ChildView.h"
#include "AffinityDlg.h"
#include "Globals.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CChildView

CChildView::CChildView() : m_List(m_Threads) {}

CChildView::~CChildView() {}

BEGIN_MESSAGE_MAP(CChildView, CWnd)
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND(ID_THREAD_ACTIVATE, &CChildView::OnThreadActivate)
	ON_UPDATE_COMMAND_UI(ID_THREAD_ACTIVATE, &CChildView::OnUpdateThreadActivate)
	ON_WM_ERASEBKGND()
	ON_COMMAND(ID_THREAD_DEACTIVATE, &CChildView::OnThreadDeactivate)
	ON_UPDATE_COMMAND_UI(ID_THREAD_DEACTIVATE, &CChildView::OnUpdateThreadDeactivate)

	ON_COMMAND_RANGE(ID_ACTIVITYLEVEL_LOW, ID_ACTIVITYLEVEL_MAXIMUM, OnChangeThreadActivity)
	ON_UPDATE_COMMAND_UI_RANGE(ID_ACTIVITYLEVEL_LOW, ID_ACTIVITYLEVEL_MAXIMUM, OnUpdateChangeThreadActivity)

	ON_COMMAND(ID_PROCESS_CREATETHREAD, &CChildView::OnProcessCreatethread)
	ON_UPDATE_COMMAND_UI(ID_PROCESS_CREATETHREAD, &CChildView::OnUpdateProcessCreatethread)
	ON_COMMAND(ID_PROCESS_CREATE4THREADS, &CChildView::OnProcessCreate4threads)
	ON_UPDATE_COMMAND_UI(ID_PROCESS_CREATE4THREADS, &CChildView::OnUpdateProcessCreate4threads)
	ON_COMMAND_RANGE(ID_PRIORITY_IDLE, ID_PRIORITY_TIMECRITICAL, OnChangeThreadPriority)
	ON_UPDATE_COMMAND_UI_RANGE(ID_PRIORITY_IDLE, ID_PRIORITY_TIMECRITICAL, OnUpdateChangeThreadPriority)

	ON_NOTIFY(NM_RCLICK, IDC_LIST, OnRClickList)
	ON_COMMAND(ID_THREAD_AFFINITY, &CChildView::OnThreadAffinity)
	ON_UPDATE_COMMAND_UI(ID_THREAD_AFFINITY, OnUpdateChangeThreadPriority)

	ON_COMMAND(ID_THREAD_IDEALCPU, &CChildView::OnThreadIdealcpu)
	ON_UPDATE_COMMAND_UI(ID_THREAD_IDEALCPU, OnUpdateChangeThreadPriority)
	ON_COMMAND(ID_THREAD_KILL, &CChildView::OnThreadKill)
	ON_UPDATE_COMMAND_UI(ID_THREAD_KILL, OnUpdateChangeThreadPriority)
	ON_WM_TIMER()
	ON_COMMAND(ID_PROCESS_REFRESH, &CChildView::OnProcessRefresh)
	ON_COMMAND(ID_OPTIONS_AUTOREFRESHTHREADINDICES, &CChildView::OnOptionsAutorefreshthreadindices)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_AUTOREFRESHTHREADINDICES, &CChildView::OnUpdateOptionsAutorefreshthreadindices)
END_MESSAGE_MAP()

void CChildView::DoDataExchange(CDataExchange* pDX) {
	CWnd::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_LIST, m_List);
}

// CChildView message handlers

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) {
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		::LoadCursor(NULL, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), NULL);

	return TRUE;
}

void CChildView::OnRClickList(NMHDR*, LRESULT*) {
	CPoint pt;
	::GetCursorPos(&pt);
	CPoint screen(pt);
	m_List.ScreenToClient(&pt);
	int item = m_List.HitTest(pt);
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXTMENU);
	menu.GetSubMenu(item < 0 ? 1 : 0)->TrackPopupMenu(TPM_RIGHTBUTTON, screen.x, screen.y, AfxGetMainWnd());
}

void CChildView::OnPaint() {
	CPaintDC dc(this);
}

int CChildView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_List.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
		CRect(), this, IDC_LIST);
	m_List.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_FLATSB);

	m_List.InsertColumn(0, L"#", 0, 60);
	m_List.InsertColumn(1, L"ID", LVCFMT_LEFT, 60);
	m_List.InsertColumn(2, L"Active?", LVCFMT_CENTER, 80);
	m_List.InsertColumn(3, L"Activity", LVCFMT_LEFT, 80);
	m_List.InsertColumn(4, L"Priority", LVCFMT_LEFT, 100);
	m_List.InsertColumn(5, L"Ideal CPU", LVCFMT_CENTER, 80);
	m_List.InsertColumn(6, L"Affinity", LVCFMT_RIGHT, 120);

	CreateThreads();

	SetTimer(1, 10000, nullptr);

	return 0;
}

void CChildView::CreateThreads() {
	for (int i = 0; i < 4; i++) {
		auto thread = CreateThread();
		if (i == 0)
			thread->Resume();
		AddThread(thread);
	}
	UpdateThreadProcessIndices();
}

void CChildView::AddThread(unique_ptr<CThread>& thread) {
	CString number;
	number.Format(L"%d", m_List.GetItemCount() + 1);
	int n = m_List.InsertItem(m_List.GetItemCount(), number);
	number.Format(L"%d", thread->GetThreadId());
	m_List.SetItemText(n, 1, number);
	UpdateThread(n, thread.get());

	m_Threads.push_back(move(thread));
}

void CChildView::UpdateThread(int n, const CThread* thread) {
	if (thread == nullptr)
		thread = reinterpret_cast<CThread*>(m_List.GetItemData(n));
	CString str;
	m_List.SetItemText(n, 2, thread->IsActive() ? L"Yes" : L"");
	m_List.SetItemText(n, 3, ActivityLevelToString(thread->GetActivityLevel()));
	m_List.SetItemText(n, 4, ThreadPriorityToString(thread->GetPriority()));
	str.Format(L"%d", thread->GetIdealCPU());
	m_List.SetItemText(n, 5, str);
	WCHAR buffer[65];
	_ui64tow_s(thread->GetAffinity(), buffer, 65, 2);
	m_List.SetItemText(n, 6, buffer);

	m_List.SetItemData(n, reinterpret_cast<DWORD_PTR>(thread));
}

PCWSTR CChildView::ActivityLevelToString(ActivityLevel level) {
	static const PCWSTR Levels[] = { L"Low", L"Medium", L"Busy", L"Maximum" };
	return Levels[(int)level - 1];
}

PCWSTR CChildView::ThreadPriorityToString(int priority) {
	static const PCWSTR Levels[] = {
		L"Idle", L"Lowest", L"Below Normal", L"Normal", L"Above Normal", L"Highest", L"Time Critical"
	};
	return Levels[3 + (::abs(priority) == 15 ? priority - 12 * priority / 15 : priority)];
}

void CChildView::UpdateThreadProcessIndices() {
	auto threads = CGlobals::EnumerateThreads(::GetCurrentProcessId());
	if (m_CurrentThreadIds == threads)
		return;

	m_CurrentThreadIds = threads;

	int i = 0;
	CString text;
	for (auto tid : threads) {
		int j = 0;
		for (auto& thread : m_Threads) {
			if (thread->GetThreadId() == tid) {
				thread->SetProcessIndex(i);
				text.Format(L"%d (%d)", j + 1, i);
				m_List.SetItemText(j, 0, text);
				break;
			}
			j++;
		}
		i++;
	}
}

unique_ptr<CThread> CChildView::CreateThread() {
	auto thread = make_unique<CThread>();
	return thread;
}

void CChildView::OnSize(UINT nType, int cx, int cy) {
	CWnd::OnSize(nType, cx, cy);

	m_List.MoveWindow(0, 0, cx, cy);
}


vector<pair<CThread*, int>> CChildView::GetSelectedThreads() const {
	vector<pair<CThread*, int>> selectedThreads;
	for (auto pos = m_List.GetFirstSelectedItemPosition(); pos; ) {
		int n = m_List.GetNextSelectedItem(pos);
		auto data = m_List.GetItemData(n);

		selectedThreads.push_back(make_pair(reinterpret_cast<CThread*>(data), n));
	}
	return selectedThreads;
}

void CChildView::OnThreadActivate() {
	for (auto& item : GetSelectedThreads()) {
		item.first->Resume();
		UpdateThread(item.second, item.first);
	}
}


void CChildView::OnUpdateThreadActivate(CCmdUI *pCmdUI) {
	pCmdUI->Enable(m_List.GetSelectedCount() > 0);
}


BOOL CChildView::OnEraseBkgnd(CDC* pDC) {
	return TRUE;
}


void CChildView::OnThreadDeactivate() {
	for (auto& item : GetSelectedThreads()) {
		item.first->Suspend();
		UpdateThread(item.second, item.first);
	}
}

void CChildView::OnUpdateThreadDeactivate(CCmdUI *pCmdUI) {
	pCmdUI->Enable(m_List.GetSelectedCount() > 0);
}

void CChildView::OnChangeThreadActivity(UINT id) {
	auto level = (ActivityLevel)(id - ID_ACTIVITYLEVEL_LOW + 1);

	for (auto& item : GetSelectedThreads()) {
		item.first->SetActivityLevel(level);
		UpdateThread(item.second, item.first);
	}
}

void CChildView::OnUpdateChangeThreadActivity(CCmdUI* pCmdUI) {
	pCmdUI->Enable(m_List.GetSelectedCount() > 0);
}


void CChildView::OnProcessCreatethread() {
	AddThread(CreateThread());
	UpdateThreadProcessIndices();
}


void CChildView::OnUpdateProcessCreatethread(CCmdUI *pCmdUI) {
	pCmdUI->Enable(m_Threads.size() < 64);
}


void CChildView::OnProcessCreate4threads() {
	for (int i = 0; i < 4; i++) {
		AddThread(CreateThread());
	}
	UpdateThreadProcessIndices();
}


void CChildView::OnUpdateProcessCreate4threads(CCmdUI *pCmdUI) {
	pCmdUI->Enable(m_Threads.size() < 60);
}

void CChildView::OnChangeThreadPriority(UINT id) {
	for (auto& item : GetSelectedThreads()) {
		item.first->SetPriority(IndexToPriority(id - ID_PRIORITY_IDLE));
		UpdateThread(item.second, item.first);
	}
}
void CChildView::OnUpdateChangeThreadPriority(CCmdUI* pCmdUI) {
	pCmdUI->Enable(m_List.GetFirstSelectedItemPosition() != nullptr);
}

int CChildView::IndexToPriority(int index) {
	static int priorities[] = {
		THREAD_PRIORITY_IDLE, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL,
		THREAD_PRIORITY_NORMAL,
		THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST, THREAD_PRIORITY_TIME_CRITICAL
	};
	return priorities[index];
}

void CChildView::OnThreadAffinity() {
	auto threads = GetSelectedThreads();
	CAffinityDlg dlg(true, threads.size() == 1 ? threads[0].first : nullptr, L"Select CPU Affinity");
	if (dlg.DoModal() == IDOK) {
		for (auto& item : GetSelectedThreads()) {
			item.first->SetAfinity(dlg.GetSelectedAffinity());
			UpdateThread(item.second, item.first);
		}
	}
}

void CChildView::OnThreadIdealcpu() {
	auto threads = GetSelectedThreads();
	CAffinityDlg dlg(false, threads.size() == 1 ? threads[0].first : nullptr, L"Select Ideal CPU");
	if (dlg.DoModal() == IDOK) {
		for (auto& item : GetSelectedThreads()) {
			item.first->SetIdealCPU(dlg.GetSelectedCPU());
			UpdateThread(item.second, item.first);
		}
	}
}

void CChildView::OnThreadKill() {
	auto threads = GetSelectedThreads();
	CString text;
	text.Format(L"Terminate %d thread(s)?", threads.size());
	if (AfxMessageBox(text, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDNO)
		return;

	int offset = 0;
	for (size_t i = 0; i < threads.size(); i++) {
		auto& item = threads[i];
		item.first->Terminate();
		int index = item.second;
		m_List.DeleteItem(index - offset);
		m_Threads.erase(m_Threads.begin() + (index - offset));
		offset++;
	}
	UpdateThreadProcessIndices();
}

void CChildView::OnTimer(UINT_PTR nIDEvent) {
	UpdateThreadProcessIndices();
}

void CChildView::OnProcessRefresh() {
	UpdateThreadProcessIndices();
}


void CChildView::OnOptionsAutorefreshthreadindices() {
	m_AutoRefreshThreadIndices = !m_AutoRefreshThreadIndices;
	if (m_AutoRefreshThreadIndices)
		SetTimer(1, 10000, nullptr);
	else
		KillTimer(1);
}


void CChildView::OnUpdateOptionsAutorefreshthreadindices(CCmdUI *pCmdUI) {
	pCmdUI->SetCheck(m_AutoRefreshThreadIndices);
}
