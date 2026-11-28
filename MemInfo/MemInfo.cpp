// MemInfo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MemInfo.h"
#include <unordered_set>
#include <unordered_map>
#include <cctype>

#define SUPERFETCH_VERSION      45
#define SUPERFETCH_MAGIC        'kuhC'

#pragma comment(lib, "ntdll")

SIZE_T MmPageCounts[TransitionPage + 1];
SIZE_T MmUseCounts[MMPFNUSE_KERNELSTACK + 1];
SIZE_T MmPageUseCounts[MMPFNUSE_KERNELSTACK + 1][TransitionPage + 1];
SIZE_T MmHighestPhysicalPage;
ULONG MmHighestPhysicalPageNumber;
PPF_PFN_PRIO_REQUEST MmPfnDatabase;
RTL_BITMAP MmVaBitmap, MmPfnBitMap;
LIST_ENTRY MmProcessListHead;
ULONG MmProcessCount;
LIST_ENTRY MmFileListHead;
ULONG MmFileCount;
// Key -> file. The log can hold over half a million files, and every file-backed page
// is looked up, so a list walk per lookup is far too slow.
std::unordered_map<ULONGLONG, PPF_FILE> MmFileMap;
ULONG MmPfnDatabaseSize;
HANDLE PfiFileInfoHandle;
PPF_PHYSICAL_MEMORY_RANGE MemoryRanges;	// points into MemoryRangesBuffer
ULONG MemoryRangeCount;
PVOID MemoryRangesBuffer;

void PrintHeader() {
	printf("MemInfo v4.00 - Show PFN database information\n");
	printf("Copyright (C) 2007-2026 Alex Ionescu and Pavel Yosifovich\n");
	printf("http://www.windows-internals.com\n\n");
}

void PrintUsage() {
	printf("usage: meminfo [-a][-u][-c][-r][-s][-w][-f][-o PID][-p PFN][-v VA]\n");
	printf("%6s    Dump full information about each page in the PFN database\n", "-a");
	printf("%6s    Show summary page usage information for the system\n", "-u");
	printf("%6s    Display detailed information about the prioritized page lists\n", "-c");
	printf("%6s    Show valid physical memory ranges detected\n", "-r");
	printf("%6s    Display summary information about the pages on the system\n", "-s");
	printf("%6s    Show detailed page usage information for private working sets\n", "-w");
	printf("%6s    Display file names associated to memory mapped pages\n", "-f");
	printf("%6s    Display information about each page in the process' working set\n", "-o");
	printf("%6s    Display information on the given page frame index (PFN)\n", "-p");
	printf("%6s    Display information on the given virtual address (use -o PID to pick the process)\n", "-v");
}

void PfiBuildSuperfetchInfo(IN PSUPERFETCH_INFORMATION SuperfetchInfo, IN PVOID Buffer, IN ULONG Length, IN SUPERFETCH_INFORMATION_CLASS InfoClass) {
	SuperfetchInfo->Version = SUPERFETCH_VERSION;
	SuperfetchInfo->Magic = SUPERFETCH_MAGIC;
	SuperfetchInfo->Data = Buffer;
	SuperfetchInfo->Length = Length;
	SuperfetchInfo->InfoClass = InfoClass;
}

NTSTATUS PfiQueryMemoryRangesVersion(ULONG Version) {
	NTSTATUS Status;
	SUPERFETCH_INFORMATION SuperfetchInfo;
	ULONG Length = 4096;
	ULONG ResultLength = 0;
	PVOID Buffer;

	for (;;) {
		Buffer = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, Length);
		if (!Buffer)
			return STATUS_NO_MEMORY;

		//
		// The version is the first field in both layouts
		//
		*static_cast<PULONG>(Buffer) = Version;

		PfiBuildSuperfetchInfo(&SuperfetchInfo,
			Buffer,
			Length,
			SuperfetchMemoryRangesQuery);

		Status = NtQuerySystemInformation(SystemSuperfetchInformation,
			&SuperfetchInfo,
			sizeof(SuperfetchInfo),
			&ResultLength);
		if (Status == STATUS_BUFFER_TOO_SMALL && ResultLength > Length) {
			::HeapFree(::GetProcessHeap(), 0, Buffer);
			Length = ResultLength;
			continue;
		}
		break;
	}

	if (!NT_SUCCESS(Status)) {
		::HeapFree(::GetProcessHeap(), 0, Buffer);
		return Status;
	}

	MemoryRangesBuffer = Buffer;
	if (Version == 1) {
		auto Info = static_cast<PPF_MEMORY_RANGE_INFO>(Buffer);
		MemoryRangeCount = Info->RangeCount;
		MemoryRanges = Info->Ranges;
	}
	else {
		auto Info = static_cast<PPF_MEMORY_RANGE_INFO_V2>(Buffer);
		MemoryRangeCount = Info->RangeCount;
		MemoryRanges = Info->Ranges;
	}

	return STATUS_SUCCESS;
}

NTSTATUS PfiQueryMemoryRanges() {
	//
	// Current builds only accept version 2; older ones only know version 1
	//
	NTSTATUS Status = PfiQueryMemoryRangesVersion(2);
	if (!NT_SUCCESS(Status))
		Status = PfiQueryMemoryRangesVersion(1);
	if (!NT_SUCCESS(Status))
		printf("Failure querying memory ranges (0x%X)\n", Status);
	return Status;
}

void PfiDumpPfnRanges(VOID) {
	PPHYSICAL_MEMORY_RUN Node;

	for (ULONG i = 0; i < MemoryRangeCount; i++) {
		//
		// Print information on the range
		//
		Node = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&MemoryRanges[i]);
#ifdef _WIN64
		printf("Physical Memory Range: %p to %p (%lld pages, %lld KB)\n",
#else
		printf("Physical Memory Range: %p to %p (%d pages, %d KB)\n",
#endif
			reinterpret_cast<void*>(Node->BasePage << PAGE_SHIFT),
			reinterpret_cast<void*>((Node->BasePage + Node->PageCount) << PAGE_SHIFT),
			Node->PageCount,
			(Node->PageCount << PAGE_SHIFT) >> 10);

		//
		// Get the highest page
		//
		MmHighestPhysicalPage = max(MmHighestPhysicalPage, Node->BasePage + Node->PageCount);
	}

	//
	// Print highest page found
	//
#ifdef _WIN64
	printf("MmHighestPhysicalPage: %lld\n", MmHighestPhysicalPage);
#else
	printf("MmHighestPhysicalPage: %d\n", MmHighestPhysicalPage);
#endif
}

