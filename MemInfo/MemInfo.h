#pragma once

#define NT_SUCCESS(Status)              (((NTSTATUS)(Status)) >= 0)

#define SE_DEBUG_PRIVILEGE                (20L)
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE  (13L)

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << 12)

#define MI_GET_PFN(x)                   (PMMPFN_IDENTITY)(&MmPfnDatabase->PageData[(x)])
#define MI_PFN_IS_MISMATCHED(Pfn)       (Pfn->u2.e1.Mismatch)

//
// Page-Rounding Macros
//
#define PAGE_ROUND_DOWN(x)                                  \
    (((ULONG_PTR)(x))&(~(PAGE_SIZE-1)))

//
// Definitions for Object Creation
//
#define OBJ_INHERIT                             0x00000002L
#define OBJ_PERMANENT                           0x00000010L
#define OBJ_EXCLUSIVE                           0x00000020L
#define OBJ_CASE_INSENSITIVE                    0x00000040L
#define OBJ_OPENIF                              0x00000080L
#define OBJ_OPENLINK                            0x00000100L
#define OBJ_KERNEL_HANDLE                       0x00000200L
#define OBJ_FORCE_ACCESS_CHECK                  0x00000400L
#define OBJ_VALID_ATTRIBUTES                    0x000007F2L

#define InitializeObjectAttributes(p,n,a,r,s) { \
    (p)->Length = sizeof(OBJECT_ATTRIBUTES);    \
    (p)->RootDirectory = (r);                   \
    (p)->Attributes = (a);                      \
    (p)->ObjectName = (n);                      \
    (p)->SecurityDescriptor = (s);              \
    (p)->SecurityQualityOfService = NULL;       \
}

FORCEINLINE VOID InitializeListHead(IN PLIST_ENTRY ListHead) {
	ListHead->Flink = ListHead->Blink = ListHead;
}

FORCEINLINE VOID InsertTailList(
	IN PLIST_ENTRY ListHead,
	IN PLIST_ENTRY Entry) {
	PLIST_ENTRY OldBlink;
	OldBlink = ListHead->Blink;
	Entry->Flink = ListHead;
	Entry->Blink = OldBlink;
	OldBlink->Flink = Entry;
	ListHead->Blink = Entry;
}


//
// Memory Manager Page Lists
//
typedef enum _MMLISTS {
	ZeroedPageList = 0,
	FreePageList = 1,
	StandbyPageList = 2,
	ModifiedPageList = 3,
	ModifiedNoWritePageList = 4,
	BadPageList = 5,
	ActiveAndValid = 6,
	TransitionPage = 7
} MMLISTS;

//
// PFN Identity Uses
//
#define MMPFNUSE_PROCESSPRIVATE                             0
#define MMPFNUSE_FILE                                       1
#define MMPFNUSE_PAGEFILEMAPPED                             2
#define MMPFNUSE_PAGETABLE                                  3
#define MMPFNUSE_PAGEDPOOL                                  4
#define MMPFNUSE_NONPAGEDPOOL                               5
#define MMPFNUSE_SYSTEMPTE                                  6
#define MMPFNUSE_SESSIONPRIVATE                             7
#define MMPFNUSE_METAFILE                                   8
#define MMPFNUSE_AWEPAGE                                    9
#define MMPFNUSE_DRIVERLOCKPAGE                             10
#define MMPFNUSE_KERNELSTACK                                11

//
//  System Information Classes for NtQuerySystemInformation
//
typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation,
	SystemSuperfetchInformation = 79,
} SYSTEM_INFORMATION_CLASS;

