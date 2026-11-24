#include "stdafx.h"
#include "Globals.h"

using namespace std;

int CGlobals::s_Processors;
int CGlobals::s_GroupProcessors;

int CGlobals::GetProcessorCount() {
	if (s_Processors == 0) {
		s_Processors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	}
	return s_Processors;
}

int CGlobals::GetGroupProcessorCount() {
	if (s_GroupProcessors == 0) {
		GROUP_AFFINITY affinity{};
		WORD group = 0;
		if (::GetThreadGroupAffinity(::GetCurrentThread(), &affinity))
			group = affinity.Group;
		int count = static_cast<int>(::GetActiveProcessorCount(group));
		if (count <= 0 || count > static_cast<int>(sizeof(DWORD_PTR) * 8))
			count = min(GetProcessorCount(), static_cast<int>(sizeof(DWORD_PTR) * 8));
		s_GroupProcessors = count;
	}
	return s_GroupProcessors;
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