NTSTATUS PfiInitializePfnDatabase() {
	NTSTATUS Status;
	SUPERFETCH_INFORMATION SuperfetchInfo;
	ULONG ResultLength = 0;
	PMMPFN_IDENTITY Pfn1;
	ULONG PfnCount, i, k;
	ULONG PfnOffset = 0;
	ULONG BadPfn = 0;
	PVOID BitMapBuffer;
	PPF_PFN_PRIO_REQUEST PfnDbStart;
	PPHYSICAL_MEMORY_RUN Node;


	//
	// Calculate maximum amount of memory required
	//
	PfnCount = MmHighestPhysicalPageNumber + 1;
	//
	// The Superfetch request carries the buffer length as a ULONG, which limits it to 4 GB
	// (roughly 680 GB of physical address space). Fail rather than truncate the size.
	//
	SIZE_T DatabaseSize = FIELD_OFFSET(PF_PFN_PRIO_REQUEST, PageData) +
		static_cast<SIZE_T>(PfnCount) * sizeof(MMPFN_IDENTITY);
	if (DatabaseSize > ~0UL) {
		printf("Physical address space is too large to query the PFN database in one request.\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	MmPfnDatabaseSize = static_cast<ULONG>(DatabaseSize);

	//
	// Build the PFN List Information Request
	//
	PfnDbStart = MmPfnDatabase = static_cast<PPF_PFN_PRIO_REQUEST>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MmPfnDatabaseSize));
	if (!MmPfnDatabase)
		return STATUS_NO_MEMORY;
	MmPfnDatabase->Version = 1;
	MmPfnDatabase->RequestFlags = 1;

	//
	// Build the Superfetch Query
	//
	PfiBuildSuperfetchInfo(&SuperfetchInfo,
		MmPfnDatabase,
		MmPfnDatabaseSize,
		SuperfetchPfnQuery);

#if 1
	//
	// Initial request, assume all bits valid
	//
	for (ULONG i = 0; i < PfnCount; i++) {
		//
		// Get the PFN and write the physical page number
		//
		Pfn1 = MI_GET_PFN(i);
		Pfn1->PageFrameIndex = i;
	}

	//
	// Build a bitmap of pages
	//
	// RTL bitmaps operate on whole ULONGs, so round up
	BitMapBuffer = ::HeapAlloc(::GetProcessHeap(), 0, ((PfnCount + 31) / 32) * sizeof(ULONG));
	if (!BitMapBuffer)
		return STATUS_NO_MEMORY;
	RtlInitializeBitMap(&MmPfnBitMap, static_cast<PULONG>(BitMapBuffer), PfnCount);
	RtlSetAllBits(&MmPfnBitMap);
	MmVaBitmap = MmPfnBitMap;
#endif


	//
	// Loop all the ranges
	//
	for (k = 0, i = 0; i < MemoryRangeCount; i++) {
		//
		// Print information on the range
		//
		Node = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&MemoryRanges[i]);
		for (SIZE_T j = Node->BasePage; j < (Node->BasePage + Node->PageCount); j++) {
			//
			// Get the PFN and write the physical page number
			//
			Pfn1 = MI_GET_PFN(k++);
			Pfn1->PageFrameIndex = j;
		}
	}

	//
	// Query all valid PFNs
	//
	MmPfnDatabase->PfnCount = k;

	//
	// Query the PFN Database
	//
	Status = NtQuerySystemInformation(SystemSuperfetchInformation,
		&SuperfetchInfo,
		sizeof(SuperfetchInfo),
		&ResultLength);

	return Status;
}

NTSTATUS PfSvFIGetHandle(OUT PHANDLE DeviceHandle) {
	OBJECT_ATTRIBUTES ObjectAttributes;
	UNICODE_STRING DeviceString;
	IO_STATUS_BLOCK IoStatusBlock;

	RtlInitUnicodeString(&DeviceString, L"\\Device\\FileInfo");
	InitializeObjectAttributes(&ObjectAttributes, &DeviceString, OBJ_CASE_INSENSITIVE, NULL, NULL);

	return NtOpenFile(DeviceHandle, 1179785, &ObjectAttributes, &IoStatusBlock, 7, 32);
}

PPF_PROCESS PfiFindProcess(IN ULONGLONG UniqueProcessKey) {
	PLIST_ENTRY NextEntry;
	PPF_PROCESS FoundProcess;

	//
	// Older builds use the (truncated) EPROCESS address as the key, which must be
	// sign-extended. Current builds hide kernel addresses and use the process ID,
	// which must be compared as-is.
	//
#ifdef _WIN64
	ULONGLONG ExtendedKey = UniqueProcessKey | 0xFFFF000000000000;
#else
	ULONGLONG ExtendedKey = UniqueProcessKey | 0xFFFFFFFF00000000;
#endif

	NextEntry = MmProcessListHead.Flink;
	while (NextEntry != &MmProcessListHead) {
		FoundProcess = CONTAINING_RECORD(NextEntry, PF_PROCESS, ProcessLinks);
		if (FoundProcess->ProcessKey == UniqueProcessKey || FoundProcess->ProcessKey == ExtendedKey)
			return FoundProcess;

		NextEntry = NextEntry->Flink;
	}

	return nullptr;
}

