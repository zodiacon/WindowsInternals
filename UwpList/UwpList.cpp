// UwpList.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "resource.h"

using namespace std;

map<wstring, wstring> capabilitiesMap;

PCSTR ArchToString(UINT32 arch) {
	switch (arch) {
	case APPX_PACKAGE_ARCHITECTURE_X86: return "x86";
	case APPX_PACKAGE_ARCHITECTURE_ARM: return "ARM";
	case APPX_PACKAGE_ARCHITECTURE_X64: return "x64";
	case APPX_PACKAGE_ARCHITECTURE_NEUTRAL: return "Neutral";
	}

	return "Unknown";
}

PCSTR VersionToString(const PACKAGE_VERSION* version) {
	static char buffer[256];
	sprintf_s(buffer, "%d.%d.%d.%d", (int)version->Major, (int)version->Minor, (int)version->Build, (int)version->Revision);
	return buffer;
}

void DisplayCapability(const SID_AND_ATTRIBUTES& saa) {
	PWSTR sid;
	::ConvertSidToStringSid(saa.Sid, &sid);
	auto it = capabilitiesMap.find(sid);
	if (it != capabilitiesMap.end()) {
		printf("%ws (%ws)", it->second.c_str(), sid);
	}
	else {
		printf("%ws", sid);
	}
	::LocalFree(sid);

	if (saa.Attributes & SE_GROUP_ENABLED)
		printf(" (ENABLED)");

	printf("\n");
}

void DisplayProcessInfo(HANDLE hProcess, DWORD pid) {
	HANDLE hToken;
	if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
		return;

	DWORD size;

	// is this an AppContainer?
	DWORD isAppContainer;
	if (!::GetTokenInformation(hToken, TokenIsAppContainer, &isAppContainer, sizeof(DWORD), &size) || !isAppContainer) {
		::CloseHandle(hToken);
		return;
	}

	UINT len = 0;
	auto error = ::GetPackageFullName(hProcess, &len, nullptr);

	PCWSTR packageName{};
	unique_ptr<wchar_t[]> name;
	if (error == ERROR_INSUFFICIENT_BUFFER) {
		name = make_unique<wchar_t[]>(len);
		error = ::GetPackageFullName(hProcess, &len, name.get());
		packageName = name.get();
	}

	printf("Process ID: %6d\n", pid);
	printf("------------------\n");

	wchar_t exeName[MAX_PATH];
	DWORD count = MAX_PATH;
	if (::QueryFullProcessImageName(hProcess, 0, exeName, &count)) {
		printf("Image name: %ws\n", exeName);
	}

	len = 1024;
	auto buffer = make_unique<BYTE[]>(len);
	if (ERROR_SUCCESS == ::GetPackageId(hProcess, &len, buffer.get())) {
		auto id = reinterpret_cast<PACKAGE_ID*>(buffer.get());
		printf("Package name: %ws\n", id->name);
		printf("Publisher: %ws\n", id->publisher);
		printf("Published ID: %ws\n", id->publisherId);
		printf("Architecture: %s\n", ArchToString(id->processorArchitecture));
		printf("Version: %s\n", VersionToString(&id->version));
	}

	if (packageName)
		printf("Full package name: %ws\n", packageName);

	{
		auto buffer = make_unique<BYTE[]>(256);
		if (::GetTokenInformation(hToken, TokenAppContainerSid, buffer.get(), 256, &size)) {
			auto appContainerSid = reinterpret_cast<SID_AND_ATTRIBUTES*>(buffer.get());
			PWSTR sid;
			if (appContainerSid->Sid) {
				::ConvertSidToStringSid(appContainerSid->Sid, &sid);
				printf("AppContainer SID: %ws\n", sid);
				::LocalFree(sid);
			}
		}
	}

	DWORD number;
	if (::GetTokenInformation(hToken, TokenAppContainerNumber, &number, sizeof(DWORD), &size)) {
		printf("Number: %u\n", number);
	}
	{
		buffer = make_unique<BYTE[]>(1 << 16);
		if (::GetTokenInformation(hToken, TokenCapabilities, buffer.get(), 1 << 16, &size)) {
			auto capabilities = reinterpret_cast<TOKEN_GROUPS*>(buffer.get());
			printf("Capabilities: %d\n", capabilities->GroupCount);
			for (DWORD i = 0; i < capabilities->GroupCount; i++) {
				DisplayCapability(capabilities->Groups[i]);
			}
		}
	}

	::CloseHandle(hToken);


	printf("\n");
}

