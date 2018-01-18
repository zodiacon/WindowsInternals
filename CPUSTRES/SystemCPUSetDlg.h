#pragma once
#include "afxcmn.h"

class CSystemCPUSetDlg : public CDialogEx {
public:
	CSystemCPUSetDlg(SYSTEM_CPU_SET_INFORMATION* pCpuSets, int count, CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CPUSETS };
#endif

protected:
	void InitSystemCpuSets();

	void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support
	BOOL OnInitDialog() override;

	CListCtrl m_CpuSets;
	int m_CpuSetCount;
	SYSTEM_CPU_SET_INFORMATION* m_pCpuSets;
};

