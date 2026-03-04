#include <stdio.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

int main() {
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // Get the size of the table
    GetIfTable(NULL, &dwSize, FALSE);
    MIB_IFTABLE *pIfTable = (MIB_IFTABLE *) malloc(dwSize);
    if (pIfTable == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }

    // Retrieve the actual data
    dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    if (dwRetVal != NO_ERROR) {
        printf("GetIfTable failed with error: %lu\n", dwRetVal);
        free(pIfTable);
        return 1;
    }

    printf("Number of interfaces: %lu\n", pIfTable->dwNumEntries);

    ULONG totalIn = 0;
    ULONG totalOut = 0;

    for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
         MIB_IFROW row = pIfTable->table[i];
    wprintf(L"Interface %lu: %ls\n", i, row.wszName);
    printf("  Description: %.*s\n", row.dwDescrLen, row.bDescr);
    printf("  InOctets: %lu\n", row.dwInOctets);
    printf("  OutOctets: %lu\n", row.dwOutOctets);

        totalIn += row.dwInOctets;
        totalOut += row.dwOutOctets;
    }

    printf("\nTotal Bytes Received (In): %lu\n", totalIn);
    printf("Total Bytes Sent (Out): %lu\n", totalOut);

    free(pIfTable);
    return 0;
}
