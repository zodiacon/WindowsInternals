/*++ NDK Version: 0098

Copyright (c) Alex Ionescu.  All rights reserved.

Header Name:

    ntndk.h

Abstract:

    Master include file for the Native Development Kit.

Author:

    Alex Ionescu (alexi@tinykrnl.org) - Updated - 27-Feb-2006

--*/

#ifndef _NTNDK_
#define _NTNDK_

//
// Disable some warnings that we'd get on /W4.
// Only active for compilers which support this feature.
//
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4001)
#pragma warning(disable:4201)
#pragma warning(disable:4115)
#pragma warning(disable:4214)
#endif

//
// Headers needed for NDK
//
#include <stdio.h>          // C Standard Header
#include <excpt.h>          // C Standard Header
#include <stdarg.h>         // C Standard Header
#include "ndk/umtypes.h"        // General Definitions

//
// Type Headers
//
#include "ndk/cctypes.h"        // Cache Manager Types
#include "ndk/cmtypes.h"        // Configuration Manager Types
#include "ndk/dbgktypes.h"      // User-Mode Kernel Debugging Types
#include "ndk/extypes.h"        // Executive Types
#include "ndk/kdtypes.h"        // Kernel Debugger Types
#include "ndk/ketypes.h"        // Kernel Types
#include "ndk/haltypes.h"       // Hardware Abstraction Layer Types
#include "ndk/ifssupp.h"        // IFS Support Header
#include "ndk/iotypes.h"        // Input/Output Manager Types
#include "ndk/ldrtypes.h"       // Loader Types
#include "ndk/lpctypes.h"       // Local Procedure Call Types
#include "ndk/mmtypes.h"        // Memory Manager Types
#include "ndk/obtypes.h"        // Object Manager Types
#include "ndk/potypes.h"        // Power Manager Types
#include "ndk/pstypes.h"        // Process Manager Types
#include "ndk/rtltypes.h"       // Runtime Library Types
#include "ndk/setypes.h"        // Security Subsystem Types

//
// Function Headers
//
#include "ndk/cmfuncs.h"        // Configuration Manager Functions
#include "ndk/dbgkfuncs.h"      // User-Mode Kernel Debugging Functions
#include "ndk/kdfuncs.h"        // Kernel Debugger Functions
#include "ndk/kefuncs.h"        // Kernel Functions
#include "ndk/exfuncs.h"        // Executive Functions
#include "ndk/halfuncs.h"       // Hardware Abstraction Layer Functions
//#include "ndk/iofuncs.h"        // Input/Output Manager Functions
//#include "ndk/inbvfuncs.h"      // Initialization Boot Video Functions
//#include "ndk/ldrfuncs.h"       // Loader Functions
//#include "ndk/lpcfuncs.h"       // Local Procedure Call Functions
//#include "ndk/mmfuncs.h"        // Memory Manager Functions
//#include "ndk/obfuncs.h"        // Object Manager Functions
//#include "ndk/pofuncs.h"        // Power Manager Functions
//#include "ndk/psfuncs.h"        // Process Manager Functions
#include "ndk/rtlfuncs.h"       // Runtime Library Functions
//#include "ndk/sefuncs.h"        // Security Subsystem Functions
//#include "ndk/umfuncs.h"        // User-Mode NT Library Functions

//
// Assembly Support
//
#include "ndk/asm.h"            // Assembly Offsets

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // _NTNDK_
