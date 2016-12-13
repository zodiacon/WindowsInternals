// UwpList.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

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

#ifndef SECURITY_CAPABILITY_APPOINTMENTS
#define SECURITY_CAPABILITY_APPOINTMENTS	11
#endif

#ifndef SECURITY_CAPABILITY_CONTACTS
#define SECURITY_CAPABILITY_CONTACTS		12
#endif

PCSTR BuiltInCapabilityToString(PCWSTR rid) {
	switch (_wtoi(rid)) {
	case SECURITY_CAPABILITY_INTERNET_CLIENT: return "Internet Client";
	case SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER: return "Internet Client & Server";
	case SECURITY_CAPABILITY_PRIVATE_NETWORK_CLIENT_SERVER: return "Private Nwtworks (Client & Server)";
	case SECURITY_CAPABILITY_PICTURES_LIBRARY: return "Pictures Library";
	case SECURITY_CAPABILITY_VIDEOS_LIBRARY: return "Video Library";
	case SECURITY_CAPABILITY_MUSIC_LIBRARY: return "Music Library";
	case SECURITY_CAPABILITY_DOCUMENTS_LIBRARY: return "Documents Library";
	case SECURITY_CAPABILITY_ENTERPRISE_AUTHENTICATION: return "Enterprise Authentication";
	case SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES: return "Shared User Certificates";
	case SECURITY_CAPABILITY_REMOVABLE_STORAGE: return "Removable Storage";
	case SECURITY_CAPABILITY_APPOINTMENTS: return "Appointments";
	case SECURITY_CAPABILITY_CONTACTS: return "Contacts";
	}
	return nullptr;
}

void DisplayCapability(const SID_AND_ATTRIBUTES& saa) {
	PWSTR sid;
	::ConvertSidToStringSid(saa.Sid, &sid);
	printf("\t%ws", sid);
	if (::_wcsnicmp(sid, L"S-1-15-3-", 9) == 0) {
		// if built-in capability, display friendly name
		auto friendlyName = BuiltInCapabilityToString(sid + 9);
		if (friendlyName)
			printf(" %s", friendlyName);
	}
	::LocalFree(sid);

	if (saa.Attributes & SE_GROUP_ENABLED)
		printf(" (ENABLED)");

	printf("\n");
}

void DisplayProcessInfo(HANDLE hProcess, DWORD pid) {
	UINT len = 0;
	auto error = ::GetPackageFullName(hProcess, &len, nullptr);
	if (error == APPMODEL_ERROR_NO_PACKAGE)
		return;

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

	auto name = make_unique<wchar_t[]>(len);
	error = ::GetPackageFullName(hProcess, &len, name.get());
	assert(error == ERROR_SUCCESS);

	printf("Full package name: %ws\n", name.get());

	HANDLE hToken;
	if (::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
		auto buffer = make_unique<BYTE[]>(256);
		DWORD len;
		if (::GetTokenInformation(hToken, TokenAppContainerSid, buffer.get(), 256, &len)) {
			auto appContainerSid = reinterpret_cast<SID_AND_ATTRIBUTES*>(buffer.get());
			PWSTR sid;
			if (appContainerSid->Sid) {
				::ConvertSidToStringSid(appContainerSid->Sid, &sid);
				printf("AppContainer SID: %ws\n", sid);
				::LocalFree(sid);
			}
		}
		DWORD number;
		if (::GetTokenInformation(hToken, TokenAppContainerNumber, &number, sizeof(DWORD), &len)) {
			printf("Number: %u\n", number);
		}
		buffer = make_unique<BYTE[]>(1 << 16);
		if (::GetTokenInformation(hToken, TokenCapabilities, buffer.get(), 1 << 16, &len)) {
			auto capabilities = reinterpret_cast<TOKEN_GROUPS*>(buffer.get());
			printf("Capabilities: %d\n", capabilities->GroupCount);
			for (DWORD i = 0; i < capabilities->GroupCount; i++) {
				DisplayCapability(capabilities->Groups[i]);
			}
		}
		::CloseHandle(hToken);
	}


	printf("\n");
}

int main(int argc, char* argv[]) {
	printf("List UWP Processes - version 1.0 (C)2016 by Pavel Yosifovich\n\n");

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

			if (::IsImmersiveProcess(hProcess))
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

