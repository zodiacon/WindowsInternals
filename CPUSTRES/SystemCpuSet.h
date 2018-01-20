#pragma once

class SystemCpuSet {
public:
	bool Init();

	SYSTEM_CPU_SET_INFORMATION* GetCpuSets() const {
		return _cpuSets;
	}

	const SYSTEM_CPU_SET_INFORMATION& GetCpuSet(int index) const {
		return _cpuSets[index];
	}

	int GetCount() const {
		return _count;
	}

private:
	std::unique_ptr<BYTE[]> _buffer;
	SYSTEM_CPU_SET_INFORMATION* _cpuSets = nullptr;
	int _count = 0;
};

