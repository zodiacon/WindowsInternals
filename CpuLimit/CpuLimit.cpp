// CpuLimit.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int PrintUsage() {
	printf("CPU Limit - by Pavel Yosifovich (C)2016\n");
	printf("Usage: cpulimit <pid> <percentage>\n");

	return 0;
}

int Error(DWORD code) {
	printf("Error: %d\n", code);
	return code;
}

int main(int argc, const char* argv[]) {
	if (argc < 3)
		return PrintUsage();

	// parse process ID

	int pid = atoi(argv[1]);

	HANDLE hProcess = ::OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, pid);
	if (!hProcess)
		return Error(::GetLastError());

	HANDLE hJob = ::CreateJobObject(nullptr, nullptr);
	if (!hJob)
		return Error(::GetLastError());

	if (!::AssignProcessToJobObject(hJob, hProcess))
		return Error(::GetLastError());

	JOBOBJECT_CPU_RATE_CONTROL_INFORMATION info;
	info.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
	info.CpuRate = atoi(argv[2]) * 100;

	if (!::SetInformationJobObject(hJob, JobObjectCpuRateControlInformation, &info, sizeof(info)))
		return Error(::GetLastError());

	::CloseHandle(hProcess);
	::CloseHandle(hJob);

	return 0;
}