//
// Superfetch Information Class
//
typedef enum _SUPERFETCH_INFORMATION_CLASS {
	SuperfetchRetrieveTrace = 1,        // Query
	SuperfetchSystemParameters = 2,     // Query
	SuperfetchLogEvent = 3,             // Set
	SuperfetchGenerateTrace = 4,        // Set
	SuperfetchPrefetch = 5,             // Set
	SuperfetchPfnQuery = 6,             // Query
	SuperfetchPfnSetPriority = 7,       // Set
	SuperfetchPrivSourceQuery = 8,      // Query
	SuperfetchSequenceNumberQuery = 9,  // Query
	SuperfetchScenarioPhase = 10,       // Set
	SuperfetchWorkerPriority = 11,      // Set
	SuperfetchScenarioQuery = 12,       // Query
	SuperfetchScenarioPrefetch = 13,    // Set
	SuperfetchRobustnessControl = 14,   // Set
	SuperfetchTimeControl = 15,         // Set
	SuperfetchMemoryListQuery = 16,     // Query
	SuperfetchMemoryRangesQuery = 17,   // Query
	SuperfetchTracingControl = 18,       // Set
	SuperfetchTrimWhileAgingControl = 19,
	SuperfetchInformationMax = 20
} SUPERFETCH_INFORMATION_CLASS;

//
// Superfetch Private Sources
//
typedef enum _PFS_PRIVATE_PAGE_SOURCE_TYPE {
	PfsPrivateSourceKernel = 0,
	PfsPrivateSourceSession = 1,
	PfsPrivateSourceProcess = 2,
	PfsPrivateSourceMax = 3
} PFS_PRIVATE_PAGE_SOURCE_TYPE;

typedef struct _PHYSICAL_MEMORY_RUN {
	SIZE_T BasePage;
	SIZE_T PageCount;
} PHYSICAL_MEMORY_RUN, *PPHYSICAL_MEMORY_RUN;

typedef struct _PF_PHYSICAL_MEMORY_RANGE {
	ULONG_PTR BasePfn;
	ULONG_PTR PageCount;
} PF_PHYSICAL_MEMORY_RANGE, *PPF_PHYSICAL_MEMORY_RANGE;

typedef struct _PF_MEMORY_RANGE_INFO {
	ULONG Version;
	ULONG RangeCount;
	PF_PHYSICAL_MEMORY_RANGE Ranges[ANYSIZE_ARRAY];
} PF_MEMORY_RANGE_INFO, *PPF_MEMORY_RANGE_INFO;

//
// Sub-Information Types for PFN Identity
//
typedef struct _MEMORY_FRAME_INFORMATION {
	ULONGLONG UseDescription : 4;
	ULONGLONG ListDescription : 3;
	ULONGLONG Reserved0 : 1;
	ULONGLONG Pinned : 1;
	ULONGLONG DontUse : 48;
	ULONGLONG Priority : 3;
	ULONGLONG Reserved : 4;
} MEMORY_FRAME_INFORMATION, *PMEMORY_FRAME_INFORMATION;

typedef struct _FILEOFFSET_INFORMATION {
	ULONGLONG DontUse : 9;
	ULONGLONG Offset : 48;
	ULONGLONG Reserved : 7;
} FILEOFFSET_INFORMATION, *PFILEOFFSET_INFORMATION;

typedef struct _PAGEDIR_INFORMATION {
	ULONGLONG DontUse : 9;
	ULONGLONG PageDirectoryBase : 48;
	ULONGLONG Reserved : 7;
} PAGEDIR_INFORMATION, *PPAGEDIR_INFORMATION;

typedef struct _UNIQUE_PROCESS_INFORMATION {
	ULONGLONG DontUse : 9;
	ULONGLONG UniqueProcessKey : 48;
	ULONGLONG Reserved : 7;
} UNIQUE_PROCESS_INFORMATION, *PUNIQUE_PROCESS_INFORMATION;

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

//
// PFN Identity Data Structure
//
typedef struct _MMPFN_IDENTITY {
	union {
		MEMORY_FRAME_INFORMATION e1;
		FILEOFFSET_INFORMATION e2;
		PAGEDIR_INFORMATION e3;
		UNIQUE_PROCESS_INFORMATION e4;
	} u1;
	SIZE_T PageFrameIndex;
	union {
		struct {
			ULONG Image : 1;
			ULONG Mismatch : 1;
		} e1;
		PVOID FileObject;
		PVOID UniqueFileObjectKey;
		PVOID ProtoPteAddress;
		PVOID VirtualAddress;
	} u2;
} MMPFN_IDENTITY, *PMMPFN_IDENTITY;

