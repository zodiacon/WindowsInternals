
// CPUStressEx.h : main header file for the CPUStressEx application
//
#pragma once

#ifndef __AFXWIN_H__
#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CCPUStressExApp:
// See CPUStressEx.cpp for the implementation of this class
//

class CCPUStressExApp : public CWinApp {
public:
	CCPUStressExApp();


	// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

	// Implementation

public:
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CCPUStressExApp theApp;
