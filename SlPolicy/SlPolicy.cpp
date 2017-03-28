// SlPolicy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


extern "C" NTSYSCALLAPI
NTSTATUS NTAPI NtQueryLicenseValue(PUNICODE_STRING ValueName, PULONG Type, PVOID Data, ULONG DataSize, PULONG ResultDataSize);

#pragma comment(lib, "ntdll")

enum class ValueType {
	Dword,
	Bool
};

const int MaxValuesInFacility = 24;

struct LicenseFacility {
	PCWSTR FacilityName;
	PCWSTR AlternateName;
	struct LicenseValue {
		PCWSTR Name;
		PCSTR Description;
		ValueType Type;
	} Values[MaxValuesInFacility];
} facilities[] = {
	{ L"Desktop Windows Manager", L"DWM", {
			{ L"Microsoft-Windows-DesktopWindowManager-Core-AnimatedTransitionsAllowed", "Animated Transitions allowed", ValueType::Bool },
			{ L"Microsoft-Windows-DesktopWindowManager-Core-LivePreviewAllowed", "Live preview allowed", ValueType::Bool },
			{ L"Microsoft-Windows-DesktopWindowManager-Core-Flip3dAllowed", "Flip 3D allowed", ValueType::Bool },
			{ L"Microsoft-Windows-DesktopWindowManager-Core-TransparencyAllowed", "Transparency allowed", ValueType::Bool },
			{ L"Microsoft-Windows-DesktopWindowManager-Core-ThumbnailsAllowed", "Thumbnails allowed", ValueType::Bool },
		}
	},

	{ L"Explorer", nullptr, {
			{ L"explorer-AeroAnimationAllowed", "Aero animation allowed", ValueType::Bool },
			{ L"explorer-AeroShakeAllowed", "Aero shake allowed", ValueType::Bool }
		}
	},

	{ L"Fax", nullptr, {
			{ L"Microsoft-Windows-Fax-Common-EnableServerPolicy", "Enable server policy", ValueType::Bool },
			{ L"Microsoft-Windows-Fax-Common-DeviceLimit", "Device limit" }
		}
	},

	{ L"IIS", nullptr, {
			{ L"IIS-W3SVC-MaxConcurrentRequests", "Maximum concurrent requests" }
		}
	},

	{ L"Kernel", nullptr, {
			{ L"Kernel-RegisteredProcessors", "Maximum allowed processor sockets" },
			{ L"Kernel-WindowsMaxMemAllowedx86", "Maximum memory allowed in MB (x86)" },
			{ L"Kernel-WindowsMaxMemAllowedx64", "Maximum memory allowed in MB (x64)" },
			{ L"Kernel-WindowsMaxMemAllowedia64", "Maximum memory allowed in MB (IA64)" },
			{ L"Kernel-WindowsMaxMemAllowedArm", "Maximum memory allowed in MB (ARM)" },
			{ L"Kernel-WindowsMaxMemAllowedArm64", "Maximum memory allowed in MB (ARM64)" },
			{ L"Kernel-PhysicalMemoryAddAllowed", "Add physical memory allowed", ValueType::Bool },
			{ L"Kernel-VmPhysicalMemoryAddAllowed", "Add VM physical memory allowed", ValueType::Bool },
			{ L"Kernel-MaxPhysicalPage", "Maximum physical page in bytes" },
			{ L"Kernel-OneCore-DeviceFamilyID", "Device Family ID" },
			{ L"Kernel-NativeVHDBoot", "Native VHD boot", ValueType::Bool },
			{ L"Kernel-DynamicPartitioningSupported", "Dynamic Partitioning supported", ValueType::Bool },
			{ L"Kernel-VirtualDynamicPartitioningSupported", "Virtual Dynamic Partitioning supported", ValueType::Bool },
			{ L"Kernel-MemoryMirroringSupported", "Memory Mirroring supported", ValueType::Bool },
			{ L"Kernel-PersistDefectiveMemoryList", "Persist defective memory list", ValueType::Bool },
			{ L"Kernel-ProductInfo", "Product info" }
		}
	},

	{ L"LSA Policy", L"LSA", {
			{ L"LSA-Policy-EnableCredentialIsolation", "Enable credential isolation", ValueType::Bool },
			{ L"LSA-Policy-EnableTrustedDomains", "Enable transted domains", ValueType::Bool }
		}
	},

	{ L"Offline Files", nullptr, {
			{ L"Microsoft-Windows-OfflineFiles-Core-FeatureEnabled", "Enabled", ValueType::Bool },
			{ L"Microsoft-Windows-OfflineFiles-Core-BranchCachingEnabled", "Branch caching enabled", ValueType::Bool }
		}
	},

	{ L"OMD", nullptr, {
			{ L"OMD-API-Enabled", "API enabled", ValueType::Bool }
		}
	},

	{ L"Parental Controls", nullptr, {
			{ L"parentalcontrols-EnableFeature", "Enabled", ValueType::Bool }
		}
	},

	{ L"Print Spooler", L"Spooler", {
			{ L"Printing-Spooler-Core-Spoolss-Licensing-Enabled", "Spooler licensing enabled", ValueType::Bool },
			{ L"Printing-Spooler-Pmc-Licensing-Enabled", "Spooler PMC licensing enabled", ValueType::Bool },
			{ L"Printing-Spooler-Core-Localspl-Licensing-Enabled", "Spooler SPL licensing enabled", ValueType::Bool },
			{ L"Printing-Spooler-Core-Spoolss-Licensing-Network-Default-Printer-Enabled", "Spooler licensing network default enabled", ValueType::Bool }
		}
	},

	{ L"Shell Inbox Games", L"Games", {
			{ L"Shell-InBoxGames-FreeCell-EnableGame", "FreeCell enabled", ValueType::Bool },
			{ L"Shell-InBoxGames-Hearts-EnableGame", "Hearts enabled", ValueType::Bool },
			{ L"Shell-InBoxGames-Minesweeper-EnableGame", "Minesweeper enabled", ValueType::Bool },
			{ L"Shell-InBoxGames-PurblePlace-EnableGame", "Purble Place enabled enabled", ValueType::Bool },
			{ L"Shell-InBoxGames-Shanghai-EnableGame", "Shanghai enabled", ValueType::Bool },
			{ L"Shell-InBoxGames-Solitaire-EnableGame", "Solitaire enabled", ValueType::Bool },
			{ L"Shell-InBoxGames-SpiderSolitaire-EnableGame", "Spider Solitaire enabled", ValueType::Bool },
		}
	},

	{ L"Telnet", nullptr, {
			{ L"Telnet-Client-EnableTelnetClient", "Telnet client enabled", ValueType::Bool },
			{ L"Telnet-Server-EnableTelnetServer", "Telnet server enabled", ValueType::Bool },
		}
	},

	{ L"Terminal Services", L"TermSvc", {
			{ L"TerminalServices-RemoteConnectionManager-UiEffects-DWMRemotingAllowed", "DWM remoting allowed", ValueType::Bool },
			{ L"TerminalServices-RemoteConnectionManager-AllowD3DRemoting", "3D remoting allowed", ValueType::Bool },
			{ L"TerminalServices-DeviceRedirection-Licenses-PnpRedirectionAllowed", "PnP redirection allowed", ValueType::Bool },
			{ L"TerminalServices-DeviceRedirection-Licenses-TSMFPluginAllowed", "TSMF plugin allowed", ValueType::Bool },
			{ L"TerminalServices-DeviceRedirection-Licenses-TSEasyPrintAllowed", "Easy print allowed", ValueType::Bool },
			{ L"TerminalServices-DeviceRedirection-Licenses-TSAudioCaptureAllowed", "Audio capture allowed", ValueType::Bool },
			{ L"TerminalServices-RemoteApplications-ClientSku-RAILAllowed", "RAIL allowed", ValueType::Bool },
			{ L"TerminalServices-RDP-7-Advanced-Compression-Allowed", "Advanced compression allowed", ValueType::Bool },
			{ L"TerminalServices-RemoteConnectionManager-AllowRemoteConnections", "Allow remote connections", ValueType::Bool },
			{ L"TerminalServices-RemoteConnectionManager-AllowMultimon", "Allow multi monitors", ValueType::Bool }
		}
	},

	{ L"Windows Media Player", L"WMP", {
			{ L"WMPPlayer-HMEAllowed", "Home Media Ecosystem allowed", ValueType::Bool },
			{ L"WMPPlayer-RMEAllowed", "Remote Media Ecosystem allowed", ValueType::Bool }
		}
	},

	{ L"Workstation Service", L"WksSvc", {
			{ L"WorkstationService-DomainJoinEnabled", "Domain Join enabled", ValueType::Bool }
		}
	}
};

