// CPlist.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

vector<CLSID> EnumerateProviders() {
	vector<CLSID> guids;

	HKEY hKey;
	auto error = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Authentication\\Credential Providers",
		0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKey);
	if (error == ERROR_SUCCESS) {
		WCHAR name[128];
		for (int i = 0; ; i++) {
			DWORD len = 128;
			error = ::RegEnumKeyEx(hKey, i, name, &len, nullptr, nullptr, nullptr, nullptr);
			if (ERROR_SUCCESS != error)
				break;
			CLSID clsid;
			if (SUCCEEDED(CLSIDFromString(name, &clsid)))
				guids.push_back(clsid);
		}
		::RegCloseKey(hKey);
	}

	return guids;
}

void ShowProviderInfo(const CLSID& clsid) {
	// read name from registry
	WCHAR sclsid[128], value[128], path[260];
	::StringFromGUID2(clsid, sclsid, 128);
	DWORD size = 128;
	auto error = ::RegGetValue(HKEY_CLASSES_ROOT, CString(L"CLSID\\") + sclsid, L"", RRF_RT_REG_SZ, nullptr, value, &size);
	size = 260;
	error |= ::RegGetValue(HKEY_CLASSES_ROOT, CString(L"CLSID\\") + sclsid + "\\InProcServer32", L"",
		RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, path, &size);
	if (error == ERROR_SUCCESS) {
		printf("%ws %-50ws %ws\n", sclsid, value, path);
	}
}

int main() {
	printf("Credential Provider List version 1.0 (C)2016 by Pavel Yosifovich\n\n");

	auto clsids = EnumerateProviders();
	for (const auto& clsid : clsids)
		ShowProviderInfo(clsid);

	return 0;
}

