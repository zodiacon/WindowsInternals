#pragma once

#include "SystemCpuSet.h"

class CSystemCPUSetDlg : public CDialogEx {
public:
	CSystemCPUSetDlg(const SystemCpuSet& cpuSets, CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CPUSETS };
#endif
	void SetAllowSelection(bool allow, HANDLE hThread = nullptr) {
		m_AllowSelection = allow;
		m_hThread = hThread;
	}

protected:
	void InitCpuSets();
	void InitProcessCpuSet();
	void InitThreadCpuSet();
	void OnOK() override;

	void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support
	BOOL OnInitDialog() override;

	CListCtrl m_CpuSets;
	int m_CpuSetCount;
	const SystemCpuSet& m_SystemCpuSet;
	HANDLE m_hThread = nullptr;
	bool m_AllowSelection = false;
};

