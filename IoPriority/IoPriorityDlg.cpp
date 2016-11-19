
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
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CIoPriorityDlg message handlers

DWORD CIoPriorityDlg::IoThread(PVOID p) {
	auto pData = reinterpret_cast<ThreadData*>(p);

	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_LOWEST);

	CString filename;
	filename.Format(L"_File_%d", ::GetCurrentThreadId());
	HANDLE hFile = ::CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return ::GetLastError();

	const DWORD size = 1 << 10;
	BYTE buffer[size];
	DWORD written;

	::WriteFile(hFile, buffer, size, &written, nullptr);
	DWORD count = 0;
	auto hDlg = pData->pThis->m_hWnd;
	auto tick = ::GetTickCount();
	CString status;
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
		::ReadFile(hFile, buffer, size, &written, nullptr);
		count += written;
		auto current = ::GetTickCount();
		if (current - tick < 500)
			continue;
		status.Format(L"I/O Speed: %d bytes/sec", count * 1000 / (current - tick));
		tick = ::GetTickCount();
		::SetDlgItemText(hDlg, pData->StatusId, status);
		count = 0;
	}

	// cancellation

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

	for (int i = 0; i < 2; i++)
		m_hThread[i] = ::CreateThread(nullptr, 0, IoThread, m_ThreadData + i, 0, nullptr);

	m_Working = true;
}


void CIoPriorityDlg::OnBnClickedCancel() {
	m_CancelEvent.SetEvent();

	do {
		auto rc = ::MsgWaitForMultipleObjectsEx(2, m_hThread, INFINITE, QS_SENDMESSAGE | QS_PAINT, MWMO_INPUTAVAILABLE);
		if (rc < WAIT_OBJECT_0 + 2)
			break;
		AfxGetApp()->PumpMessage();

	} while (true);

	m_Working = false;

	::CloseHandle(m_hThread[0]);
	::CloseHandle(m_hThread[1]);

	GetDlgItem(IDC_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_CANCEL)->EnableWindow(FALSE);

}


void CIoPriorityDlg::OnClose() {
	if(m_Working)
		OnBnClickedCancel();

	CDialogEx::OnClose();
}