NTSTATUS PfiQueryPrivateSources() {
	NTSTATUS Status;
	SUPERFETCH_INFORMATION SuperfetchInfo;
	PF_PRIVSOURCE_QUERY_REQUEST PrivateSourcesQuery = { 0 };
	PPF_PRIVSOURCE_QUERY_REQUEST MmPrivateSources = &PrivateSourcesQuery;
	ULONG ResultLength = 0;

	/* Version 2 for Beta 2, Version 3 for RTM */
	PrivateSourcesQuery.Version = 8; //3;

	PfiBuildSuperfetchInfo(&SuperfetchInfo,
		&PrivateSourcesQuery,
		sizeof(PrivateSourcesQuery),
		SuperfetchPrivSourceQuery);

	Status = NtQuerySystemInformation(SystemSuperfetchInformation,
		&SuperfetchInfo,
		sizeof(SuperfetchInfo),
		&ResultLength);
	if (Status == STATUS_BUFFER_TOO_SMALL) {
		MmPrivateSources = static_cast<PPF_PRIVSOURCE_QUERY_REQUEST>(::HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ResultLength));
		if (!MmPrivateSources)
			return STATUS_NO_MEMORY;
		MmPrivateSources->Version = 8;

		PfiBuildSuperfetchInfo(&SuperfetchInfo,
			MmPrivateSources,
			ResultLength,
			SuperfetchPrivSourceQuery);

		Status = NtQuerySystemInformation(SystemSuperfetchInformation,
			&SuperfetchInfo,
			sizeof(SuperfetchInfo),
			&ResultLength);
		if (!NT_SUCCESS(Status)) {
			printf("Superfetch Information Query Failed\n");
			::HeapFree(::GetProcessHeap(), 0, MmPrivateSources);
			return Status;
		}
	}
	else if (!NT_SUCCESS(Status)) {
		return Status;
	}

	//
	// Loop the private sources
	//
	for (ULONG i = 0; i < MmPrivateSources->InfoCount; i++) {
		//
		// Make sure it's a process
		//
		if (MmPrivateSources->InfoArray[i].DbInfo.Type == PfsPrivateSourceProcess) {
			//
			// Do we already know about this process?
			//
			PPF_PROCESS Process;
			CLIENT_ID ClientId;
			OBJECT_ATTRIBUTES ObjectAttributes;
			Process = PfiFindProcess(reinterpret_cast<ULONGLONG>(MmPrivateSources->InfoArray[i].EProcess));
			if (!Process) {
				//
				// We don't, allocate it
				//
				Process = static_cast<PPF_PROCESS>(::HeapAlloc(::GetProcessHeap(), 0, sizeof(PF_PROCESS) +
					MmPrivateSources->InfoArray[i].NumberOfPrivatePages * sizeof(ULONG)));
				InsertTailList(&MmProcessListHead, &Process->ProcessLinks);
				MmProcessCount++;

				//
				// Set it up
				//
				Process->ProcessKey = reinterpret_cast<ULONGLONG>(MmPrivateSources->InfoArray[i].EProcess);
				strncpy_s(Process->ProcessName, MmPrivateSources->InfoArray[i].ImageName, 16);
				Process->ProcessPfnCount = 0;
				Process->PrivatePages = static_cast<ULONG>(MmPrivateSources->InfoArray[i].NumberOfPrivatePages);
				Process->ProcessId = reinterpret_cast<HANDLE>(static_cast<ULONGLONG>(MmPrivateSources->InfoArray[i].DbInfo.ProcessId));
				Process->SessionId = MmPrivateSources->InfoArray[i].SessionID;
				Process->ProcessHandle = NULL;

				//
				// Open a handle to it
				//
				InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);
				ClientId.UniqueProcess = Process->ProcessId;
				ClientId.UniqueThread = 0;
				// VirtualQueryEx needs only PROCESS_QUERY_INFORMATION. Protected processes still
				// refuse it; their pages then show UNKNOWN type and protection.
				NtOpenProcess(&Process->ProcessHandle, PROCESS_QUERY_INFORMATION, &ObjectAttributes, &ClientId);
			}
			// An already-known process keeps its PFNs: this runs again in the middle of
			// PfiQueryPfnDatabase, and clearing the count there would drop pages already counted.
		}
	}

	if (MmPrivateSources != &PrivateSourcesQuery)
		::HeapFree(::GetProcessHeap(), 0, MmPrivateSources);
	return Status;
}

VOID PfiBuildFileInfoQuery(IN PPFFI_ENUMERATE_INFO Request, IN ULONG Version) {
	RtlZeroMemory(Request, sizeof(PFFI_ENUMERATE_INFO));
	Request->ETWLoggerId = 1;
	Request->LogForETW = TRUE;
	Request->LogForSuperfetch = TRUE;
	Request->Version = Version;
}

NTSTATUS
PfSvFICommand(HANDLE DeviceHandle, ULONG IoCtl, PVOID a3, int a4, PVOID a5, PULONG ResultLength) {
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatusBlock;

	IoStatusBlock.Information = 0;
	Status = NtDeviceIoControlFile(DeviceHandle, 0, 0, 0, &IoStatusBlock, IoCtl,
		a3, a4, a5, *ResultLength);

	//
	// On STATUS_BUFFER_OVERFLOW (a warning, but negative as an NTSTATUS) Information holds
	// the size the caller needs, so pass it back rather than zeroing it
	//
	if ((ULONG)Status >> 30 == 3)	// NT_ERROR: severity is error
		*ResultLength = 0;
	else
		*ResultLength = static_cast<ULONG>(IoStatusBlock.Information);

	return Status;
}

PPF_FILE PfiFindFile(IN ULONGLONG UniqueFileKey) {
	auto it = MmFileMap.find(UniqueFileKey);
	return it == MmFileMap.end() ? nullptr : it->second;
}

