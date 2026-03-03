#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

typedef struct {
    char processName[MAX_PATH];
    DWORD pid;
    ULONGLONG bytesSent;   // Currently placeholder (requires advanced tracking)
    ULONGLONG bytesRecv;
} ProcessNetworkInfo;

void PrintNetworkStatus();
int GetProcessesNetworkUsage(ProcessNetworkInfo processes[]);
void PrintAllProcessNetworkUsage();
void SearchProcessByName();

void PrintNetworkStatus() {
    MIB_IFTABLE* pIfTable1 = NULL, * pIfTable2 = NULL;
    DWORD dwSize = 0, dwRetVal;

    // First snapshot
    GetIfTable(NULL, &dwSize, TRUE);
    pIfTable1 = (MIB_IFTABLE*)malloc(dwSize);
    if (!pIfTable1) {
        printf("Memory allocation failed.\n");
        return;
    }

    if ((dwRetVal = GetIfTable(pIfTable1, &dwSize, TRUE)) != NO_ERROR) {
        printf("GetIfTable failed: %lu\n", dwRetVal);
        free(pIfTable1);
        return;
    }

    ULONGLONG sent1 = 0, recv1 = 0;
    for (DWORD i = 0; i < pIfTable1->dwNumEntries; i++) {
        sent1 += pIfTable1->table[i].dwOutOctets;
        recv1 += pIfTable1->table[i].dwInOctets;
    }

    Sleep(1000); // Wait 1 second

    // Second snapshot
    GetIfTable(NULL, &dwSize, TRUE);
    pIfTable2 = (MIB_IFTABLE*)malloc(dwSize);
    if (!pIfTable2) {
        printf("Memory allocation failed (second).\n");
        free(pIfTable1);
        return;
    }

    if ((dwRetVal = GetIfTable(pIfTable2, &dwSize, TRUE)) != NO_ERROR) {
        printf("GetIfTable failed (second): %lu\n", dwRetVal);
        free(pIfTable1);
        free(pIfTable2);
        return;
    }

    ULONGLONG sent2 = 0, recv2 = 0;
    for (DWORD i = 0; i < pIfTable2->dwNumEntries; i++) {
        sent2 += pIfTable2->table[i].dwOutOctets;
        recv2 += pIfTable2->table[i].dwInOctets;
    }

    free(pIfTable1);
    free(pIfTable2);

    double mbps = ((double)((sent2 - sent1) + (recv2 - recv1)) * 8) / 1000000.0;
    printf("Network Utilization: Sent+Received = %.2f Mbps\n", mbps);
}

int GetProcessesNetworkUsage(ProcessNetworkInfo processes[]) {
    DWORD processIDs[1024], cbNeeded;
    unsigned int count = 0;

    if (!EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded)) {
        printf("EnumProcesses failed.\n");
        return 0;
    }

    DWORD numProcs = cbNeeded / sizeof(DWORD);
    for (DWORD i = 0; i < numProcs; i++) {
        DWORD pid = processIDs[i];
        if (pid == 0) continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess) {
            HMODULE hMod;
            DWORD cbMod;
            char name[MAX_PATH] = "<unknown>";
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbMod)) {
                GetModuleBaseNameA(hProcess, hMod, name, sizeof(name));
            }

            strncpy(processes[count].processName, name, MAX_PATH);
            processes[count].pid = pid;
            processes[count].bytesSent = 0; // Needs external driver to track
            processes[count].bytesRecv = 0;

            CloseHandle(hProcess);
            count++;
        }
    }

    return count;
}

void PrintAllProcessNetworkUsage() {
    ProcessNetworkInfo processes[1024];
    int count = GetProcessesNetworkUsage(processes);

    if (count == 0) return;

    printf("\n--- Process Network Usage (Bytes sent and received - demo only) ---\n");
    printf("%-30s  %-6s  %-15s  %-15s\n", "Process Name", "PID", "Bytes Sent", "Bytes Received");
    printf("-------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-30s  %-6u  %-15llu  %-15llu\n",
            processes[i].processName,
            processes[i].pid,
            processes[i].bytesSent,
            processes[i].bytesRecv);
    }
}

void SearchProcessByName() {
    ProcessNetworkInfo processes[1024];
    int count = GetProcessesNetworkUsage(processes);
    if (count == 0) return;

    char search[MAX_PATH];
    printf("Enter process name to search: ");
    scanf("%s", search);

    ULONGLONG totalSent = 0, totalRecv = 0;
    int found = 0;

    for (int i = 0; i < count; i++) {
        if (strstr(processes[i].processName, search)) {
            totalSent += processes[i].bytesSent;
            totalRecv += processes[i].bytesRecv;
            found = 1;
        }
    }

    if (found) {
        printf("Total usage for processes matching '%s':\n", search);
        printf("  Bytes Sent: %llu\n", totalSent);
        printf("  Bytes Received: %llu\n", totalRecv);
    } else {
        printf("No processes found matching '%s'.\n", search);
    }
}

int main() {
    int choice;
    while (1) {
        printf("\nMenu:\n");
        printf("1. View total network utilization\n");
        printf("2. View all processes and their network usage\n");
        printf("3. Search for a process by name\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // clear invalid input
            printf("Invalid input\n");
            continue;
        }

        switch (choice) {
            case 1:
                PrintNetworkStatus();
                break;
            case 2:
                PrintAllProcessNetworkUsage();
                break;
            case 3:
                SearchProcessByName();
                break;
            case 0:
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid option\n");
        }
    }
}
