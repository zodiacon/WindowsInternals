#include "stdafx.h"
#include "SystemCpuSet.h"


bool SystemCpuSet::Init() {
	ULONG length;
	BOOL success = ::GetSystemCpuSetInformation(nullptr, 0, &length, ::GetCurrentProcess(), 0);
	ASSERT(!success);

	if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		return false;
	}

	_buffer = std::make_unique<BYTE[]>(length);
	_cpuSets = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(_buffer.get());
	success = ::GetSystemCpuSetInformation(_cpuSets, length, &length, ::GetCurrentProcess(), 0);
	ASSERT(success);

	_count = length / _cpuSets[0].Size;
	return true;
}

