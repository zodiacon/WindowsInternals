// CpuLimit.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <errno.h>
#include <stdlib.h>

int PrintUsage() {
	printf("CPU Limit - by Pavel Yosifovich (C)2016\n");
	printf("Usage: cpulimit <pid> <percentage>\n");

	return 0;
}

int Error(DWORD code, HANDLE hProcess = nullptr, HANDLE hJob = nullptr) {
	printf("Error: %u\n", code);
	if (hJob)
		::CloseHandle(hJob);
	if (hProcess)
		::CloseHandle(hProcess);
	return code;
}

bool ParseNumber(const char* text, unsigned long& value) {
	char* end;
	errno = 0;
	value = strtoul(text, &end, 10);
	return *text >= '0' && *text <= '9' && *end == 0 && errno == 0;
}

int main(int argc, const char* argv[]) {
	if (argc < 3)
		return PrintUsage();

	// parse process ID and percentage

	unsigned long pid, percent;
	if (!ParseNumber(argv[1], pid)) {
		printf("Invalid process ID: %s\n", argv[1]);
		return 1;
	}
	if (!ParseNumber(argv[2], percent) || percent < 1 || percent > 100) {
		printf("Percentage must be a whole number from 1 to 100\n");
		return 1;
	}

	HANDLE hProcess = ::OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, pid);
	if (!hProcess)
		return Error(::GetLastError());

	HANDLE hJob = ::CreateJobObject(nullptr, nullptr);
	if (!hJob)
		return Error(::GetLastError(), hProcess);

	if (!::AssignProcessToJobObject(hJob, hProcess))
		return Error(::GetLastError(), hProcess, hJob);

	JOBOBJECT_CPU_RATE_CONTROL_INFORMATION info;
	info.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
	info.CpuRate = percent * 100;	// in 1/100 of a percent

	if (!::SetInformationJobObject(hJob, JobObjectCpuRateControlInformation, &info, sizeof(info)))
		return Error(::GetLastError(), hProcess, hJob);

	::CloseHandle(hProcess);
	::CloseHandle(hJob);

	return 0;
}

