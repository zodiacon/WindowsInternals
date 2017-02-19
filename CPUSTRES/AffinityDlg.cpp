// AffinityDlg.cpp : implementation file
//

#include "stdafx.h"
#include "CPUStressEx.h"
#include "AffinityDlg.h"
#include "afxdialogex.h"
#include "Globals.h"
#include "Thread.h"

// CAffinityDlg dialog

IMPLEMENT_DYNAMIC(CAffinityDlg, CDialogEx)

CAffinityDlg::CAffinityDlg(bool affinity, CThread* pThread, const CString& title, CWnd* pParent)
	: CDialogEx(IDD_AFFINITY, pParent), m_Affinity(affinity), m_Title(title), m_pThread(pThread) {
}

CAffinityDlg::CAffinityDlg(const CString& title, CWnd* pParent) : CDialogEx(IDD_AFFINITY, pParent), m_Title(title), m_Process(true), m_Affinity(true) {
}

CAffinityDlg::~CAffinityDlg() {
}

void CAffinityDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	if(!m_Affinity) {
		DDX_Radio(pDX, IDC_FIRST, m_IdealCPU);
	}
}

BEGIN_MESSAGE_MAP(CAffinityDlg, CDialogEx)
	ON_BN_CLICKED(IDC_SELECTALL, &CAffinityDlg::OnBnClickedSelectall)
	ON_BN_CLICKED(IDC_UNSELECTALL, &CAffinityDlg::OnBnClickedUnselectall)
END_MESSAGE_MAP()


// CAffinityDlg message handlers


BOOL CAffinityDlg::OnInitDialog() {

	SetWindowText(m_Title);

	CRect rc;
	GetDlgItem(IDC_DUMMY)->GetClientRect(&rc);
	int cpus = CGlobals::GetProcessorCount();

	DWORD_PTR processAffinity, systemAffinity;
	BOOL validAffinity = FALSE;

	if(m_Affinity && m_Process)
		validAffinity = ::GetProcessAffinityMask(::GetCurrentProcess(), &processAffinity, &systemAffinity);

	CString name;
	int row = cpus > 32 ? 8 : 4;
	auto font = GetFont();
	for(int i = 0; i < cpus; i++) {
		name.Format(L"CPU %d", i);
		m_Buttons[i].Create(name, WS_CHILD | WS_VISIBLE | (m_Affinity ? BS_AUTOCHECKBOX : BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0) | WS_TABSTOP),
			CRect(CPoint(10 + (rc.Width() + 10) * (i % row), 20 + (rc.Height() + 10) * (i / row)), CSize(rc.Width(), rc.Height())),
			this, IDC_FIRST + i);
		if(m_Affinity && m_pThread == nullptr)
			CheckDlgButton(IDC_FIRST + i, (validAffinity && processAffinity & (1i64 << i)) == 0 ? BST_UNCHECKED : BST_CHECKED);
		m_Buttons[i].SetFont(font);
	}

	if(cpus > 16) {
		CRect rcButton, rcWindow;
		GetDlgItem(IDOK)->GetClientRect(&rcButton);
		GetWindowRect(&rcWindow);
		int sizex = row == 4 ? rcWindow.Width() : (rcWindow.Width() - rcButton.Width()) * 2 + rcButton.Width();
		if(row > 4) {
			int x1 = sizex / 2 - rcButton.Width() - 20;
			int x2 = sizex / 2 + 20;
			CRect rcButton2;
			GetDlgItem(IDOK)->GetWindowRect(&rcButton2);
			ScreenToClient(&rcButton2);
			GetDlgItem(IDOK)->MoveWindow(x1, rcButton2.top, rcButton.Width(), rcButton.Height());
			GetDlgItem(IDCANCEL)->MoveWindow(x2, rcButton2.top, rcButton.Width(), rcButton.Height());

			GetDlgItem(IDC_SELECTALL)->MoveWindow(sizex - rcButton.Width() - 30, 20, rcButton.Width(), rcButton.Height());
			GetDlgItem(IDC_UNSELECTALL)->MoveWindow(sizex - rcButton.Width() - 30, 30 + rcButton.Height(), rcButton.Width(), rcButton.Height());
		}
		SetWindowPos(nullptr, 0, 0, sizex, rcWindow.Height(), SWP_NOMOVE | SWP_NOZORDER);
	}

	if(!m_Affinity) {
		GetDlgItem(IDC_SELECTALL)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_UNSELECTALL)->ShowWindow(SW_HIDE);
	}

	if(m_Affinity && m_pThread) {
		auto affinity = m_pThread->GetAffinity();
		int i = 0;
		while(affinity) {
			if(affinity & 1)
				CheckDlgButton(IDC_FIRST + i, BST_CHECKED);
			affinity >>= 1;
			i++;
		}
	}
	if(m_Affinity && !m_Process)
		DisableNonProcessAffinity();

	if(!m_Affinity) {
		m_IdealCPU = m_pThread ? m_pThread->GetIdealCPU() : 0;
	}
	CDialogEx::OnInitDialog();

	return TRUE;
}

void CAffinityDlg::DisableNonProcessAffinity() {
	DWORD_PTR processAffinity, systemAffinity;
	if(!::GetProcessAffinityMask(::GetCurrentProcess(), &processAffinity, &systemAffinity))
		return;

	if(processAffinity == systemAffinity)
		return;

	// disable CPUs that are not part of the process affinity mask
	int n = 0;
	while(processAffinity) {
		if((processAffinity & 1) == 0) {
			CheckDlgButton(IDC_FIRST + n, BST_UNCHECKED);
			m_Buttons[n].EnableWindow(FALSE);
		}
		processAffinity >>= 1;
		++n;
	}
}

DWORD_PTR CAffinityDlg::CalcAffinity() {
	DWORD_PTR affinity = 0;
	for(int i = 0; i < CGlobals::GetProcessorCount(); i++)
		if(m_Buttons[i].GetCheck() == BST_CHECKED)
			affinity |= (DWORD_PTR)1 << i;
	return affinity;
}

void CAffinityDlg::OnOK() {
	UpdateData();

	if(m_Affinity) {
		auto affinity = CalcAffinity();
		if(affinity == 0) {
			AfxMessageBox(L"Affinity of zero is not allowed");
			return;
		}
		m_SelectedAfinity = affinity;
	}
	EndDialog(IDOK);
}

void CAffinityDlg::OnBnClickedSelectall() {
	for(int i = 0; i < CGlobals::GetProcessorCount(); i++)
		CheckDlgButton(IDC_FIRST + i, BST_CHECKED);
}

void CAffinityDlg::OnBnClickedUnselectall() {
	for(int i = 0; i < CGlobals::GetProcessorCount(); i++)
		CheckDlgButton(IDC_FIRST + i, BST_UNCHECKED);
}