typedef struct _SYSTEM_MEMORY_LIST_INFORMATION {
	SIZE_T ZeroPageCount;
	SIZE_T FreePageCount;
	SIZE_T ModifiedPageCount;
	SIZE_T ModifiedNoWritePageCount;
	SIZE_T BadPageCount;
	SIZE_T PageCountByPriority[8];
	SIZE_T RepurposedPagesByPriority[8];
	ULONG_PTR ModifiedPageCountPageFile;
} SYSTEM_MEMORY_LIST_INFORMATION, *PSYSTEM_MEMORY_LIST_INFORMATION;

//
// Data Structure for SuperfetchPfnQuery
//
typedef struct _PF_PFN_PRIO_REQUEST {
	ULONG Version;
	ULONG RequestFlags;
	SIZE_T PfnCount;
	SYSTEM_MEMORY_LIST_INFORMATION MemInfo;
	MMPFN_IDENTITY PageData[256];
} PF_PFN_PRIO_REQUEST, *PPF_PFN_PRIO_REQUEST;

typedef struct _PF_PROCESS {
	LIST_ENTRY ProcessLinks;
	ULONGLONG ProcessKey;
	CHAR ProcessName[16];
	ULONG ProcessPfnCount;
	ULONG PrivatePages;
	HANDLE ProcessId;
	ULONG SessionId;
	HANDLE ProcessHandle;
	ULONG ProcessPfns[ANYSIZE_ARRAY];
} PF_PROCESS, *PPF_PROCESS;

//
// ClientID Structure
//
typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

//
// Buffer for NtQuery/SetInformationSystem for the Superfetch Class
//
typedef struct _SUPERFETCH_INFORMATION {
	ULONG Version;
	ULONG Magic;
	SUPERFETCH_INFORMATION_CLASS InfoClass;
	PVOID Data;
	ULONG Length;
} SUPERFETCH_INFORMATION, *PSUPERFETCH_INFORMATION;

typedef struct _RTL_BITMAP {
	ULONG SizeOfBitMap;
	PULONG Buffer;
} RTL_BITMAP, *PRTL_BITMAP;

