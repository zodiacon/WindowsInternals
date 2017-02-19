#pragma once


// CThreadsListCtrl

enum class Color : unsigned {
	White = RGB(255, 255, 255),
	Black = 0,
	DarkRed = RGB(128, 0, 0),
	Red = RGB(255, 0, 0),
	Orange = RGB(255, 128, 0),
	Yellow = RGB(255, 255, 0)
};

#include "Thread.h"

class CThreadsListCtrl : public CMFCListCtrl {
	DECLARE_DYNAMIC(CThreadsListCtrl)

public:
	CThreadsListCtrl(const std::vector<std::unique_ptr<CThread>>& threads);
	virtual ~CThreadsListCtrl();

	COLORREF OnGetCellBkColor(int nRow, int nColumn);
	COLORREF OnGetCellTextColor(int nRow, int nColumn);

private:
	CThread* GetThread(int row) const;

protected:
	const std::vector<std::unique_ptr<CThread>>& m_Threads;

	DECLARE_MESSAGE_MAP()
};


