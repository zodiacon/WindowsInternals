// MemInfo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MemInfo.h"

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
PPF_PRIVSOURCE_QUERY_REQUEST MmPrivateSources;
LIST_ENTRY MmProcessListHead;
ULONG MmProcessCount;
LIST_ENTRY MmFileListHead;
ULONG MmFileCount;
ULONG MmPfnDatabaseSize;
HANDLE PfiFileInfoHandle;
PPF_MEMORY_RANGE_INFO MemoryRanges;

void PrintHeader() {
	printf("MemInfo v3.10 - Show PFN database information\n");
	printf("Copyright (C) 2007-2017 Alex Ionescu and Pavel Yosifovich\n");
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
	printf("%6s    Display information on the given virtual address (must use -o)\n", "-v");
}

void PfiBuildSuperfetchInfo(IN PSUPERFETCH_INFORMATION SuperfetchInfo, IN PVOID Buffer, IN ULONG Length, IN SUPERFETCH_INFORMATION_CLASS InfoClass) {
	SuperfetchInfo->Version = SUPERFETCH_VERSION;
	SuperfetchInfo->Magic = SUPERFETCH_MAGIC;
	SuperfetchInfo->Data = Buffer;
	SuperfetchInfo->Length = Length;
	SuperfetchInfo->InfoClass = InfoClass;
}

NTSTATUS PfiQueryMemoryRanges() {
	NTSTATUS Status;
	SUPERFETCH_INFORMATION SuperfetchInfo;
	PF_MEMORY_RANGE_INFO MemoryRangeInfo;
	ULONG ResultLength = 0;

	//
	// Memory Ranges API was added in RTM, this is Version 1
	//
	MemoryRangeInfo.Version = 1;

	//
	// Build the Superfetch Information Buffer
	//
	PfiBuildSuperfetchInfo(&SuperfetchInfo,
		&MemoryRangeInfo,
		sizeof(MemoryRangeInfo),
		SuperfetchMemoryRangesQuery);

	//
	// Query the Memory Ranges
	//
	Status = NtQuerySystemInformation(SystemSuperfetchInformation,
		&SuperfetchInfo,
		sizeof(SuperfetchInfo),
		&ResultLength);
	if (Status == STATUS_BUFFER_TOO_SMALL) {
		//
		// Reallocate memory
		//
		MemoryRanges = static_cast<PPF_MEMORY_RANGE_INFO>(::HeapAlloc(GetProcessHeap(), 0, ResultLength));
		MemoryRanges->Version = 1;

		//
		// Rebuild the buffer
		//
		PfiBuildSuperfetchInfo(&SuperfetchInfo,
			MemoryRanges,
			ResultLength,
			SuperfetchMemoryRangesQuery);

		//
		// Query memory information
		//
		Status = NtQuerySystemInformation(SystemSuperfetchInformation,
			&SuperfetchInfo,
			sizeof(SuperfetchInfo),
			&ResultLength);
		if (!NT_SUCCESS(Status)) {
			printf("Failure querying memory ranges!\n");
			return Status;
		}
	}
	else {
		//
		// Use local buffer
		//
		MemoryRanges = &MemoryRangeInfo;
	}

	return STATUS_SUCCESS;
}

void PfiDumpPfnRanges(VOID) {
	PPHYSICAL_MEMORY_RUN Node;

	for (ULONG i = 0; i < MemoryRanges->RangeCount; i++) {
		//
		// Print information on the range
		//
		Node = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&MemoryRanges->Ranges[i]);
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
	MmPfnDatabaseSize = FIELD_OFFSET(PF_PFN_PRIO_REQUEST, PageData) +
		PfnCount * sizeof(MMPFN_IDENTITY);

	//
	// Build the PFN List Information Request
	//
	PfnDbStart = MmPfnDatabase = static_cast<PPF_PFN_PRIO_REQUEST>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MmPfnDatabaseSize));
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
	BitMapBuffer = ::HeapAlloc(::GetProcessHeap(), 0, PfnCount / 8);
	RtlInitializeBitMap(&MmPfnBitMap, static_cast<PULONG>(BitMapBuffer), PfnCount);
	RtlSetAllBits(&MmPfnBitMap);
	MmVaBitmap = MmPfnBitMap;
