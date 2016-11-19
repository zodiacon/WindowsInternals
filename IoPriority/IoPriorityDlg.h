
// IoPriorityDlg.h : header file
//

#pragma once


// CIoPriorityDlg dialog
class CIoPriorityDlg : public CDialogEx
{
// Construction
public:
	CIoPriorityDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_IOPRIORITY_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	struct ThreadData {
		CIoPriorityDlg* pThis;
		CEvent* pCancelEvent;
		UINT CheckBoxId;
		UINT StatusId;
	};

	HANDLE m_hThread[2];
	ThreadData m_ThreadData[2];
	CEvent m_CancelEvent;
	bool m_Working = false;

	static DWORD CALLBACK IoThread(PVOID);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnClose();
};
