#include <windows.h>
#include <psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 1024

typedef struct {
    char processName[MAX_PATH];
    DWORD pid;
    double cpuUsage; // CPU usage percentage (here: total CPU time in seconds)
} ProcessInfo;

void PrintCPUStatus();
int GetProcesses(ProcessInfo processes[]);
void PrintAllProcessCPUUsage();
void SearchProcessByName();

void PrintCPUStatus() {
    FILETIME idleTime1, kernelTime1, userTime1;
    FILETIME idleTime2, kernelTime2, userTime2;

    if (!GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) {
        printf("Failed to get system times (first sample).\n");
        return;
    }

    Sleep(500);  // Wait half a second

    if (!GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) {
        printf("Failed to get system times (second sample).\n");
        return;
    }

    ULARGE_INTEGER idle1, kernel1, user1;
    ULARGE_INTEGER idle2, kernel2, user2;

    idle1.LowPart = idleTime1.dwLowDateTime;
    idle1.HighPart = idleTime1.dwHighDateTime;
    kernel1.LowPart = kernelTime1.dwLowDateTime;
    kernel1.HighPart = kernelTime1.dwHighDateTime;
    user1.LowPart = userTime1.dwLowDateTime;
    user1.HighPart = userTime1.dwHighDateTime;

    idle2.LowPart = idleTime2.dwLowDateTime;
    idle2.HighPart = idleTime2.dwHighDateTime;
    kernel2.LowPart = kernelTime2.dwLowDateTime;
    kernel2.HighPart = kernelTime2.dwHighDateTime;
    user2.LowPart = userTime2.dwLowDateTime;
    user2.HighPart = userTime2.dwHighDateTime;

    ULONGLONG idleDiff = idle2.QuadPart - idle1.QuadPart;
    ULONGLONG kernelDiff = kernel2.QuadPart - kernel1.QuadPart;
    ULONGLONG userDiff = user2.QuadPart - user1.QuadPart;

    ULONGLONG system = kernelDiff + userDiff;

    if (system == 0) {
        printf("Invalid system time difference.\n");
        return;
    }

    double cpuUsage = (double)(system - idleDiff) * 100.0 / system;
    printf("Total CPU Utilization: %.2f%%\n", cpuUsage);
}

int GetProcesses(ProcessInfo processes[]) {
    DWORD processIDs[MAX_PROCESSES], cbNeeded, numProcesses;
    int processCount = 0;

    if (!EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded)) {
        printf("Failed to enumerate processes.\n");
        return 0;
    }

    numProcesses = cbNeeded / sizeof(DWORD);

    for (DWORD i = 0; i < numProcesses && processCount < MAX_PROCESSES; i++) {
        DWORD pid = processIDs[i];
        if (pid == 0) continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) continue;

        HMODULE hMod;
        DWORD cbNeededMod;
        char processName[MAX_PATH] = "<unknown>";
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeededMod)) {
            GetModuleBaseNameA(hProcess, hMod, processName, sizeof(processName));
        }

        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULONGLONG kTime = (((ULONGLONG)kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
            ULONGLONG uTime = (((ULONGLONG)userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
            double totalTime = (double)(kTime + uTime) / 10000000.0; // Convert to seconds

            strncpy(processes[processCount].processName, processName, MAX_PATH);
            processes[processCount].pid = pid;
            processes[processCount].cpuUsage = totalTime;
            processCount++;
        }

        CloseHandle(hProcess);
    }

    return processCount;
}

void PrintAllProcessCPUUsage() {
    ProcessInfo processes[MAX_PROCESSES];
    int processCount = GetProcesses(processes);

    if (processCount == 0) return;

    printf("\n--- Process CPU Usage ---\n");
    printf("%-30s  %-6s  %-10s\n", "Process Name", "PID", "CPU Time (s)");
    printf("---------------------------------------------------------------\n");

    for (int i = 0; i < processCount; i++) {
        printf("%-30s  %-6u  %-10.2f\n", processes[i].processName, processes[i].pid, processes[i].cpuUsage);
    }
}

void SearchProcessByName() {
    ProcessInfo processes[MAX_PROCESSES];
    int processCount = GetProcesses(processes);

    if (processCount == 0) return;

    char searchTerm[MAX_PATH];
    printf("Enter process name to search: ");
    scanf("%s", searchTerm);

    double totalCPU = 0.0;
    int found = 0;

    for (int i = 0; i < processCount; i++) {
        if (strstr(processes[i].processName, searchTerm) != NULL) {
            totalCPU += processes[i].cpuUsage;
            found = 1;
        }
    }

    if (found) {
        printf("\nTotal CPU time for processes matching '%s': %.2f seconds\n", searchTerm, totalCPU);
    } else {
        printf("\nNo processes found matching '%s'.\n", searchTerm);
    }
}

int main() {
    int choice;

    while (1) {
        printf("\nMenu:\n");
        printf("1. View total CPU utilization\n");
        printf("2. View all processes and their CPU usage\n");
        printf("3. Search for a process by name\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                PrintCPUStatus();
                break;
            case 2:
                PrintAllProcessCPUUsage();
                break;
            case 3:
                SearchProcessByName();
                break;
            case 0:
                printf("Exiting...\n");
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
