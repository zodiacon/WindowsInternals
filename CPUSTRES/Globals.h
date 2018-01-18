#pragma once

class CGlobals {
public:
	static int GetProcessorCount();

	static std::vector<DWORD> EnumerateThreads(DWORD pid);

private:
	static int s_Processors;
};

