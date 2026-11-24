
// IoPriorityDlg.cpp : implementation file
//

#include "stdafx.h"
#include "IoPriority.h"
#include "IoPriorityDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx {
public:
	CAboutDlg();

	// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CIoPriorityDlg dialog



CIoPriorityDlg::CIoPriorityDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_IOPRIORITY_DIALOG, pParent), m_CancelEvent(FALSE, TRUE) {
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CIoPriorityDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CIoPriorityDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START, &CIoPriorityDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_CANCEL, &CIoPriorityDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CIoPriorityDlg message handlers

DWORD CIoPriorityDlg::IoThread(PVOID p) {
	auto pData = reinterpret_cast<ThreadData*>(p);

	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_LOWEST);

	auto hDlg = pData->pThis->m_hWnd;
	CString status;

	// Use the temp directory: the current directory may not be writable
	WCHAR dir[MAX_PATH];
	if (::GetTempPath(_countof(dir), dir) == 0)
		wcscpy_s(dir, L".\\");
	CString filename;
	filename.Format(L"%s_IoPriority_%u.tmp", dir, ::GetCurrentThreadId());
	HANDLE hFile = ::CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		DWORD error = ::GetLastError();
		status.Format(L"Failed to create test file (error %u)", error);
		::SetDlgItemText(hDlg, pData->StatusId, status);
		return error;
	}

	// FILE_FLAG_NO_BUFFERING requires sector-aligned buffers and sizes. VirtualAlloc
	// gives page alignment, and 4 KB is a multiple of every common sector size.
	const DWORD size = 1 << 12;
	auto buffer = static_cast<BYTE*>(::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	DWORD written = 0;
	if (!buffer || !::WriteFile(hFile, buffer, size, &written, nullptr) || written != size) {
		DWORD error = buffer ? ::GetLastError() : ERROR_NOT_ENOUGH_MEMORY;
		status.Format(L"Failed to write test file (error %u)", error);
		::SetDlgItemText(hDlg, pData->StatusId, status);
		if (buffer)
			::VirtualFree(buffer, 0, MEM_RELEASE);
		::CloseHandle(hFile);
		return error;
	}

	ULONGLONG count = 0;	// 64-bit: count * 1000 overflows a DWORD on fast disks
	auto tick = ::GetTickCount();
	bool currentLowPriority = false;

	while (!pData->pCancelEvent->Lock(0)) {
		bool lowPriority = ::IsDlgButtonChecked(hDlg, pData->CheckBoxId) == BST_CHECKED;
		if (lowPriority != currentLowPriority) {
			if (lowPriority)
				::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
			else
				::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
			currentLowPriority = lowPriority;
		}

		::SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
		if (!::ReadFile(hFile, buffer, size, &written, nullptr)) {
			status.Format(L"Read failed (error %u)", ::GetLastError());
			::SetDlgItemText(hDlg, pData->StatusId, status);
			break;
		}
		count += written;
		auto current = ::GetTickCount();
		if (current - tick < 500)
			continue;
		status.Format(L"I/O Speed: %llu bytes/sec", count * 1000 / (current - tick));
		tick = ::GetTickCount();
		::SetDlgItemText(hDlg, pData->StatusId, status);
		count = 0;
	}

	// cancellation

	::VirtualFree(buffer, 0, MEM_RELEASE);
	::CloseHandle(hFile);

	return 0;
}

BOOL CIoPriorityDlg::OnInitDialog() {
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL) {
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty()) {
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CIoPriorityDlg::OnSysCommand(UINT nID, LPARAM lParam) {
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else {
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CIoPriorityDlg::OnPaint() {
	if (IsIconic()) {
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else {
		CDialogEx::OnPaint();
	}
}

HCURSOR CIoPriorityDlg::OnQueryDragIcon() {
	return static_cast<HCURSOR>(m_hIcon);
}



void CIoPriorityDlg::OnBnClickedStart() {
	// create threads

	m_CancelEvent.ResetEvent();

	m_ThreadData[0].CheckBoxId = IDC_LOWPRIORITY1;
	m_ThreadData[0].pCancelEvent = &m_CancelEvent;
	m_ThreadData[0].pThis = this;
	m_ThreadData[0].StatusId = IDC_STATUS1;

	m_ThreadData[1].CheckBoxId = IDC_LOWPRIORITY2;
	m_ThreadData[1].pCancelEvent = &m_CancelEvent;
	m_ThreadData[1].pThis = this;
	m_ThreadData[1].StatusId = IDC_STATUS2;

	GetDlgItem(IDC_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_CANCEL)->EnableWindow(TRUE);

	SetDlgItemText(IDC_STATUS1, L"");
	SetDlgItemText(IDC_STATUS2, L"");

	m_Working = true;
	for (int i = 0; i < 2; i++) {
		m_hThread[i] = ::CreateThread(nullptr, 0, IoThread, m_ThreadData + i, 0, nullptr);
		if (!m_hThread[i]) {
			CString status;
			status.Format(L"Failed to create thread (error %u)", ::GetLastError());
			SetDlgItemText(m_ThreadData[i].StatusId, status);
		}
	}
	if (!m_hThread[0] && !m_hThread[1])
		StopThreads();	// re-enables Start
}


void CIoPriorityDlg::OnBnClickedCancel() {
	StopThreads();
}

void CIoPriorityDlg::StopThreads() {
	if (!m_Working)
		return;
	m_Working = false;

	m_CancelEvent.SetEvent();

	// Wait for each thread in turn: waiting on both handles at once returns when either one exits.
	// The workers SendMessage to this dialog (IsDlgButtonChecked, SetDlgItemText), so keep
	// servicing sent messages while waiting, or both sides deadlock.
	for (auto& hThread : m_hThread) {
		if (!hThread)
			continue;
		for (;;) {
			auto rc = ::MsgWaitForMultipleObjectsEx(1, &hThread, INFINITE, QS_SENDMESSAGE, MWMO_INPUTAVAILABLE);
			if (rc != WAIT_OBJECT_0 + 1)
				break;
			MSG msg;
			::PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
		}
		::CloseHandle(hThread);
		hThread = nullptr;
	}

	GetDlgItem(IDC_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_CANCEL)->EnableWindow(FALSE);
}

// Esc, Enter and the close box all end the dialog through OnOK/OnCancel; stop the
// worker threads first, since they reference this (stack-allocated) dialog.
void CIoPriorityDlg::OnOK() {
	StopThreads();
	CDialogEx::OnOK();
}

void CIoPriorityDlg::OnCancel() {
	StopThreads();
	CDialogEx::OnCancel();
}
