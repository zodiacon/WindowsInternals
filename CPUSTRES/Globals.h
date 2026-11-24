#pragma once

class CGlobals {
public:
	// All active processors in the system (for normalizing CPU usage)
	static int GetProcessorCount();
	// Processors in the calling thread's processor group (at most 64). Affinity masks
	// (DWORD_PTR) only address processors within a single group.
	static int GetGroupProcessorCount();

	static std::vector<DWORD> EnumerateThreads(DWORD pid);

private:
	static int s_Processors;
	static int s_GroupProcessors;
};

