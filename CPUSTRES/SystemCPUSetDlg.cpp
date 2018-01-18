#include "stdafx.h"
#include "SystemCPUSetDlg.h"
#include "resource.h"

CSystemCPUSetDlg::CSystemCPUSetDlg(SYSTEM_CPU_SET_INFORMATION* pCpuSets, int count, CWnd* pParent) : CDialogEx(IDD_CPUSETS, pParent), 
	m_pCpuSets(pCpuSets), m_CpuSetCount(count) {
}

void CSystemCPUSetDlg::DoDataExchange(CDataExchange * pDX) {
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CPUSETS, m_CpuSets);
}

BOOL CSystemCPUSetDlg::OnInitDialog() {
	CDialogEx::OnInitDialog();

	m_CpuSets.InsertColumn(0, L"Index", LVCFMT_LEFT, 50);
	m_CpuSets.InsertColumn(1, L"ID", LVCFMT_CENTER, 80);
	m_CpuSets.InsertColumn(2, L"Group", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(3, L"LP", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(4, L"Core", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(5, L"Cache", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(6, L"Numa Node", LVCFMT_CENTER, 70);
	m_CpuSets.InsertColumn(7, L"Efficiency Class", LVCFMT_CENTER, 90);
	m_CpuSets.InsertColumn(8, L"Flags", LVCFMT_LEFT, 120);

	InitSystemCpuSets();

	return TRUE;
}

void CSystemCPUSetDlg::InitSystemCpuSets() {
	CString str;
	for (int i = 0; i < m_CpuSetCount; i++) {
		const auto& set = m_pCpuSets[i].CpuSet;
		str.Format(L"%d", i + 1);
		int n = m_CpuSets.InsertItem(i, str);
		str.Format(L"%d (0x%X)", set.Id, set.Id);
		m_CpuSets.SetItemText(n, 1, str);

		str.Format(L"%d", set.Group);
		m_CpuSets.SetItemText(n, 2, str);

		str.Format(L"%d", set.LogicalProcessorIndex);
		m_CpuSets.SetItemText(n, 3, str);

		str.Format(L"%d", set.CoreIndex);
		m_CpuSets.SetItemText(n, 4, str);

		str.Format(L"%d", set.LastLevelCacheIndex);
		m_CpuSets.SetItemText(n, 5, str);

		str.Format(L"%d", set.NumaNodeIndex);
		m_CpuSets.SetItemText(n, 6, str);

		str.Format(L"%d", set.EfficiencyClass);
		m_CpuSets.SetItemText(n, 7, str);

		str.Empty();
		if (set.Parked)
			str += L"[Parked] ";
		if (set.Allocated)
			str += L"[Allocated] ";
		if (set.AllocatedToTargetProcess)
			str += "[Allocated to Process] ";
		if (set.RealTime)
			str += "[Realtime]";
		m_CpuSets.SetItemText(n, 8, str);
	}
}

