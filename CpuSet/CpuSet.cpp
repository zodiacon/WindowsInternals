// CpuSet.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemAllowedCpuSetsInformation = 168,
	SystemCpuSetInformation = 175,
	SystemCpuSetTagInformation = 176,
} SYSTEM_INFORMATION_CLASS;

typedef enum _PROCESSINFOCLASS {
	ProcessDefaultCpuSetsInformation = 66,
	ProcessAllowedCpuSetsInformation = 67,
} PROCESSINFOCLASS;

#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#define STATUS_SUCCESS		 ((NTSTATUS)0)

extern "C"
NTSTATUS
NTAPI
NtSetInformationProcess(
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_In_reads_bytes_opt_(ProcessInformationLength) PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength
);

extern "C"
NTSTATUS
NTAPI
NtQuerySystemInformationEx(
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength,
	_Out_opt_ PULONG ReturnLength
);

extern "C"
NTSTATUS
NTAPI
NtSetSystemInformation(
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_In_reads_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength
);

extern "C"
NTSTATUS
NTAPI
NtQuerySystemInformation(
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_Out_writes_bytes_to_opt_(SystemInformationLength, *ReturnLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength,
	_Out_opt_ PULONG ReturnLength
);

extern "C"
NTSTATUS
NTAPI
NtQueryInformationProcess(
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_Out_writes_bytes_opt_(ProcessInformationLength) PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength,
	_Out_opt_ PULONG ReturnLength
);

#pragma comment(lib, "ntdll")

void PrintTitle() {
	printf("CpuSet v1.0: Examine and change CPU Sets\nCopyright (C)2016 Pavel Yosifovich\n\n");
}

void PrintUsage() {
	printf("Usage: cpuset [-?] [-n index] [-p pid ] [-s cpusets]\n");
	printf("\t-p\tShow/Change CPU Set for the specified process (otherwise System CPU set)\n");
	printf("\t-n\tShow CPU Set with the specified index (System CPU set only)\n");
	printf("\t-s\tSet the allowed CPU sets. Pass CPU mask.\n");
	printf("\t\tdecimal and hex values supported (prefix hex with 0x)\n");
}

void PrintDecHex(const char* title, unsigned value, bool cr = true) {
	printf("%s%u (0x%X)", title, value, value);
	if (cr)
		printf("\n");
}

void PrintBool(const char* title, BYTE value) {
	printf("%s%s", title, value ? "True" : "False");
}

void PrintSystemCpuSet(SYSTEM_CPU_SET_INFORMATION& cpuSet, bool process = false) {
	PrintDecHex("  Id: ", cpuSet.CpuSet.Id);
	printf("  Group: %d\n", cpuSet.CpuSet.Group);
	printf("  Logical Processor: %d\n", (int)cpuSet.CpuSet.LogicalProcessorIndex);
	printf("  Core: %d\n", (int)cpuSet.CpuSet.CoreIndex);
	printf("  Last Level Cache: %d\n", cpuSet.CpuSet.LastLevelCacheIndex);
	printf("  NUMA Node: %d\n", (int)cpuSet.CpuSet.NumaNodeIndex);
	PrintDecHex("  Flags: ", cpuSet.CpuSet.AllFlags, false);
	PrintBool("  Parked: ", cpuSet.CpuSet.Parked);
	PrintBool("  Allocated: ", cpuSet.CpuSet.Allocated);
	PrintBool("  Realtime: ", cpuSet.CpuSet.RealTime);
	printf("  Tag: %llu", cpuSet.CpuSet.AllocationTag);
	if (process)
		PrintBool("  Allocated to process: ", cpuSet.CpuSet.AllocatedToTargetProcess);
	printf("\n");
}

bool ArgumentIs(int index, const char* value) {
	if (index >= __argc)
		return false;
	auto arg = __argv[index];

	if (*arg != '-' && *arg != '/')
		return false;

	return ::_strcmpi(value, arg + 1) == 0;
}

void PrintSystemCpuSets(int pid, int index = -1) {
	HANDLE hProcess = nullptr;
	if (pid) {
		hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	}

	printf("System CPU Sets\n");
	printf("---------------\n");

	auto size = 0x20 * 64;
	auto cpuSet = static_cast<PSYSTEM_CPU_SET_INFORMATION>(::malloc(size));

	ULONG length;
	auto success = ::GetSystemCpuSetInformation(cpuSet, size, &length, hProcess, 0);

	int count = length / cpuSet->Size;;
	printf("Total CPU Sets: %d\n", count);

	for (int i = 0; i < count; i++) {
		if (index < 0 || index == i) {
			printf("\nCPU Set %d\n", i);
			PrintSystemCpuSet(cpuSet[i], hProcess != nullptr);
		}
	}

	if (hProcess)
		::CloseHandle(hProcess);

	::free(cpuSet);
}

void PrintAllowedCpuSet(int pid) {
	NTSTATUS status;
	ULONG64 sets[20];
	ULONG size;
	if (pid) {
		HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!hProcess) {
			printf("Error opening process (%d)\n", ::GetLastError());
			return;
		}

		ULONG ids[64];
		DWORD count;
		auto success = ::GetProcessDefaultCpuSets(hProcess, ids, _countof(ids), &count);
		if (success) {
			for (ULONG i = 0; i < count; i++) {
				printf("%d: ", i);
				PrintDecHex("", ids[i]);
			}

		}

		status = ::NtQueryInformationProcess(hProcess, ProcessDefaultCpuSetsInformation, sets, sizeof(sets), &size);
	}
	else {
		HANDLE hProcess = nullptr;
		status = ::NtQuerySystemInformationEx(SystemCpuSetInformation, &hProcess, sizeof(hProcess), sets, sizeof(sets), &size);
	}
	if (status == STATUS_SUCCESS) {
		if (size == 0)
			printf("Empty CPU set\n");
		printf("CPU Masks: ");

		for (ULONG i = 0; i < size / sizeof(ULONG64); i++) {
			printf("0x%02I64X ", sets[i]);
		}
		printf("\n");

		//for (ULONG i = 0; i < count; i++) {
		//	printf("%d: ", i);
		//	PrintDecHex("", ids[i]);
		//}
	}
	else {
		printf("Failed to get allowed CPU sets (%d)\n", ::GetLastError());
		return;
	}
}

