// Thread.cpp : implementation file
//

#include "stdafx.h"
#include "CPUStressEx.h"
#include "Thread.h"
#include "Globals.h"

// CThread

CThread::CThread() {
	int cpus = CGlobals::GetProcessorCount();
	if(cpus == sizeof(DWORD_PTR) * 8)
		m_Affinity = (DWORD_PTR)-1;
	else
		m_Affinity = (((DWORD_PTR)1) << cpus) - 1;
	m_hTerminate = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	m_hThread = ::CreateThread(nullptr, 0, ThreadFunction, this, CREATE_SUSPENDED, &m_ID);
}

CThread::~CThread() {
	::CloseHandle(m_hThread);
	::CloseHandle(m_hTerminate);
} 

void CThread::Suspend() {
	if (!IsActive()) return;

	::SuspendThread(m_hThread);
	m_Active = false;
}

void CThread::Resume() {
	if (IsActive()) return;

	::ResumeThread(m_hThread);
	m_Active = true;
}

DWORD CThread::ThreadFunction(PVOID p) {
	return reinterpret_cast<CThread*>(p)->ThreadFunction();
}

DWORD CThread::ThreadFunction() {
	auto hTerminate = m_hTerminate;
	for (;;) {
		if (::WaitForSingleObject(hTerminate, 0) == WAIT_OBJECT_0)
			break;
		
		auto level = m_ActivityLevel;
		if (level != ActivityLevel::Maximum) {
			auto time = ::GetTickCount();
			while (::GetTickCount() - time < (unsigned)level * 25)
				;
			::Sleep(100 - (int)level * 25);
		}
	}

	return 0;
}

void CThread::SetAfinity(DWORD_PTR affinity) {
	::SetThreadAffinityMask(m_hThread, m_Affinity = affinity);
}

DWORD_PTR CThread::GetAffinity() const {
	return m_Affinity;
}

DWORD CThread::SetIdealCPU(int n) {
	return ::SetThreadIdealProcessor(m_hThread, n);
}

int CThread::GetIdealCPU() const {
	PROCESSOR_NUMBER n;
	::GetThreadIdealProcessorEx(m_hThread, &n);
	return n.Number;
}

void CThread::Terminate() {
	m_ActivityLevel = ActivityLevel::None;
	::SetEvent(m_hTerminate);
	Resume();
	if(::WaitForSingleObject(m_hThread, 500) == WAIT_TIMEOUT)
		::TerminateThread(m_hThread, 1);
}