//
// Information Structures for NtQuerySystemInformation
//
typedef struct _SYSTEM_BASIC_INFORMATION {
	ULONG Reserved;
	ULONG TimerResolution;
	ULONG PageSize;
	ULONG NumberOfPhysicalPages;
	ULONG LowestPhysicalPageNumber;
	ULONG HighestPhysicalPageNumber;
	ULONG AllocationGranularity;
	ULONG_PTR MinimumUserModeAddress;
	ULONG_PTR MaximumUserModeAddress;
	ULONG_PTR ActiveProcessorsAffinityMask;
	CCHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

//
// Private Source Database Information
//
typedef struct _PFS_PRIVATE_PAGE_SOURCE {
	PFS_PRIVATE_PAGE_SOURCE_TYPE Type;
	ULONG ProcessId;
	ULONG ImagePathHash;
	ULONG_PTR UniqueProcessHash;
} PFS_PRIVATE_PAGE_SOURCE, *PPFS_PRIVATE_PAGE_SOURCE;

//
// NL Log Entry Types
//
typedef enum _PFNL_ENTRY_TYPE {
	PfNLInfoTypeFile,
	PfNLInfoTypePfBacked,
	PfNLInfoTypeVolume,
	PfNLInfoTypeDelete,
	PfNLInfoTypeMax
} PFNL_ENTRY_TYPE;

//
// Header for NL Log Entry
//
typedef struct _PFNL_ENTRY_HEADER {
	PFNL_ENTRY_TYPE Type : 3;
	ULONG Size : 28;
	ULONG Timestamp;
	ULONG SequenceNumber;
} PFNL_ENTRY_HEADER, *PPFNL_ENTRY_HEADER;

//
// File Information NL Log Entry
//
typedef struct _PFNL_FILE_INFO {
	SIZE_T Key;
	SIZE_T VolumeKey;
	ULONG VolumeSequenceNumber;
	ULONG Metafile : 1;
	ULONG FileRenamed : 1;
	ULONG PagingFile : 1;
#ifdef _WIN64
	ULONG Spare : 29;
	USHORT Something;
#else
	ULONG Spare : 13;
#endif
	USHORT NameLength;
	WCHAR Filename[ANYSIZE_ARRAY];
} PFNL_FILE_INFO, *PPFNL_FILE_INFO;

//
// Pagefile Backed Information NL Log Entry
//
typedef struct _PFNL_PFBACKED_INFO {
	ULONG Key;
	PVOID ProtoPteStart;
	PVOID ProtoPteEnd;
	USHORT NameLength;
	WCHAR SectionName[ANYSIZE_ARRAY];
} PFNL_PFBACKED_INFO, *PPFNL_PFBACKED_INFO;

typedef struct _OBJECT_NAME_INFORMATION {
	UNICODE_STRING Name;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

//
// Volume Information NL Log Entry
//
typedef struct _PFNL_VOLUME_INFO {
	LARGE_INTEGER CreationTime;
	ULONG Key;
	ULONG SerialNumber;
	ULONG DeviceType : 4;
	ULONG DeviceFlags : 4;
	ULONG Reserved : 24;
	OBJECT_NAME_INFORMATION Path;
	WCHAR VolumePath[ANYSIZE_ARRAY];
} PFNL_VOLUME_INFO, *PPFNL_VOLUME_INFO;

//
// Delete Information NL Log Entry
//
typedef struct _PFNL_DELETE_ENTRY_INFO {
	union {
		struct {
			ULONG Type : 2;
			ULONG FileDeleted : 1;
			ULONG Spare : 28;
		};
		struct {
			ULONG Reserved : 2;
			ULONG DeleteFlags : 29;
		};
	};
	ULONG Key;
} PFNL_DELETE_ENTRY_INFO, *PPFNL_DELETE_ENTRY_INFO;

//
// NL Log Entry
//
typedef struct _PFNL_LOG_ENTRY {
	PFNL_ENTRY_HEADER Header;
	union {
		PFNL_FILE_INFO FileInfo;
		PFNL_PFBACKED_INFO PfBackedInfo;
		PFNL_VOLUME_INFO VolumeInfo;
		PFNL_DELETE_ENTRY_INFO DeleteEntryInfo;
	};
} PFNL_LOG_ENTRY, *PPFNL_LOG_ENTRY;

//
// Input Structure for IOCTL_PFFI_ENUMERATE
//
typedef struct _PFFI_ENUMERATE_INFO {
	ULONG Version;
	ULONG LogForETW : 1;
	ULONG LogForSuperfetch : 1;
	ULONG Reserved : 30;
	ULONG ETWLoggerId;
} PFFI_ENUMERATE_INFO, *PPFFI_ENUMERATE_INFO;

//
// Private Source Entry
//
typedef struct _PF_PRIVSOURCE_INFO {
	PFS_PRIVATE_PAGE_SOURCE DbInfo;
	PVOID EProcess;
	SIZE_T WorkingSetPrivateSize;
	SIZE_T NumberOfPrivatePages;
	ULONG SessionID;
	CHAR ImageName[16];

	union {
		ULONG_PTR WsSwapPages;                 // process only PF_PRIVSOURCE_QUERY_WS_SWAP_PAGES.
		ULONG_PTR SessionPagedPoolPages;       // session only.
		ULONG_PTR StoreSizePages;              // process only PF_PRIVSOURCE_QUERY_STORE_INFO.
	};
	ULONG_PTR WsTotalPages;         // process/session only.
	ULONG DeepFreezeTimeMs;         // process only.
	ULONG ModernApp : 1;            // process only.
	ULONG DeepFrozen : 1;           // process only. If set, DeepFreezeTimeMs contains the time at which the freeze occurred
	ULONG Foreground : 1;           // process only.
	ULONG PerProcessStore : 1;      // process only.
	ULONG Spare : 28;

} PF_PRIVSOURCE_INFO, *PPF_PRIVSOURCE_INFO;

//
// Query Data Structure for SuperfetchPrivSourceQuery
//
typedef struct _PF_PRIVSOURCE_QUERY_REQUEST {
	ULONG Version;
	ULONG Flags;
	ULONG InfoCount;
	PF_PRIVSOURCE_INFO InfoArray[ANYSIZE_ARRAY];
} PF_PRIVSOURCE_QUERY_REQUEST, *PPF_PRIVSOURCE_QUERY_REQUEST;

//
// Output Structure for IOCTL_PFFI_ENUMERATE
//
typedef struct _PFFI_UNKNOWN {
	USHORT SuperfetchVersion;
	USHORT FileinfoVersion;
	ULONG Tag;
	ULONG BufferSize;
	ULONG Always1;
	ULONG Always3;
	ULONG Reserved;
	ULONG Reserved2;
	ULONG AlignedSize;
	ULONG EntryCount;
	ULONG Reserved3;
} PFFI_UNKNOWN, *PPFFI_UNKNOWN;

typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	} DUMMYUNIONNAME;

	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _OBJECT_ATTRIBUTES {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

typedef struct _PF_FILE {
	LIST_ENTRY FileLinks;
	ULONGLONG FileKey;
	WCHAR FileName[ANYSIZE_ARRAY];
} PF_FILE, *PPF_FILE;

//
// Mapping of page priority strings
//
PCHAR Priorities[8] =
{
	"Idle",
	"Very Low",
	"Low",
	"Background",
	"Background",
	"Normal",
	"SuperFetch",
	"SuperFetch"
};

//
// Mapping of page list short-strings
//
PCHAR ShortPfnList[TransitionPage + 1] =
{
	"Zero",
	"Free",
	"Standby",
	"Modified",
	"Mod No-Write",
	"Bad",
	"Active",
	"Transition"
};

//
// Mapping of page usage strings
//
PCHAR UseList[MMPFNUSE_KERNELSTACK + 1] =
{
	"Process Private",
	"Memory Mapped File",
	"Page File Mapped",
	"Page Table",
	"Paged Pool",
	"Non Paged Pool",
	"System PTE",
	"Session Private",
	"Metafile",
	"AWE Pages",
	"Driver Lock Pages",
	"Kernel Stack"
};

//
// Mapping of page usage short-strings
//
PCHAR ShortUseList[MMPFNUSE_KERNELSTACK + 1] =
{
	"Process Private",
	"Mem Mapped File",
	"PF Mapped File",
	"Page Table",
	"Paged Pool",
	"Non Paged Pool",
	"System PTE",
	"Session Private",
	"Metafile",
	"AWE ",
	"Driver Locked",
	"Kernel Stack"
};

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(
	IN ULONG Privilege,
	IN BOOLEAN NewValue,
	IN BOOLEAN ForThread,
	OUT PBOOLEAN OldValue
);

extern "C" NTSTATUS NTAPI NtQuerySystemInformation(
	IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
	OUT PVOID SystemInformation,
	IN ULONG Length,
	OUT PULONG ResultLength
);

extern "C" VOID NTAPI RtlInitializeBitMap(
	IN PRTL_BITMAP BitMapHeader,
	IN PULONG BitMapBuffer,
	IN ULONG SizeOfBitMap
);

extern "C" VOID NTAPI RtlSetAllBits(
	PRTL_BITMAP BitMapHeader
);

extern "C" VOID NTAPI
RtlInitUnicodeString(
	IN OUT PUNICODE_STRING DestinationString,
	IN PCWSTR SourceString);

extern "C" NTSTATUS NTAPI NtOpenFile(
	OUT PHANDLE FileHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG ShareAccess,
	IN ULONG OpenOptions
);

extern "C" NTSTATUS NTAPI NtOpenProcess(
	OUT PHANDLE ProcessHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes,
	IN PCLIENT_ID ClientId
);

//
// APC Callback for NtCreateFile
//
typedef VOID
(NTAPI *PIO_APC_ROUTINE)(
	IN PVOID ApcContext,
	IN PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG Reserved);

extern "C" NTSTATUS NTAPI NtDeviceIoControlFile(
	IN HANDLE DeviceHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
	IN PVOID UserApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	OUT PVOID OutputBuffer,
	IN ULONG OutputBufferSize
);

extern "C" BOOLEAN NTAPI
RtlTestBit(
	PRTL_BITMAP BitMapHeader,
	ULONG BitNumber
);
