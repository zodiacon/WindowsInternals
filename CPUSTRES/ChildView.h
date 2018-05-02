
#pragma once

#include "Thread.h"
#include "ThreadsListCtrl.h"

struct AppSettings {
	int InitialThreads;
	int ActiveThreads;
	ActivityLevel ActivityLevel;
};

// CChildView window

class CChildView : public CWnd {
	// Construction
public:
	CChildView();

	// Attributes
public:

	enum { IDC_LIST = 1234 };

	// Operations
public:

	// Overrides
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
public:
	virtual ~CChildView();

private:
	CThreadsListCtrl m_List;
	std::vector < std::unique_ptr<CThread>> m_Threads;
	std::vector<DWORD> m_CurrentThreadIds;
	bool m_AutoRefreshThreadIndices = true;
	int m_TotalThreads;
	LARGE_INTEGER m_LastQueryCount, m_QueryFrequency;
	long long m_LastProcessTimes = 0;
	double m_ProcessCPU;
	HANDLE m_hJob = nullptr;
	int m_CpuUpdateInterval = 1000, m_CpuUpdateIndex = 1;

	void CreateThreads();
	std::unique_ptr<CThread> CreateThread();
	void AddThread(std::unique_ptr<CThread>& thread);
	static PCWSTR ActivityLevelToString(ActivityLevel level);
	static PCWSTR ThreadPriorityToString(int priority);
	void UpdateThreadProcessIndices();
	BOOL QueueItemForThreadPool(int busyPercent);
	static DWORD CALLBACK BurnSomeCycles(int percent);
	void UpdateCPUTimes();
	AppSettings ReadConfiguration();

	// Generated message map functions
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	std::vector<std::pair<CThread*, int>> GetSelectedThreads() const;
	void UpdateThread(int n, const CThread* thread = nullptr);
	static int IndexToPriority(int index);
	void OnThreadActivate();
	void OnUpdateThreadActivate(CCmdUI *pCmdUI);
	BOOL OnEraseBkgnd(CDC* pDC);
	void OnThreadDeactivate();
	void OnUpdateThreadDeactivate(CCmdUI *pCmdUI);
	void OnRClickList(NMHDR*, LRESULT*);

	void OnChangeThreadActivity(UINT id);
	void OnUpdateChangeThreadActivity(CCmdUI* pCmdUI);
	void OnProcessCreatethread();
	void OnUpdateProcessCreatethread(CCmdUI *pCmdUI);
	void OnProcessCreate4threads();
	void OnUpdateProcessCreate4threads(CCmdUI *pCmdUI);
	void OnChangeThreadPriority(UINT id);
	void OnUpdateChangeThreadPriority(CCmdUI* pCmdUI);
	
	afx_msg void OnThreadAffinity();
	afx_msg void OnThreadIdealcpu();
//	afx_msg void OnProcessAffinity();
//	afx_msg void OnUpdateProcessAffinity(CCmdUI *pCmdUI);
	afx_msg void OnThreadKill();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnProcessRefresh();
	afx_msg void OnOptionsAutorefreshthreadindices();
	afx_msg void OnUpdateOptionsAutorefreshthreadindices(CCmdUI *pCmdUI);
	afx_msg void OnCpusetsSystemcpuset();
	afx_msg void OnCpusetsProcesscpuset();
	afx_msg void OnCpusetsThreadselectedcpuset();
	afx_msg void OnUpdateCpusetsThreadselectedcpuset(CCmdUI *pCmdUI);
	afx_msg void OnProcessQueuethreadpoolwork();

	void OnUpdateStatusThreads(CCmdUI*);
	void OnUpdateStatusProcessCpu(CCmdUI*);
	afx_msg void OnProcessCpuratelimit();
	void OnCpuUpdateInterval(UINT id);
	void OnUpdateCpuUpdateInterval(CCmdUI*);
};