NTSTATUS PfiQueryFileInfo() {
	PFFI_ENUMERATE_INFO Request;
	PPFNL_LOG_ENTRY LogEntry;
	PPFFI_UNKNOWN LogHeader;
	PVOID OutputBuffer;
	ULONG OutputLength;
	NTSTATUS Status;

	//
	// Request version 16 is what current builds accept (12 now fails with 0xC0000355);
	// keep 12 as a fallback for older builds
	//
	static const ULONG Versions[] = { 16, 12 };
	ULONG BufferSize = 4 * 1024 * 1024;
	OutputBuffer = nullptr;
	Status = STATUS_UNSUCCESSFUL;
	for (ULONG v = 0; v < _countof(Versions); v++) {
		PfiBuildFileInfoQuery(&Request, Versions[v]);

		//
		// The log can be well over 100 MB; grow the buffer to the size the driver asks for
		//
		for (int Attempt = 0; Attempt < 4; Attempt++) {
			if (!OutputBuffer) {
				OutputBuffer = ::VirtualAlloc(nullptr, BufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				if (!OutputBuffer)
					return STATUS_NO_MEMORY;
			}
			OutputLength = BufferSize;
			Status = PfSvFICommand(PfiFileInfoHandle,
				0x22000F,
				&Request,
				sizeof(Request),
				OutputBuffer,
				&OutputLength);
			if (Status != STATUS_BUFFER_OVERFLOW && Status != STATUS_BUFFER_TOO_SMALL)
				break;

			// The log keeps growing, so leave some slack over the size reported
			ULONG Needed = OutputLength > BufferSize ? OutputLength : BufferSize * 2;
			if (Needed > ~0UL - 16 * 1024 * 1024)
				break;
			BufferSize = Needed + 16 * 1024 * 1024;
			::VirtualFree(OutputBuffer, 0, MEM_RELEASE);
			OutputBuffer = nullptr;
		}
		if (NT_SUCCESS(Status) || Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
			break;	// only an unsupported request version moves on to the next one
	}

	if (!NT_SUCCESS(Status) || OutputLength < sizeof(PFFI_UNKNOWN)) {
		if (OutputBuffer)
			::VirtualFree(OutputBuffer, 0, MEM_RELEASE);
		return NT_SUCCESS(Status) ? STATUS_UNSUCCESSFUL : Status;
	}

	//
	// Parse the log, never trusting sizes beyond what the driver actually returned
	//
	LogHeader = static_cast<PPFFI_UNKNOWN>(OutputBuffer);
	bool V13 = LogHeader->FileinfoVersion >= PFFI_FILEINFO_VERSION_V13;
	ULONG_PTR End = reinterpret_cast<ULONG_PTR>(OutputBuffer) + min(LogHeader->BufferSize, OutputLength);
	ULONG_PTR Next = reinterpret_cast<ULONG_PTR>(OutputBuffer) + (V13 ? PFFI_HEADER_SIZE_V13 : sizeof(PFFI_UNKNOWN));
	while (Next + sizeof(PFNL_ENTRY_HEADER) <= End) {
		LogEntry = reinterpret_cast<PPFNL_LOG_ENTRY>(Next);
		ULONG Size = LogEntry->Header.Size;
		if (Size < sizeof(PFNL_ENTRY_HEADER) || Size > End - Next)
			break;	// malformed entry; a zero size would otherwise loop forever
		ULONG_PTR EntryEnd = Next + Size;
		Next = EntryEnd;

		//
		// What kind of entry is it?
		//
		PCWSTR Name;
		ULONGLONG Key;
		switch (LogEntry->Header.Type) {
		case PfNLInfoTypeFile:
			Name = LogEntry->FileInfo.Filename;
			Key = LogEntry->FileInfo.Key;
			break;

		case PfNLInfoTypePfBacked:
			Name = LogEntry->PfBackedInfo.SectionName;
			Key = LogEntry->PfBackedInfo.Key;
			break;

		case PfNLInfoTypeVolume:
			if (V13) {
				Name = LogEntry->VolumeInfoV13.VolumePath;
				Key = LogEntry->VolumeInfoV13.Key;
			}
			else {
				Name = LogEntry->VolumeInfo.VolumePath;
				Key = LogEntry->VolumeInfo.Key;
			}
			break;

		default:
			continue;	// delete notifications etc. carry no name
		}

		if (reinterpret_cast<ULONG_PTR>(Name) >= EntryEnd)
			continue;

		//
		// Do we already know about this file?
		//
		if (PfiFindFile(Key))
			continue;

		//
		// The name may not be NUL-terminated; never read past the entry
		//
		size_t NameLength = wcsnlen(Name, (EntryEnd - reinterpret_cast<ULONG_PTR>(Name)) / sizeof(WCHAR));
		auto File = static_cast<PPF_FILE>(::HeapAlloc(::GetProcessHeap(),
			0,
			FIELD_OFFSET(PF_FILE, FileName) + (NameLength + 1) * sizeof(WCHAR)));
		if (!File)
			break;

		wmemcpy(File->FileName, Name, NameLength);
		File->FileName[NameLength] = L'\0';
		File->FileKey = Key;
		InsertTailList(&MmFileListHead, &File->FileLinks);
		MmFileMap.emplace(Key, File);
		MmFileCount++;
	}

	//
	// Free the output buffer
	//
	::VirtualFree(OutputBuffer, 0, MEM_RELEASE);
	return STATUS_SUCCESS;
}

NTSTATUS PfiQueryPfnDatabase() {
	NTSTATUS Status;
	PMMPFN_IDENTITY Pfn1;
	SUPERFETCH_INFORMATION SuperfetchInfo;
	ULONG ResultLength = 0;

	//
	// Build the Superfetch Query
	//
	PfiBuildSuperfetchInfo(&SuperfetchInfo,
		MmPfnDatabase,
		MmPfnDatabaseSize,
		SuperfetchPfnQuery);

	//
	// Query the PFN Database
	//
	Status = NtQuerySystemInformation(SystemSuperfetchInformation,
		&SuperfetchInfo,
		sizeof(SuperfetchInfo),
		&ResultLength);
	if (!NT_SUCCESS(Status))
		return Status;

	//
	// Initialize page counts
	//
	RtlZeroMemory(MmPageCounts, sizeof(MmPageCounts));
	RtlZeroMemory(MmUseCounts, sizeof(MmUseCounts));
	RtlZeroMemory(MmPageUseCounts, sizeof(MmPageUseCounts));

	//
	// Process keys still unknown after reloading the private sources (e.g. the process exited)
	//
	std::unordered_set<ULONGLONG> UnknownProcessKeys;

	//
	// Loop the database
	//
	for (ULONG i = 0; i < MmPfnDatabase->PfnCount; i++) {
		//
		// Get the PFN
		//
		Pfn1 = MI_GET_PFN(i);

		//
		// Save the count
		//
		MmPageCounts[Pfn1->u1.e1.ListDescription]++;

		//
		// Save the usage
		//
		MmUseCounts[Pfn1->u1.e1.UseDescription]++;

		//
		// Save both
		//
		MmPageUseCounts[Pfn1->u1.e1.UseDescription][Pfn1->u1.e1.ListDescription]++;

		//
		// Is this a process page?
		//
		if ((Pfn1->u1.e1.UseDescription == MMPFNUSE_PROCESSPRIVATE) && (Pfn1->u1.e4.UniqueProcessKey != 0)) {
			//
			// Get the process structure
			//
			ULONGLONG Key = Pfn1->u1.e4.UniqueProcessKey;
			PPF_PROCESS Process = PfiFindProcess(Key);
			if (!Process && UnknownProcessKeys.find(Key) == UnknownProcessKeys.end()) {
				//
				// The private sources changed during a query -- reload private sources once
				//
				PfiQueryPrivateSources();
				Process = PfiFindProcess(Key);
				if (!Process)
					UnknownProcessKeys.insert(Key);
			}

			if (Process) {
				if (Process->ProcessPfnCount >= Process->PrivatePages) {
					//
					// Our original estimate might be off, let's allocate some more PFNs
					//
					ULONG NewCapacity = Process->PrivatePages ? Process->PrivatePages * 2 : 64;
					PLIST_ENTRY PreviousEntry, NextEntry;
					PreviousEntry = Process->ProcessLinks.Blink;
					NextEntry = Process->ProcessLinks.Flink;
					auto NewProcess = static_cast<PPF_PROCESS>(::HeapReAlloc(::GetProcessHeap(), 0, Process,
						sizeof(PF_PROCESS) +
						NewCapacity * sizeof(ULONG)));
					if (!NewProcess)
						continue;
					Process = NewProcess;
					Process->PrivatePages = NewCapacity;
					PreviousEntry->Flink = NextEntry->Blink = &Process->ProcessLinks;
				}

				//
				// Add this to the process' PFN array
				//
				Process->ProcessPfns[Process->ProcessPfnCount++] = i;
			}
		}
	}

	return STATUS_SUCCESS;
}

VOID PfiDumpPfnEntry(ULONG i, IN BOOLEAN ShowFiles) {
	PMMPFN_IDENTITY Pfn1;
	PCHAR ProcessName = "N/A";
	PWCHAR FileName = NULL;
#ifdef _WIN64
	CHAR VirtualAddress[19];
#else
	CHAR VirtualAddress[11];
#endif
	CHAR Extra[sizeof(VirtualAddress)];	// also holds a "0x%p" value

	Pfn1 = MI_GET_PFN(i);

	if ((Pfn1->u1.e1.UseDescription == MMPFNUSE_PROCESSPRIVATE) && (Pfn1->u1.e4.UniqueProcessKey != 0)) {
		auto Process = PfiFindProcess(Pfn1->u1.e4.UniqueProcessKey);
		ProcessName = Process ? Process->ProcessName : "<unknown>";	// e.g. process exited
	}

	if ((Pfn1->u1.e1.UseDescription != MMPFNUSE_FILE) &&
		((Pfn1->u1.e1.UseDescription == MMPFNUSE_PAGETABLE) ||
		(ProcessName) ||
			(Pfn1->u1.e1.ListDescription != TransitionPage))) {
		sprintf_s(VirtualAddress, "0x%08p", Pfn1->u2.VirtualAddress);
	}
	else {
		strcpy_s(VirtualAddress, "N/A");
	}

	if (Pfn1->u1.e1.UseDescription == MMPFNUSE_PAGETABLE) {
		sprintf_s(Extra, "0x%08p", reinterpret_cast<void*>(Pfn1->u1.e3.PageDirectoryBase));
	}
	else if (Pfn1->u1.e1.UseDescription == MMPFNUSE_FILE) {
		auto File = PfiFindFile(((ULONG_PTR)Pfn1->u2.FileObject & ~0x1));
		if (File)
			FileName = File->FileName;
		sprintf_s(Extra, "0x%07X", static_cast<ULONG>(Pfn1->u1.e2.Offset));
	}
	else {
		strcpy_s(Extra, "N/A");
	}

	printf("0x%08p %-11s %-16s %d %c %-14s %-10s %s\n",
		reinterpret_cast<void*>(Pfn1->PageFrameIndex << PAGE_SHIFT),
		ShortPfnList[Pfn1->u1.e1.ListDescription],
		ShortUseList[Pfn1->u1.e1.UseDescription],
		(UCHAR)Pfn1->u1.e1.Priority,
		Pfn1->u2.e1.Image ? '*' : ' ',
		ProcessName,
		VirtualAddress,
		Extra);
	if ((FileName) && (ShowFiles)) {
		printf("%S\n", FileName);
	}
}

void PfiDumpPfnDatabase(bool ShowFiles) {
	ULONG i;

	printf("Address    List        Type             P I Process        VA         Offset\n");
	for (i = 0; i < MmPfnDatabase->PfnCount; i++) {
		PfiDumpPfnEntry(i, ShowFiles);
	}
}

#pragma warning (disable:4477)

void PfiDumpPfnLists() {
	SIZE_T i, Total = 0;

	for (i = 0; i < 8; i++) Total += MmPageCounts[i];
	printf("\nPFN Database List Statistics\n");
	printf("%20s:%8d (%8d kb)\n",
		"Zeroed",
		MmPageCounts[ZeroedPageList],
		(MmPageCounts[ZeroedPageList] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Free",
		MmPageCounts[FreePageList],
		(MmPageCounts[FreePageList] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Standby",
		MmPageCounts[StandbyPageList],
		(MmPageCounts[StandbyPageList] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Modified",
		MmPageCounts[ModifiedPageList],
		(MmPageCounts[ModifiedPageList] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"ModifiedNoWrite",
		MmPageCounts[ModifiedNoWritePageList],
		(MmPageCounts[ModifiedNoWritePageList] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Active/Valid",
		MmPageCounts[ActiveAndValid],
		(MmPageCounts[ActiveAndValid] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Transition",
		MmPageCounts[TransitionPage],
		(MmPageCounts[TransitionPage] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Bad",
		MmPfnDatabase->MemInfo.BadPageCount,
		(MmPfnDatabase->MemInfo.BadPageCount << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"Unknown",
		MmPageCounts[BadPageList],
		(MmPageCounts[BadPageList] << PAGE_SHIFT) / 1024);
	printf("%20s:%8d (%8d kb)\n",
		"TOTAL",
		Total,
		(Total << PAGE_SHIFT) / 1024);

}

void PfiDumpPfnPrioLists() {
	SIZE_T i, Total[2] = { 0 };

	printf("\nPriority                Standby               Repurposed\n");


	for (i = 0; i < 8; i++) {
		printf("%d - %-13s %8d (%7d KB)  %8d (%7d KB)\n",
			i,
			Priorities[i],
			MmPfnDatabase->MemInfo.PageCountByPriority[i],
			(MmPfnDatabase->MemInfo.PageCountByPriority[i] << PAGE_SHIFT) >> 10,
			MmPfnDatabase->MemInfo.RepurposedPagesByPriority[i],
			(MmPfnDatabase->MemInfo.RepurposedPagesByPriority[i] << PAGE_SHIFT) >> 10);
		Total[0] += MmPfnDatabase->MemInfo.PageCountByPriority[i];
		Total[1] += MmPfnDatabase->MemInfo.RepurposedPagesByPriority[i];
	}

	printf("%-16s %9d (%7d KB)  %9d (%7d KB)\n", "TOTAL",
		Total[0], (Total[0] << PAGE_SHIFT) >> 10,
		Total[1], (Total[1] << PAGE_SHIFT) >> 10);
}

void PfiDumpPfnUsages() {
	SIZE_T i, Total[3] = { 0 };

	printf("\nUsage                   Active                Standby               TOTAL\n");

	for (i = 0; i < (MMPFNUSE_KERNELSTACK + 1); i++) {
		printf("%-18s %8d (%8d KB) %8d (%8d KB)  %8d (%8d KB)\n",
			UseList[i],
			MmPageUseCounts[i][ActiveAndValid],
			(MmPageUseCounts[i][ActiveAndValid] << PAGE_SHIFT) >> 10,
			MmPageUseCounts[i][StandbyPageList],
			(MmPageUseCounts[i][StandbyPageList] << PAGE_SHIFT) >> 10,
			MmUseCounts[i],
			(MmUseCounts[i] << PAGE_SHIFT) >> 10);

		Total[0] += MmPageUseCounts[i][ActiveAndValid];
		Total[1] += MmPageUseCounts[i][StandbyPageList];
		Total[2] += MmUseCounts[i];
	}

	printf("%-18s %8d (%8d KB) %8d (%8d KB)  %8d (%8d KB)\n", "TOTAL",
		Total[0], (Total[0] << PAGE_SHIFT) >> 10,
		Total[1], (Total[1] << PAGE_SHIFT) >> 10,
		Total[2], (Total[2] << PAGE_SHIFT) >> 10);
}

void PfiDumpPfnProcUsages(VOID) {
	PLIST_ENTRY NextEntry;
	PPF_PROCESS Process;

	printf("Process         Session   PID       WorkingSet\n");

	NextEntry = MmProcessListHead.Flink;
	while (NextEntry != &MmProcessListHead) {
		Process = CONTAINING_RECORD(NextEntry, PF_PROCESS, ProcessLinks);

		printf("%-15s %7d %5d %6d (%7d KB)\n",
			Process->ProcessName,
			Process->SessionId,
			static_cast<int>(reinterpret_cast<long long>(Process->ProcessId)),
			Process->ProcessPfnCount,
			(Process->ProcessPfnCount << PAGE_SHIFT) >> 10);

		NextEntry = NextEntry->Flink;
	}
}

// Formats a page protection: the base protection, plus any modifier flags
PCSTR PfiProtectionToString(DWORD Protect, PCHAR Buffer, size_t Size) {
	PCSTR Base;
	switch (Protect & 0xFF) {
	case 0:							Base = "NONE"; break;	// reserved, or no access to the region
	case PAGE_NOACCESS:				Base = "PAGE_NOACCESS"; break;
	case PAGE_READONLY:				Base = "PAGE_READONLY"; break;
	case PAGE_READWRITE:			Base = "PAGE_READWRITE"; break;
	case PAGE_WRITECOPY:			Base = "PAGE_WRITECOPY"; break;
	case PAGE_EXECUTE:				Base = "PAGE_EXECUTE"; break;
	case PAGE_EXECUTE_READ:			Base = "PAGE_EXECUTE_READ"; break;
	case PAGE_EXECUTE_READWRITE:	Base = "PAGE_EXECUTE_READWRITE"; break;
	case PAGE_EXECUTE_WRITECOPY:	Base = "PAGE_EXECUTE_WRITECOPY"; break;
	default:
		sprintf_s(Buffer, Size, "0x%X", Protect);
		return Buffer;
	}
	sprintf_s(Buffer, Size, "%s%s%s%s", Base,
		(Protect & PAGE_GUARD) ? "|GUARD" : "",
		(Protect & PAGE_NOCACHE) ? "|NOCACHE" : "",
		(Protect & PAGE_WRITECOMBINE) ? "|WRITECOMBINE" : "");
	return Buffer;
}

VOID
PfiDumpProcessPfnEntry(ULONG i) {
	PMMPFN_IDENTITY Pfn1;
	PCHAR Type = "UNKNOWN";
	PCSTR Protect = "UNKNOWN";
	CHAR ProtectText[64];
	PCHAR Usage = "Private";
#ifdef _WIN64
	CHAR VirtualAddress[19];
#else
	CHAR VirtualAddress[11];
#endif
	SIZE_T ResultLength = 0;
	MEMORY_BASIC_INFORMATION MemoryBasicInfo;
	PPF_PROCESS Process;

	Pfn1 = MI_GET_PFN(i);

	sprintf_s(VirtualAddress, "0x%08p", Pfn1->u2.VirtualAddress);

	Process = PfiFindProcess(Pfn1->u1.e4.UniqueProcessKey);
	auto size = Process == nullptr ? 0 : ::VirtualQueryEx(Process->ProcessHandle,
		Pfn1->u2.VirtualAddress,
		&MemoryBasicInfo,
		sizeof(MemoryBasicInfo));
	if (size) {
		switch (MemoryBasicInfo.Type) {
		case MEM_IMAGE:
			Type = "MEM_IMAGE";
			break;

		case MEM_MAPPED:
			Type = "MEM_MAPPED";
			break;

		case MEM_PRIVATE:
			Type = "MEM_PRIVATE";
			break;
		}

		Protect = PfiProtectionToString(MemoryBasicInfo.Protect, ProtectText, sizeof(ProtectText));
	}

	printf("0x%08p %-11s %d %-10s %-11s %-23s %-7s\n",
		Pfn1->PageFrameIndex << PAGE_SHIFT,
		ShortPfnList[Pfn1->u1.e1.ListDescription],
		(UCHAR)Pfn1->u1.e1.Priority,
		VirtualAddress,
		Type,
		Protect,
		Usage);
}

VOID PfiDumpProcess(IN PPF_PROCESS Process) {
	int pid = static_cast<int>(reinterpret_cast<long long>(Process->ProcessId));
	printf("\nMemory pages for process %d (%X)\n", pid, pid);

#ifdef _WIN64
	printf("Address            List        P VA                 Type        Protection              Usage\n");
#else
	printf("Address    List        P VA         Type        Protection              Usage\n");
#endif

	for (ULONG i = 0; i < Process->ProcessPfnCount; i++) {
		PfiDumpProcessPfnEntry(Process->ProcessPfns[i]);
	}
}

#pragma warning (default:4477)

PPF_PROCESS PfiLookupProcessByPid(IN HANDLE Pid) {
	PLIST_ENTRY NextEntry;
	PPF_PROCESS FoundProcess;

	NextEntry = MmProcessListHead.Flink;
	while (NextEntry != &MmProcessListHead) {
		FoundProcess = CONTAINING_RECORD(NextEntry, PF_PROCESS, ProcessLinks);
		if (FoundProcess->ProcessId == Pid)
			return FoundProcess;

		NextEntry = NextEntry->Flink;
	}

	return nullptr;
}

const ULONG PFN_INDEX_NOT_FOUND = ~0UL;	// index 0 is a valid entry

//
// Finds the PFN database entry mapping Va. User addresses are per-process, so when
// Process is given only that process' private pages are considered.
//
ULONG
PfiConvertVaToPa(IN ULONG_PTR Va, IN PPF_PROCESS Process) {
	PVOID AlignedVa = reinterpret_cast<PVOID>(PAGE_ROUND_DOWN(Va));

	for (ULONG i = 0; i < MmPfnDatabase->PfnCount; i++) {
		PMMPFN_IDENTITY Pfn1 = MI_GET_PFN(i);
		if (Pfn1->u2.VirtualAddress != AlignedVa)
			continue;
		if (Process &&
			(Pfn1->u1.e1.UseDescription != MMPFNUSE_PROCESSPRIVATE ||
			 PfiFindProcess(Pfn1->u1.e4.UniqueProcessKey) != Process))
			continue;
		return i;
	}

	return PFN_INDEX_NOT_FOUND;
}

ULONG PfiGetIndexForPfn(IN ULONG Pfn) {
	ULONG i;
	PMMPFN_IDENTITY Pfn1 = NULL;

	for (i = 0; i < MmPfnDatabase->PfnCount; i++) {
		Pfn1 = MI_GET_PFN(i);
		if (Pfn1->PageFrameIndex == Pfn)
			return i;
	}

	return PFN_INDEX_NOT_FOUND;
}

static bool IsFlag(const char* arg, char flag) {
	return arg[0] == '-' && ::tolower(static_cast<unsigned char>(arg[1])) == flag && arg[2] == '\0';
}

int main(int argc, const char* argv[]) {
	PrintHeader();

	NTSTATUS status;
	ULONG ShowSummary = 0, ShowPage = 0, ShowProcess = 0, ShowFiles = 0, ShowVirtual = 0, ShowAll = 0, ShowRanges = 0, ShowUsage = 0, ShowCache = 0, ShowWs = 0;

	//
	// Check what we should do
	//
	for (int i = 1; i < argc; i++) {
		if (IsFlag(argv[i], 'r')) ShowRanges = i;
		else if (IsFlag(argv[i], 'a')) ShowAll = i;
		else if (IsFlag(argv[i], 's')) ShowSummary = i;
		else if (IsFlag(argv[i], 'c')) ShowCache = i;
		else if (IsFlag(argv[i], 'u')) ShowUsage = i;
		else if (IsFlag(argv[i], 'w')) ShowWs = i;
		else if (IsFlag(argv[i], 'f')) ShowFiles = i;
		else if (IsFlag(argv[i], 'o') || IsFlag(argv[i], 'v') || IsFlag(argv[i], 'p')) {
			//
			// These take a value: argv[i + 1] must exist
			//
			if (i + 1 >= argc) {
				printf("Option %s requires a value.\n\n", argv[i]);
				PrintUsage();
				return 1;
			}
			switch (::tolower(static_cast<unsigned char>(argv[i][1]))) {
			case 'o': ShowProcess = i; break;
			case 'v': ShowVirtual = i; break;
			case 'p': ShowPage = i; break;
			}
			i++;	// skip the value
		}
		else {
			printf("Unknown argument: %s\n\n", argv[i]);
			PrintUsage();
			return 1;
		}
	}

	//
	// You have no arguments if your flags are invalid
	//
	if (!(ShowRanges + ShowAll + ShowSummary + ShowPage + ShowCache + ShowVirtual + ShowUsage + ShowWs + ShowProcess))
		argc = 1;

	if (argc == 1) {
		PrintUsage();
		return 0;
	}

	//
	// First, get required privileges
	//
	BOOLEAN old;
	status = RtlAdjustPrivilege(SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &old);
	status |= RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old);
	if (!NT_SUCCESS(status)) {
		printf("Failed to get required privileges.\n");
		printf("Make sure that you are running with administrative privileges\n"
			"and that your account has the Profile Process and Debug privileges\n");
		return 1;
	}

	//
	// Get the highest physical page on the system, it's a pre-requisite
	//
	SYSTEM_BASIC_INFORMATION basicInfo;

	status = NtQuerySystemInformation(SystemBasicInformation,
		&basicInfo, sizeof(SYSTEM_BASIC_INFORMATION), nullptr);
	if (!NT_SUCCESS(status)) {
		//
		// Shouldn't really happen
		//
		printf("Failed to get maximum physical page\n");
		return 1;
	}

	//
	// Remember the highest page
	//
	MmHighestPhysicalPageNumber = basicInfo.HighestPhysicalPageNumber;

	//
	// Query memory ranges
	//
	status = PfiQueryMemoryRanges();
	if (!NT_SUCCESS(status)) {
		printf("Failure getting memory ranges\n");
		return 1;
	}

	//
	// If the user just wants ranges, do it
	//
	if (ShowRanges) {
		//
		// Dump the ranges
		//
		PfiDumpPfnRanges();

		return 0;
	}

	//
	// Initialize process and file table
	//
	InitializeListHead(&MmProcessListHead);
	InitializeListHead(&MmFileListHead);

	//
	// Initialize the database
	//
	printf("Initializing PFN database... ");

	status = PfiInitializePfnDatabase();
	if (!NT_SUCCESS(status)) {
		printf("Failure initializing PFN database: %X\n", status);
		return 1;
	}

	printf("Done.\n");

	//
	// Now open a handle to File Info
	//
	status = PfSvFIGetHandle(&PfiFileInfoHandle);
	if (!NT_SUCCESS(status)) {
		printf("Failure initializing File Info connection: %lx\n", status);
		return 1;
	}

	if (ShowAll || ShowSummary || ShowVirtual || ShowPage || ShowCache || ShowWs || ShowProcess || ShowUsage) {
		//
		// Query sources and files
		//
		status = PfiQueryPrivateSources();
		if (NT_SUCCESS(status) && ShowFiles)
			status = PfiQueryFileInfo();

		if (NT_SUCCESS(status)) {
			//
			// Do the query
			//
			status = PfiQueryPfnDatabase();
			if (NT_SUCCESS(status)) {
				//
				// Now check the exact request and handle simple ones
				//
				if (ShowAll)
					PfiDumpPfnDatabase(ShowFiles ? true : false);

				if (ShowSummary) {
					//
					// Dump summary information
					//
					PfiDumpPfnLists();
				}
				if (ShowCache) {
					//
					// Dump the PFN priority lists
					//
					PfiDumpPfnPrioLists();
				}
				//
				// Usage requires knowing private sources
				//
				if (ShowUsage) {
					//
					// Now dump the data
					//
					PfiDumpPfnUsages();
				}

				//
				// Usage for each process
				//
				if (ShowWs) {
					//
					// Now dump the data
					//
					PfiDumpPfnProcUsages();
				}

				//
				// Dump all pages for a given PID
				//
				PPF_PROCESS Process = nullptr;
				if (ShowProcess) {
					//
					// First, get the process structure (the parser guarantees a value follows -o)
					//
					auto Pid = strtoul(argv[ShowProcess + 1], NULL, 0);
					Process = PfiLookupProcessByPid(reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(Pid)));
					if (!Process) {
						printf("Process ID %lu could not be found\n", Pid);
						return 1;
					}

					//
					// Now dump pages belonging to it, unless -o only scopes a -v lookup
					//
					if (!ShowVirtual)
						PfiDumpProcess(Process);
				}
				//
				// Handle a slighly more complex request
				//
				if (ShowVirtual) {
					//
					// First get the page index (64-bit parse: strtoul would truncate on x64)
					//
					auto Page = PfiConvertVaToPa(static_cast<ULONG_PTR>(_strtoui64(argv[ShowVirtual + 1], NULL, 16)), Process);
					if (Page != PFN_INDEX_NOT_FOUND) {
						//
						// Dump the single entry
						//
						PfiDumpPfnEntry(Page, ShowFiles ? TRUE : FALSE);
					}
					else {
						printf("Virtual address could not be found in the PFN database!\n");
						return 1;
					}
				}
				//
				// Dump a single page
				//
				if (ShowPage) {
					//
					// First, get the page index
					//
					auto Page = PfiGetIndexForPfn(strtoul(argv[ShowPage + 1], NULL, 16));
					if (Page != PFN_INDEX_NOT_FOUND) {
						//
						// Dump the single entry
						//
						PfiDumpPfnEntry(Page, ShowFiles ? TRUE : FALSE);
					}
					else {
						printf("Page frame number could not be found in the PFN database!\n");
						return 1;
					}
				}
			}
			else {
				printf("Superfetch private sources could not be queried!\n");
				return 1;
			}
		}
		else {
			printf("Failed to query %s (0x%X)\n", ShowFiles ? "private sources or file information" : "private sources", status);
			return 1;
		}

		return 0;
	}

}