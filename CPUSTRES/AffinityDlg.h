#pragma once

class CThread;

// CAffinityDlg dialog

class CAffinityDlg : public CDialogEx {
	DECLARE_DYNAMIC(CAffinityDlg)

public:
	CAffinityDlg(bool affinity, CThread* pThread, const CString& title, CWnd* pParent = nullptr);   
	CAffinityDlg(const CString& title, CWnd* pParent = nullptr);

	virtual ~CAffinityDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AFFINITY };
#endif

	enum { IDC_FIRST = 100 };

	DWORD_PTR GetSelectedAffinity() const {
		return m_SelectedAfinity;
	}
	int GetSelectedCPU() const {
		return m_IdealCPU;
	}

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	DWORD_PTR CalcAffinity();
	void DisableNonProcessAffinity();

	bool m_Affinity;
	bool m_Process = false;
	CString m_Title;
	CButton m_Buttons[64];
	CThread* m_pThread;
	DWORD_PTR m_SelectedAfinity;
	int m_IdealCPU;

	DECLARE_MESSAGE_MAP()
	virtual BOOL OnInitDialog();
	void OnOK() override;
public:
	afx_msg void OnBnClickedSelectall();
	afx_msg void OnBnClickedUnselectall();
};
