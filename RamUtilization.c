#include <windows.h>
#include <psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char processName[MAX_PATH];
    DWORD pid;
    SIZE_T ramUsageKB; 
} ProcessInfo;

void PrintMemoryStatus() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = totalPhysMem - memInfo.ullAvailPhys;

    printf("Total RAM: %llu MB\n", totalPhysMem / (1024 * 1024));
    printf("Used RAM : %llu MB\n", physMemUsed / (1024 * 1024));
}

int compareRAMUsage(const void *a, const void *b) {
    ProcessInfo *processA = (ProcessInfo*)a;
    ProcessInfo *processB = (ProcessInfo*)b;

    if (processA->ramUsageKB < processB->ramUsageKB) return 1;
    if (processA->ramUsageKB > processB->ramUsageKB) return -1;
    return 0;
}

int GetProcesses(ProcessInfo processes[]) {
    DWORD processIDs[1024], cbNeeded, numProcesses;
    int processCount = 0;

    if (!EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded)) {
        printf("Failed to enumerate processes.\n");
        return 0;
    }

    numProcesses = cbNeeded / sizeof(DWORD);

    for (DWORD i = 0; i < numProcesses; i++) {
        DWORD pid = processIDs[i];
        if (pid == 0) continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) continue;

        HMODULE hMod;
        DWORD cbNeededMod;
        char processName[MAX_PATH] = "<unknown>";
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeededMod)) {
            GetModuleBaseNameA(hProcess, hMod, processName, sizeof(processName) / sizeof(char));
        }

        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            strncpy(processes[processCount].processName, processName, MAX_PATH);
            processes[processCount].pid = pid;
            processes[processCount].ramUsageKB = pmc.WorkingSetSize / 1024; 
            processCount++;
        }

        CloseHandle(hProcess);
    }

    return processCount;
}

void PrintAllProcessRAMUsage() {
    ProcessInfo processes[1024];
    int processCount = GetProcesses(processes);

    if (processCount == 0) return;

    qsort(processes, processCount, sizeof(ProcessInfo), compareRAMUsage);

    printf("\n--- Sorted Process RAM Usage ---\n");
    printf("%-30s  %-6s  %-10s\n", "Process Name", "PID", "RAM (KB)");
    printf("---------------------------------------------------------------\n");

    for (int i = 0; i < processCount; i++) {
        printf("%-30s  %-6u  %-10lu kb\n", processes[i].processName, processes[i].pid, processes[i].ramUsageKB);
    }
}

void SearchProcessByName() {
    ProcessInfo processes[1024];
    int processCount = GetProcesses(processes);

    if (processCount == 0) return;

    char searchTerm[MAX_PATH];
    printf("Enter process name to search: ");
    scanf("%s", searchTerm);

    SIZE_T totalRAM = 0;
    int found = 0;
    
    for (int i = 0; i < processCount; i++) {
        if (strstr(processes[i].processName, searchTerm) != NULL) {
            totalRAM += processes[i].ramUsageKB;
            found = 1;
        }
    }

    if (found) {
        printf("\nTotal RAM usage for processes matching '%s': %lu MB\n", searchTerm, totalRAM/1024);
    } else {
        printf("\nNo processes found matching '%s'.\n", searchTerm);
    }
}

int main() {
    int choice;

    while (1) {
        printf("\nMenu:\n");
        printf("1. View total RAM utilization\n");
        printf("2. View all processes and their RAM utilization\n");
        printf("3. Search for a process by name\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                PrintMemoryStatus();
                break;
            case 2:
                PrintAllProcessRAMUsage();
                break;
            case 3:
                SearchProcessByName();
                break;
            case 0:
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