template<typename T>
bool ParseCPUSet(T* pSet, const char* text, int& count) {
	bool last = false;
	for (int i = 0; !last && i < 32; i++) {
		if (*text == '\0')
			return true;
		auto comma = strchr(text, ',');
		if (comma == text)
			return false;
		if (comma == nullptr) {
			comma = text + strlen(text);
			last = true;
		}
		size_t len = comma - text;
		bool hex = false;
		if (len > 2 && (::strncmp(text, "0x", 2) == 0 || ::strncmp(text, "0X", 2) == 0))
			hex = true;

		int value = std::stoi(std::string(text + (hex ? 2 : 0), comma), nullptr, hex ? 16 : 10);
		pSet[i] = value;
		text = comma + 1;
		count = i + 1;
	}

	return true;
}

int Error(const char* text) {
	auto code = ::GetLastError();
	printf("%s (%d)\n", text, code);
	return code;
}

BOOL SetPrivilege(HANDLE hToken, PCTSTR lpszPrivilege, bool bEnablePrivilege) {
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if (!::LookupPrivilegeValue(nullptr, lpszPrivilege, &luid))
		return FALSE;

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	// Enable the privilege or disable all privileges.

	if (!::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr)) {
		return FALSE;
	}

	if (::GetLastError() == ERROR_NOT_ALL_ASSIGNED)
		return FALSE;

	return TRUE;
}

std::vector<ULONG> BuildIdsFromMask(ULONG64* set, ULONG count) {
	int index = 0x100;	// first set
	std::vector<ULONG> ids;

	for (ULONG i = 0; i < count; i++) {
		auto mask = set[i];
		while (mask) {
			if (mask & 1)
				ids.push_back(index);
			index++;
			mask >>= 1;
		}
	}
	return ids;
}

int main(int argc, char** argv) {
	PrintTitle();

	if (argc == 1) {
		PrintSystemCpuSets(0);
		return 0;
	}

	HANDLE hToken = nullptr;
	::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);

	if (!SetPrivilege(hToken, SE_INC_BASE_PRIORITY_NAME, TRUE)) {
		printf("Access denied. Try running from an elevated command prompt.\n");
		return 1;
	}
	::CloseHandle(hToken);


	int index = 1;
	bool system = false;
	DWORD pid = 0;
	int n = -1;
	ULONG cpuSet[64];
	ULONG64 systemCpuSet[20];
	int count = 0;
	bool change = false;
	bool interrupt = false;

	while (index < argc) {

		if (ArgumentIs(index, "?")) {
			PrintUsage();
			return 0;
		}

		else if (ArgumentIs(index, "k")) {
			system = true;
			index++;
		}

		else if (ArgumentIs(index, "n")) {
			n = atoi(argv[index + 1]);
			index += 2;
		}

		else if (ArgumentIs(index, "p")) {
			pid = atoi(argv[index + 1]);
			index += 2;
		}
		else if (ArgumentIs(index, "s")) {
			if (index == argc - 1) {
				printf("Error: missing CPU set after -s\n");
				PrintUsage();
				return 1;
			}

			if (!ParseCPUSet(systemCpuSet, argv[index + 1], count)) {
				printf("System CPU set list error\n");
				return 1;
			}
			change = true;
			index += 2;
		}
		else {
			printf("Unknown argument.\n");
			return 1;
		}
	}

	if (change) {
		HANDLE hProcess = nullptr;
		if (pid) {
			hProcess = ::OpenProcess(PROCESS_SET_LIMITED_INFORMATION, FALSE, pid);
			if (!hProcess)
				return Error("Failed to open process");
		}

		auto pSet = systemCpuSet;
		if (count == 1 && cpuSet[0] == 0) {
			pSet = nullptr;
			count = 0;
		}

		BOOL success;
		if (hProcess) {
			if (pSet) {
				auto ids = BuildIdsFromMask(pSet, count);
				success = ::SetProcessDefaultCpuSets(hProcess, ids.data(), (ULONG)ids.size());
			}
			else {
				success = ::SetProcessDefaultCpuSets(hProcess, nullptr, 0);
			}
			if (success)
				printf("Process CPU set changed successfully.\n");
			else
				return Error("Failed to change process CPU set");
			::CloseHandle(hProcess);
		}
		else {
			// set system CPU set

			auto status = ::NtSetSystemInformation(SystemAllowedCpuSetsInformation, pSet, count * sizeof(ULONG64));
			if (status == STATUS_SUCCESS)
				printf("System CPU set changed successfully.\n");
			else
				printf("Failed to change system CPU set (%d)\n", status);
		}
	}

	else {
		PrintSystemCpuSets(pid, n);
		if (pid) {
			printf("\n");
			PrintAllowedCpuSet(pid);
		}
	}
	return 0;
}