typedef NTSTATUS(NTAPI *PRtlDeriveCapabilitySidsFromName)(
	PCUNICODE_STRING CapName, PSID CapabilityGroupSid, PSID CapabilitySid);

#pragma comment(lib, "ntdll")

void BuildCapabilityMap() {
	auto RtlDeriveCapabilitySidsFromName = (PRtlDeriveCapabilitySidsFromName)::GetProcAddress(
		::GetModuleHandle(L"ntdll"), "RtlDeriveCapabilitySidsFromName");

	UNICODE_STRING capname;
	auto hModule = ::GetModuleHandle(nullptr);
	auto hResource = ::FindResource(hModule, MAKEINTRESOURCE(IDR_CAPS), L"TXT");

	auto hGlobal = ::LoadResource(hModule, hResource);
	PCSTR caps = (PCSTR)::LockResource(hGlobal);

	auto sid1buffer = make_unique<BYTE[]>(SECURITY_MAX_SID_SIZE);
	auto sid2buffer = make_unique<BYTE[]>(SECURITY_MAX_SID_SIZE);

	auto sid1 = reinterpret_cast<PSID>(sid1buffer.get());
	auto sid2 = reinterpret_cast<PSID>(sid2buffer.get());

	do {
		auto cr = ::strstr(caps, "\n");
		if (!cr) break;

		string name(caps, cr - 1);
		if (name.size() == 0)
			break;

		wstring wname(name.begin(), name.end());
		RtlInitUnicodeString(&capname, wname.c_str());

		auto status = RtlDeriveCapabilitySidsFromName(&capname, sid1, sid2);
		if (status == 0) {
			PWSTR ssid;
			if (::ConvertSidToStringSid(sid2, &ssid)) {
				capabilitiesMap.insert(make_pair(wstring(ssid), wname));
				::LocalFree(ssid);
			}
		}
		if (*cr == '\r')
			cr++;
		caps = cr + 1;
	} while (true);

}

int main(int argc, char* argv[]) {
	printf("List UWP Processes - version 1.1 (C)2016 by Pavel Yosifovich\n\n");
	printf("Building capablities map... ");
	BuildCapabilityMap();
	printf("done.\n\n");

	if (argc == 1) {
		DWORD count = 1000;
		DWORD size = 0;
		unique_ptr<DWORD[]> pid;

		while (!pid || count < size / sizeof(DWORD)) {
			pid = make_unique<DWORD[]>(count);
			if (::EnumProcesses(pid.get(), count * sizeof(DWORD), &size))
				break;
			count *= 2;
		}

		count = size / sizeof(DWORD);

		for (DWORD i = 0; i < count; i++) {
			if (pid[i] < 8)
				continue;

			auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid[i]);
			if (!hProcess)
				continue;

			DisplayProcessInfo(hProcess, pid[i]);

			::CloseHandle(hProcess);
		}
		return 0;
	}

	if (::_stricmp(argv[1], "/?") == 0 || ::_stricmp(argv[1], "-?") == 0 || ::_stricmp(argv[1], "/h") == 0 || ::_stricmp(argv[1], "-h") == 0) {
		printf("Usage: uwpinfo [pid]\n");
		return 0;
	}

	int pid = ::atoi(argv[1]);
	auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProcess) {
		printf("Failed to open process %d (error=%d)\n", pid, ::GetLastError());
		return 1;
	}

	DisplayProcessInfo(hProcess, pid);
	::CloseHandle(hProcess);

	return 0;
}

