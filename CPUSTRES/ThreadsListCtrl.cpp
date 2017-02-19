// ThreadsListCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "CPUStressEx.h"
#include "ThreadsListCtrl.h"


// CThreadsListCtrl

IMPLEMENT_DYNAMIC(CThreadsListCtrl, CMFCListCtrl)

CThreadsListCtrl::CThreadsListCtrl(const std::vector<std::unique_ptr<CThread>>& threads) : m_Threads(threads)
{
}

CThreadsListCtrl::~CThreadsListCtrl()
{
}


COLORREF CThreadsListCtrl::OnGetCellBkColor(int nRow, int nColumn) {
	auto thread = GetThread(nRow);
	if (!thread->IsActive())
		return (COLORREF)Color::White;

	switch (thread->GetActivityLevel()) {
	case ActivityLevel::Low:
		return (COLORREF)Color::Yellow;
	case ActivityLevel::Medium:
		return (COLORREF)Color::Orange;
	case ActivityLevel::Busy:
		return (COLORREF)Color::Red;
	case ActivityLevel::Maximum:
		return (COLORREF)Color::DarkRed;
	default:
		ASSERT(0);
		return 0;
	}
}

COLORREF CThreadsListCtrl::OnGetCellTextColor(int nRow, int nColumn) {
	auto thread = GetThread(nRow);
	auto color = Color::Black;
	if (thread->IsActive() && thread->GetActivityLevel() >= ActivityLevel::Busy)
		color = Color::White;
	return (COLORREF)color;
}

CThread * CThreadsListCtrl::GetThread(int row) const {
	return reinterpret_cast<CThread*>(GetItemData(row));
}

BEGIN_MESSAGE_MAP(CThreadsListCtrl, CMFCListCtrl)
END_MESSAGE_MAP()

// CThreadsListCtrl message handlers


