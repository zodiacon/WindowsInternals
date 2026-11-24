// CpuSet.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cctype>
#include <cerrno>
#include <cstdlib>


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

// Returns pointers to every SYSTEM_CPU_SET_INFORMATION entry; the storage lives in buffer.
// Entries are walked by their Size field, which may grow in future Windows versions.
std::vector<PSYSTEM_CPU_SET_INFORMATION> QueryCpuSets(HANDLE hProcess, std::vector<BYTE>& buffer) {
	std::vector<PSYSTEM_CPU_SET_INFORMATION> sets;
	ULONG length = 0;
	::GetSystemCpuSetInformation(nullptr, 0, &length, hProcess, 0);
	if (length == 0)
		return sets;

	buffer.resize(length);
	if (!::GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()), length, &length, hProcess, 0))
		return sets;

	for (ULONG offset = 0; offset + sizeof(SYSTEM_CPU_SET_INFORMATION) <= length; ) {
		auto set = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data() + offset);
		if (set->Size == 0)
			break;
		sets.push_back(set);
		offset += set->Size;
	}
	return sets;
}

void PrintSystemCpuSets(int pid, int index = -1) {
	HANDLE hProcess = nullptr;
	if (pid) {
		hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!hProcess)
			printf("Error opening process %d (%u); showing system CPU sets only\n\n", pid, ::GetLastError());
	}

	printf("System CPU Sets\n");
	printf("---------------\n");

	std::vector<BYTE> buffer;
	auto cpuSets = QueryCpuSets(hProcess, buffer);
	if (cpuSets.empty())
		printf("Failed to get CPU set information (%u)\n", ::GetLastError());

	int count = static_cast<int>(cpuSets.size());
	printf("Total CPU Sets: %d\n", count);

	for (int i = 0; i < count; i++) {
		if (index < 0 || index == i) {
			printf("\nCPU Set %d\n", i);
			PrintSystemCpuSet(*cpuSets[i], hProcess != nullptr);
		}
	}

	if (hProcess)
		::CloseHandle(hProcess);
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

// Parses a comma-separated list of decimal or 0x-prefixed hex masks into pSet.
// Fails on malformed or out-of-range values, or more than N entries.
template<typename T, size_t N>
bool ParseCPUSet(T (&pSet)[N], const char* text, int& count) {
	count = 0;
	while (*text != '\0') {
		if (count == static_cast<int>(N))
			return false;

		bool hex = text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
		const char* start = text + (hex ? 2 : 0);
		// strtoull would otherwise accept leading whitespace and a sign
		if (!(hex ? ::isxdigit(static_cast<unsigned char>(*start)) : ::isdigit(static_cast<unsigned char>(*start))))
			return false;

		char* end;
		errno = 0;
		unsigned long long value = ::strtoull(start, &end, hex ? 16 : 10);
		if (errno == ERANGE || (*end != ',' && *end != '\0'))
			return false;

		pSet[count++] = static_cast<T>(value);
		if (*end == '\0')
			break;
		text = end + 1;
		if (*text == '\0')
			return false;	// trailing comma
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

// Maps per-group masks (element i = processor group i, bit n = logical processor n in that
// group) to CPU set IDs, using the actual system CPU set table rather than assuming the IDs
// are contiguous from 0x100. Fails if a bit names a processor that doesn't exist.
bool BuildIdsFromMask(const ULONG64* set, ULONG count, std::vector<ULONG>& ids) {
	ULONG64 matched[64] = {};	// bits found, per group (Windows has at most 64 groups)
	if (count > _countof(matched))
		return false;

	std::vector<BYTE> buffer;
	auto cpuSets = QueryCpuSets(nullptr, buffer);

	ids.clear();
	for (auto cpuSet : cpuSets) {
		if (cpuSet->Type != CpuSetInformation)
			continue;
		auto group = cpuSet->CpuSet.Group;
		auto lp = cpuSet->CpuSet.LogicalProcessorIndex;
		if (group < count && lp < 64 && (set[group] & (1ULL << lp))) {
			ids.push_back(cpuSet->CpuSet.Id);
			matched[group] |= 1ULL << lp;
		}
	}

	for (ULONG i = 0; i < count; i++)
		if (matched[i] != set[i]) {
			printf("Mask 0x%llX for group %u includes processors that don't exist\n", set[i], i);
			return false;
		}
	return true;
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

		else if (ArgumentIs(index, "n") || ArgumentIs(index, "p")) {
			if (index == argc - 1) {
				printf("Error: missing value after %s\n", argv[index]);
				PrintUsage();
				return 1;
			}
			if (ArgumentIs(index, "n"))
				n = atoi(argv[index + 1]);
			else
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
		if (count == 1 && systemCpuSet[0] == 0) {
			pSet = nullptr;
			count = 0;
		}

		BOOL success;
		if (hProcess) {
			if (pSet) {
				std::vector<ULONG> ids;
				if (!BuildIdsFromMask(pSet, count, ids)) {
					::CloseHandle(hProcess);
					return 1;
				}
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
