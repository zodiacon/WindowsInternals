#include "stdafx.h"
#include "SystemCPUSetDlg.h"
#include "resource.h"

CSystemCPUSetDlg::CSystemCPUSetDlg(const SystemCpuSet& cpuSet, CWnd* pParent) : CDialogEx(IDD_CPUSETS, pParent), 
	m_SystemCpuSet(cpuSet), m_CpuSetCount(cpuSet.GetCount()) {
}

void CSystemCPUSetDlg::DoDataExchange(CDataExchange * pDX) {
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CPUSETS, m_CpuSets);
}

BOOL CSystemCPUSetDlg::OnInitDialog() {
	CDialogEx::OnInitDialog();

	m_CpuSets.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | (m_AllowSelection ? LVS_EX_CHECKBOXES : 0));
	m_CpuSets.InsertColumn(0, L"Index", LVCFMT_LEFT, 50);
	m_CpuSets.InsertColumn(1, L"ID", LVCFMT_CENTER, 80);
	m_CpuSets.InsertColumn(2, L"Group", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(3, L"LP", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(4, L"Core", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(5, L"Cache", LVCFMT_CENTER, 50);
	m_CpuSets.InsertColumn(6, L"Numa Node", LVCFMT_CENTER, 70);
	m_CpuSets.InsertColumn(7, L"Efficiency Class", LVCFMT_CENTER, 90);
	m_CpuSets.InsertColumn(8, L"Flags", LVCFMT_LEFT, 120);

	InitCpuSets();

	if (m_AllowSelection) {
		if (m_hThread)
			InitThreadCpuSet();
		else
			InitProcessCpuSet();
	}

	return TRUE;
}

void CSystemCPUSetDlg::InitCpuSets() {
	CString str;
	for (int i = 0; i < m_CpuSetCount; i++) {
		const auto& set = m_SystemCpuSet.GetCpuSet(i).CpuSet;

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

void CSystemCPUSetDlg::InitProcessCpuSet() {
	auto cpuSets = std::make_unique<ULONG[]>(m_SystemCpuSet.GetCount());
	ULONG count;
	VERIFY(::GetProcessDefaultCpuSets(::GetCurrentProcess(), cpuSets.get(), m_SystemCpuSet.GetCount(), &count));

	for (int i = 0; i < static_cast<int>(count == 0 ? m_SystemCpuSet.GetCount() : count); i++) {
		m_CpuSets.SetCheck(count == 0 ? i : cpuSets[i] - m_SystemCpuSet.GetCpuSet(0).CpuSet.Id);
	}
}

void CSystemCPUSetDlg::InitThreadCpuSet() {
	auto cpuSets = std::make_unique<ULONG[]>(m_SystemCpuSet.GetCount());
	ULONG count;
	VERIFY(::GetThreadSelectedCpuSets(m_hThread, cpuSets.get(), m_SystemCpuSet.GetCount(), &count));

	for (int i = 0; i < static_cast<int>(count == 0 ? m_SystemCpuSet.GetCount() : count); i++) {
		m_CpuSets.SetCheck(count == 0 ? i : cpuSets[i] - m_SystemCpuSet.GetCpuSet(0).CpuSet.Id);
	}
}

void CSystemCPUSetDlg::OnOK() {
	if (m_AllowSelection) {
		CArray<ULONG, ULONG> set;
		for (int i = 0; i < m_SystemCpuSet.GetCount(); i++) {
			if (m_CpuSets.GetCheck(i)) {
				set.Add(m_SystemCpuSet.GetCpuSet(i).CpuSet.Id);
			}
		}

		if (m_hThread) {
			if (!::SetThreadSelectedCpuSets(m_hThread, set.GetCount() == 0 ? nullptr : set.GetData(), (ULONG)set.GetCount())) {
				AfxMessageBox(L"Failed to set thread selected CPU Set");
			}
		}
		else {
			if (!::SetProcessDefaultCpuSets(::GetCurrentProcess(), set.GetCount() == 0 ? nullptr : set.GetData(), (ULONG)set.GetCount())) {
				AfxMessageBox(L"Failed to set process default CPU Set");
			}
		}
	}

	CDialogEx::OnOK();
}