#endif


	//
	// Loop all the ranges
	//
	for (k = 0, i = 0; i < MemoryRanges->RangeCount; i++) {
		//
		// Print information on the range
		//
		Node = reinterpret_cast<PPHYSICAL_MEMORY_RUN>(&MemoryRanges->Ranges[i]);
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
	// Sign-extend the key
	//
#ifdef _WIN64
	UniqueProcessKey |= 0xFFFF000000000000;
#else
	UniqueProcessKey |= 0xFFFFFFFF00000000;
#endif

	NextEntry = MmProcessListHead.Flink;
	while (NextEntry != &MmProcessListHead) {
		FoundProcess = CONTAINING_RECORD(NextEntry, PF_PROCESS, ProcessLinks);
		if (FoundProcess->ProcessKey == UniqueProcessKey)
			return FoundProcess;

		NextEntry = NextEntry->Flink;
	}

	return nullptr;
}

NTSTATUS PfiQueryPrivateSources() {
	NTSTATUS Status;
	SUPERFETCH_INFORMATION SuperfetchInfo;
	PF_PRIVSOURCE_QUERY_REQUEST PrivateSourcesQuery = { 0 };
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
		}
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
				NtOpenProcess(&Process->ProcessHandle, PROCESS_ALL_ACCESS, &ObjectAttributes, &ClientId);
			}
			else {
				//
				// Process exists -- clear stats
				//
				Process->ProcessPfnCount = 0;
			}
		}
	}

	::HeapFree(::GetProcessHeap(), 0, MmPrivateSources);
	return Status;
}

VOID PfiBuildFileInfoQuery(IN PPFFI_ENUMERATE_INFO Request) {
	RtlZeroMemory(Request, sizeof(PFFI_ENUMERATE_INFO));
	Request->ETWLoggerId = 1;
	Request->LogForETW = TRUE;
	Request->LogForSuperfetch = TRUE;
	Request->Version = 12;
}

NTSTATUS
PfSvFICommand(HANDLE DeviceHandle, ULONG IoCtl, PVOID a3, int a4, PVOID a5, PULONG ResultLength) {
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatusBlock;

	Status = NtDeviceIoControlFile(DeviceHandle, 0, 0, 0, &IoStatusBlock, IoCtl,
		a3, a4, a5, *ResultLength);
	if (Status < 0) {
		*ResultLength = 0;
	}
	else {
		if (IoCtl == 2228239 && IoStatusBlock.Status < 0) {
			*ResultLength = static_cast<ULONG>(IoStatusBlock.Information);
		}
		else {
			*ResultLength = static_cast<ULONG>(IoStatusBlock.Information);
		}
	}

	return Status;
}

PPF_FILE PfiFindFile(IN ULONGLONG UniqueFileKey) {
	PLIST_ENTRY NextEntry;
	PPF_FILE FoundFile;

	NextEntry = MmFileListHead.Flink;
	while (NextEntry != &MmFileListHead) {
		FoundFile = CONTAINING_RECORD(NextEntry, PF_FILE, FileLinks);
		if (FoundFile->FileKey == UniqueFileKey)
			return FoundFile;

		NextEntry = NextEntry->Flink;
	}

	return nullptr;
}

