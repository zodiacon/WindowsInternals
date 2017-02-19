#include "stdafx.h"
#include "Globals.h"

using namespace std;

int CGlobals::s_Processors;

int CGlobals::GetProcessorCount() {
	if (s_Processors == 0) {
		s_Processors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	}
	return s_Processors;
}

std::vector<DWORD> CGlobals::EnumerateThreads(DWORD pid) {
	vector<DWORD> threads;

	auto hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	ASSERT(hSnapshot != INVALID_HANDLE_VALUE);

	THREADENTRY32 te = { sizeof(te) };
	if (::Thread32First(hSnapshot, &te)) {
		do {
			if (te.th32OwnerProcessID == pid)
				threads.push_back(te.th32ThreadID);
		} while (::Thread32Next(hSnapshot, &te));
	}

	::CloseHandle(hSnapshot);
	return threads;
}