void ListValuesInFacility(int facility) {
	UNICODE_STRING name;
	const auto& facilityName = facilities[facility];
	printf("%ws", facilityName.FacilityName);
	if (facilityName.AlternateName)
		printf(" (%ws)", facilityName.AlternateName);

	printf("\n%s\n", std::string(wcslen(facilityName.FacilityName), '-').c_str());
	for (int i = 0; i < MaxValuesInFacility; i++) {
		const auto& valueName = facilities[facility].Values[i];
		if (valueName.Name == nullptr)
			return;

		RtlInitUnicodeString(&name, valueName.Name);
		ULONG value = 0, size;
		auto status = NtQueryLicenseValue(&name, nullptr, &value, sizeof(value), &size);
		if (NT_SUCCESS(status)) {
			assert(size <= sizeof(value));

			printf("%s: ", valueName.Description);
			if (valueName.Type == ValueType::Bool)
				printf("%s\n", value ? "Yes" : "No");
			else
				printf("%d\n", value);
		}
	}
}

int wmain(int argc, const wchar_t* argv[]) {
	printf("Software License Policy Viewer Version 1.0 (C)2016 by Pavel Yosifovich\n\n");

	if (argc == 1) {
		// list everything
		for (int i = 0; i < _countof(facilities); i++) {
			ListValuesInFacility(i);
			printf("\n");
		}
	}
	else {
		if (_wcsicmp(argv[1], L"/f") == 0 || _wcsicmp(argv[1], L"-f") == 0) {
			if (argc == 2) {
				// list facilities
				for (int i = 0; i < _countof(facilities); i++) {
					printf("%ws", facilities[i].FacilityName);
					if (facilities[i].AlternateName)
						printf(" (%ws)", facilities[i].AlternateName);
					printf("\n");
				}
			}
			else {
				// find facility name
				for (int i = 0; i < _countof(facilities); i++)
					if (_wcsicmp(argv[2], facilities[i].FacilityName) == 0 || 
						(facilities[i].AlternateName && _wcsicmp(argv[2], facilities[i].AlternateName) == 0)) {
						ListValuesInFacility(i);
						return 0;
					}
				printf("Facility not found. Use /f to list facilities.\n");
			}
		}
		else {
			printf("Unknown switch. Usage: Slpolicy [-f] [facilityName]\n");
		}
	}

	return 0;
}

