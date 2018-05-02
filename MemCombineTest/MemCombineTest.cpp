// MemCombineTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

void RunBusyThread() {
	::QueueUserWorkItem([](auto) -> DWORD {
		WCHAR text[256];
		MEMORYSTATUSEX status = { sizeof(status) };
		for (;;) {
			::Sleep(100);
			::GlobalMemoryStatusEx(&status);
			wsprintfW(text, L"MemCombineTest: Available physical memory = %d MB", (DWORD)(status.ullAvailPhys >> 20));
			::SetConsoleTitle(text);
		}
		return 0;
	}, nullptr, WT_EXECUTEINPERSISTENTTHREAD);
}

int main() {
	printf("PID: %d (0x%X)\n", ::GetCurrentProcessId(), ::GetCurrentProcessId());

	int size = 1 << 16;

	byte* p1 = static_cast<byte*>(::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	byte* p2 = static_cast<byte*>(::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	BYTE pattern[16];
	::srand(::GetTickCount());

	for (int i = 0; i < 16; i++)
		pattern[i] = ::rand() % 256;

	for (int i = 0; i < size; i++) {
		p1[i] = pattern[i % 16];
		p2[i] = pattern[i % 16];
	}

	printf("Allocated buffers and filled both with same random pattern.\n");
	printf("buffer 1: 0x%p\nbuffer 2: 0x%p\n", p1, p2);

	printf("Press any key to make a change to the first page of the first buffer...\n");

	RunBusyThread();

	getchar();

	// change a single byte

	p1[0]++;

	printf("Made a single byte change. Press any key to exit.\n");
	getchar();

	::VirtualFree(p1, 0, MEM_RELEASE | MEM_DECOMMIT);
	::VirtualFree(p2, 0, MEM_RELEASE | MEM_DECOMMIT);

	return 0;
}

