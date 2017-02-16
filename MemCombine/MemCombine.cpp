// MemCombine.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#pragma comment(lib, "ntdll")

enum MemoryCombineFlags {
	None = 0,
	CommonPagesOnly = 4
};

enum SystemInformationClass {
	SystemCombinePhysicalMemoryInformation = 130
};

#define SE_PROF_SINGLE_PROCESS_PRIVILEGE    (13L)

extern "C"
NTSTATUS
NTAPI
RtlAdjustPrivilege(
	ULONG Privilege,
	BOOLEAN Enable,
	BOOLEAN Client,
	PBOOLEAN WasEnabled);

extern "C"
NTSTATUS
NTAPI
NtSetSystemInformation(
	SystemInformationClass SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength);


struct MEMORY_COMBINE_INFORMATION_EX {
	HANDLE Handle;
	ULONG_PTR PagesCombined;
	ULONG Flags;
};


int main(int argc, const char* argv[]) {
	BOOLEAN enabled;
	auto status = RtlAdjustPrivilege(SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &enabled);
	if (status != 0) {
		printf("Error enabling privilege.\n");
		return 1;
	}

	MEMORY_COMBINE_INFORMATION_EX info{};
	info.Flags = MemoryCombineFlags::None;
	if (argc > 1 && _stricmp(argv[1], "common") == 0)
		info.Flags = MemoryCombineFlags::CommonPagesOnly;

	printf("Combining pages, please wait...\n");

	status = ::NtSetSystemInformation(SystemInformationClass::SystemCombinePhysicalMemoryInformation, &info, sizeof(info));
	if (status != 0) {
		printf("Error: %X\n", status);
		return status;
	}

	printf("Success. Total pages combined: %lld\n", (long long)info.PagesCombined);

	return 0;
}

