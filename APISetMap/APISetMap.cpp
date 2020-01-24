// apiSetMap.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

void PrintHeader() {
	printf("ApiSetMap - list API Set mappings - version 1.0\n");
	printf("(c) Alex Ionescu, Pavel Yosifovich, and Contributors\n");
	printf("http://www.alex-ionescu.com\n\n");
}


int main() {
	PrintHeader();

	auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
	auto apiSetMap = static_cast<PAPI_SET_NAMESPACE>(peb->Reserved9[0]);
	auto apiSetMapAsNumber = reinterpret_cast<ULONG_PTR>(apiSetMap);

	auto nsEntry = reinterpret_cast<PAPI_SET_NAMESPACE_ENTRY>((apiSetMap->EntryOffset + apiSetMapAsNumber));

	UNICODE_STRING nameString, valueString;

	for (ULONG i = 0; i < apiSetMap->Count; i++) {
		auto isSealed = (nsEntry->Flags & API_SET_SCHEMA_ENTRY_FLAGS_SEALED) != 0;

		//
		// Build a UNICODE_STRING for this contract
		//
		nameString.MaximumLength = static_cast<USHORT>(nsEntry->NameLength);
		nameString.Length = static_cast<USHORT>(nsEntry->NameLength);
		nameString.Buffer = reinterpret_cast<PWCHAR>(apiSetMapAsNumber + nsEntry->NameOffset);
		printf("%56wZ.dll -> %s{", &nameString, (isSealed ? "s" : "" ));

		//
		// Iterate the values (i.e.: the hosts for this set)
		//
		auto valueEntry = reinterpret_cast<PAPI_SET_VALUE_ENTRY>(apiSetMapAsNumber + nsEntry->ValueOffset);
		for (ULONG j = 0; j < nsEntry->ValueCount; j++) {
			//
			// Build a UNICODE_STRING for this host
			//
			valueString.Buffer = reinterpret_cast<PWCHAR>(apiSetMapAsNumber + valueEntry->ValueOffset);
			valueString.MaximumLength = static_cast<USHORT>(valueEntry->ValueLength);
			valueString.Length = static_cast<USHORT>(valueEntry->ValueLength);
			printf("%wZ", &valueString);

			//
			// If there's more than one, add a comma
			//
			if ((j + 1) != nsEntry->ValueCount) {
				printf(", ");
			}

			//
			// If there's an alias...
			//
			if (valueEntry->NameLength != 0) {
				//
				// Build a UNICODE_STRING for it
				//
				nameString.MaximumLength = static_cast<USHORT>(valueEntry->NameLength);
				nameString.Length = static_cast<USHORT>(valueEntry->NameLength);
				nameString.Buffer = reinterpret_cast<PWCHAR>(apiSetMapAsNumber + valueEntry->NameOffset);
				printf(" [%wZ]", &nameString);
			}

			valueEntry++;
		}
		//
		// Next contract
		//
		printf("}\n");
		nsEntry++;
	}

	return 0;
}