NTSTATUS PfiQueryFileInfo() {
	PFFI_ENUMERATE_INFO Request;
	PPFNL_LOG_ENTRY LogEntry;
	PPFFI_UNKNOWN LogHeader;
	PVOID OutputBuffer;
	ULONG OutputLength;
	NTSTATUS Status;

	OutputLength = 16 * 1024 * 1024;
	OutputBuffer = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, OutputLength);

	//
	// Build the request
	//
	PfiBuildFileInfoQuery(&Request);

	//
	// Send it
	//
	Status = PfSvFICommand(PfiFileInfoHandle,
		0x22000F,
		&Request,
		sizeof(Request),
		OutputBuffer,
		&OutputLength);
	if (!NT_SUCCESS(Status)) return Status;

	//
	// Parse the log
	//
	LogHeader = static_cast<PPFFI_UNKNOWN>(OutputBuffer);
	LogEntry = reinterpret_cast<PPFNL_LOG_ENTRY>(LogHeader + 1);
	while (1) {
		//
		// Do we already know about this file?
		//
		PPF_FILE File;
		File = PfiFindFile(LogEntry->FileInfo.Key);
		if (!File) {
			//
			// We don't, allocate it
			//
			File = static_cast<PPF_FILE>(::HeapAlloc(::GetProcessHeap(),
				0,
				sizeof(PF_FILE) +
				LogEntry->Header.Size -
				FIELD_OFFSET(PFNL_LOG_ENTRY, FileInfo)));
			InsertTailList(&MmFileListHead, &File->FileLinks);
			MmFileCount++;

			//
			// What kind of entry is it?
			//
			if (LogEntry->Header.Type == PfNLInfoTypeFile) {
				//
				// Copy the file name and key
				//
				wcscpy_s(File->FileName, MAX_PATH, LogEntry->FileInfo.Filename);
				File->FileKey = LogEntry->FileInfo.Key;
			}
			else if (LogEntry->Header.Type == PfNLInfoTypeVolume) {
				//
				// Copy the volume path
				//
				wcscpy_s(File->FileName, MAX_PATH, LogEntry->VolumeInfo.VolumePath);
			}
			else {
				//
				// Copy the section name
				//
				wcscpy_s(File->FileName, MAX_PATH, LogEntry->PfBackedInfo.SectionName);
				File->FileKey = LogEntry->PfBackedInfo.Key;
			}
		}

		//
		// Go to the next one
		//
		LogEntry = reinterpret_cast<PPFNL_LOG_ENTRY>(((ULONG_PTR)LogEntry + LogEntry->Header.Size));
		if (LogEntry >= (PVOID)((ULONG_PTR)OutputBuffer + LogHeader->BufferSize))
			break;
	}

	//
	// Free the output buffer
	//
	::HeapFree(::GetProcessHeap(), 0, OutputBuffer);
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
	assert(Status == STATUS_SUCCESS);

	//
	// Initialize page counts
	//
	RtlZeroMemory(MmPageCounts, sizeof(MmPageCounts));
	RtlZeroMemory(MmUseCounts, sizeof(MmUseCounts));
	RtlZeroMemory(MmPageUseCounts, sizeof(MmPageUseCounts));

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
			PPF_PROCESS Process;
		TryAgain:
			Process = PfiFindProcess(Pfn1->u1.e4.UniqueProcessKey);
			if (Process) {
				//
				// Add this to the process' PFN array
				//
				Process->ProcessPfns[Process->ProcessPfnCount] = i;
				if (Process->ProcessPfnCount == Process->PrivatePages) {
					//
					// Our original estimate might be off, let's allocate some more PFNs
					//
					PLIST_ENTRY PreviousEntry, NextEntry;
					PreviousEntry = Process->ProcessLinks.Blink;
					NextEntry = Process->ProcessLinks.Flink;
					Process = static_cast<PPF_PROCESS>(::HeapReAlloc(::GetProcessHeap(), 0, Process,
						sizeof(PF_PROCESS) +
						Process->PrivatePages * 2 * sizeof(ULONG)));
					Process->PrivatePages *= 2;
					PreviousEntry->Flink = NextEntry->Blink = &Process->ProcessLinks;
				}

				//
				// One more PFN
				//
				Process->ProcessPfnCount++;
			}
			else {
				//
				// The private sources changed during a query -- reload private sources
				//
				PfiQueryPrivateSources();
				goto TryAgain;
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
	CHAR Extra[11];

	Pfn1 = MI_GET_PFN(i);

	if ((Pfn1->u1.e1.UseDescription == MMPFNUSE_PROCESSPRIVATE) && (Pfn1->u1.e4.UniqueProcessKey != 0)) {
		ProcessName = PfiFindProcess(Pfn1->u1.e4.UniqueProcessKey)->ProcessName;
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
		FileName = PfiFindFile(((ULONG_PTR)Pfn1->u2.FileObject & ~0x1))->FileName;
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

VOID
PfiDumpProcessPfnEntry(ULONG i) {
	PMMPFN_IDENTITY Pfn1;
	PCHAR Type = "UNKNOWN";
	PCHAR Protect = "PAGE_NOACCESS";
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
	auto size = ::VirtualQueryEx(Process->ProcessHandle,
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

		switch (MemoryBasicInfo.Protect) {
		case PAGE_EXECUTE:
			Protect = "PAGE_EXECUTE";
			break;

		case PAGE_READWRITE:
			Protect = "PAGE_READWRITE";
			break;

		case PAGE_READONLY:
			Protect = "PAGE_READONLY";
			break;

		case PAGE_EXECUTE_READWRITE:
			Protect = "PAGE_EXECUTE_READWRITE";
			break;
		}
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

ULONG
PfiConvertVaToPa(IN ULONG_PTR Va) {
	ULONG i;
	PMMPFN_IDENTITY Pfn1 = NULL;
	ULONG_PTR AlignedVa = PAGE_ROUND_DOWN(Va);

	//
	// Is the address in the VA bitmap?
	//
	if (RtlTestBit(&MmVaBitmap, static_cast<ULONG>(AlignedVa >> PAGE_SHIFT))) {
		for (i = 0; i < MmPfnDatabase->PfnCount; i++) {
			Pfn1 = MI_GET_PFN(i);
			if (Pfn1->u2.VirtualAddress == (PVOID)AlignedVa) return i;
		}
	}

	return 0;
}

ULONG PfiGetIndexForPfn(IN ULONG Pfn) {
	ULONG i;
	PMMPFN_IDENTITY Pfn1 = NULL;

	for (i = 0; i < MmPfnDatabase->PfnCount; i++) {
		Pfn1 = MI_GET_PFN(i);
		if (Pfn1->PageFrameIndex == Pfn)
			return i;
	}

	return 0;
}

int main(int argc, const char* argv[]) {
	PrintHeader();

	NTSTATUS status;
	ULONG ShowSummary = 0, ShowPage = 0, ShowProcess = 0, ShowFiles = 0, ShowVirtual = 0, ShowAll = 0, ShowRanges = 0, ShowUsage = 0, ShowCache = 0, ShowWs = 0;

	//
	// Check what we should do
	//
	for (int i = 1; i < argc; i++) {
		//
		// Simply case, user just wants memory ranges
		//
		//if (strstr(argv[i], "-i")) UseInteractive = i;
		if (strstr(argv[i], "-r")) ShowRanges = i;
		if (strstr(argv[i], "-a")) ShowAll = i;
		if (strstr(argv[i], "-s")) ShowSummary = i;
		if (strstr(argv[i], "-v")) ShowVirtual = i;
		if (strstr(argv[i], "-p")) ShowPage = i;
		if (strstr(argv[i], "-c")) ShowCache = i;
		if (strstr(argv[i], "-u")) ShowUsage = i;
		if (strstr(argv[i], "-w")) ShowWs = i;
		if (strstr(argv[i], "-o")) ShowProcess = i;
		if (strstr(argv[i], "-f")) ShowFiles = i;
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
				if (ShowProcess) {
					//
					// Make sure a PID was entered
					//
					if (argc < 3) {
						printf("You must enter a process ID for this option to work!\n");
						return 1;
					}

					//
					// First, get the process structure
					//
					auto Process = PfiLookupProcessByPid(reinterpret_cast<HANDLE>(static_cast<long long>(strtoul(argv[ShowProcess + 1], NULL, 0))));
					if (!Process) {
						printf("Process ID %d could not be found\n", strtoul(argv[ShowProcess + 1], NULL, 0));
						return 1;
					}

					//
					// Now dump pages belonging to it
					//
					PfiDumpProcess(Process);
				}
				//
				// Handle a slighly more complex request
				//
				if (ShowVirtual) {
					//
					// Make sure a VA was entered
					//
					if (argc < 3) {
						printf("You must enter a virtual address for this option to work!\n");
						return 1;
					}

					//
					// First get the page index
					//
					auto Page = PfiConvertVaToPa(strtoul(argv[ShowVirtual + 1], NULL, 16));
					if (Page) {
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
					// Make sure a VA was entered
					//
					if (argc < 3) {
						printf("You must enter a page frame number for this option to work!\n");
						return 1;
					}

					//
					// First, get the page index
					//
					auto Page = PfiGetIndexForPfn(strtoul(argv[ShowPage + 1], NULL, 16));
					if (Page) {
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
		else if (!ShowRanges) {
			//
			// No valid parameters
			//
			PrintUsage();
		}

		return 0;
	}

}