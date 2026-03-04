#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <tchar.h>
#include <string.h>

void printTotalUtilization();
void printProcessUtilization(DWORD processID);
void printProgramUtilization();
void searchUtilization(const char* resource);

// Helper function to calculate CPU usage (simplified)
double getCPUUsage() {
    static ULONGLONG lastIdleTime = 0;
    static ULONGLONG lastKernelTime = 0;
    static ULONGLONG lastUserTime = 0;

    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return -1.0;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;

    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;

    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    ULONGLONG sysIdle = idle.QuadPart - lastIdleTime;
    ULONGLONG sysKernel = kernel.QuadPart - lastKernelTime;
    ULONGLONG sysUser = user.QuadPart - lastUserTime;
    ULONGLONG sysTotal = sysKernel + sysUser;

    lastIdleTime = idle.QuadPart;
    lastKernelTime = kernel.QuadPart;
    lastUserTime = user.QuadPart;

    if (sysTotal == 0) return 0.0;
    return (double)(sysTotal - sysIdle) * 100.0 / sysTotal;
}

void printTotalUtilization() {
    // CPU Utilization
    printf("Calculating CPU utilization... Please wait a moment.\n");
    // We call twice to calculate diff
    getCPUUsage();
    Sleep(500);  // short wait
    double cpuUtil = getCPUUsage();
    if (cpuUtil < 0) cpuUtil = 0;
    printf("Total CPU Utilization: %.2f%%\n", cpuUtil);

    // RAM Utilization
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        double ramUsed = (double)(memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);
        double ramTotal = (double)(memStatus.ullTotalPhys) / (1024 * 1024);
        double ramUtilization = ((ramTotal - ramUsed) / ramTotal) * 100.0;
        printf("RAM Utilization: %.2f%% (%.0f MB used of %.0f MB)\n", ramUtilization, ramUsed, ramTotal);
    } else {
        printf("Failed to get RAM status.\n");
    }

    // Disk utilization - Not straightforward, so just print free space for drives C: to Z:
    printf("Disk Utilization (Free Space on drives):\n");
    DWORD drives = GetLogicalDrives();
    for (char drive = 'C'; drive <= 'Z'; ++drive) {
        if (drives & (1 << (drive - 'A'))) {
            char rootPath[4] = { drive, ':', '\\', 0 };
            ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
            if (GetDiskFreeSpaceEx(rootPath, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                double freeMB = (double)totalNumberOfFreeBytes.QuadPart / (1024 * 1024);
                double totalMB = (double)totalNumberOfBytes.QuadPart / (1024 * 1024);
                double usedMB = totalMB - freeMB;
                double usagePercent = (usedMB / totalMB) * 100.0;
                printf(" %c:\\ %.2f%% used (%.0f MB / %.0f MB free)\n", drive, usagePercent, freeMB, totalMB);
            }
        }
    }

    // Network utilization - complex to implement fully; just print placeholder info.
    printf("Network Utilization: Feature not implemented in this demo.\n");
}

void printProcessUtilization(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (hProcess == NULL) {
        printf("Could not open process with ID %lu. Error: %lu\n", processID, GetLastError());
        return;
    }

    // Memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        printf("Failed to get process memory info. Error: %lu\n", GetLastError());
        CloseHandle(hProcess);
        return;
    }

    SIZE_T workingSet = pmc.WorkingSetSize;
    printf("Process ID: %lu\n", processID);
    printf("Working Set Size: %.2f MB\n", (double)workingSet / (1024 * 1024));

    // CPU time: get process times
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULONGLONG kTime = (((ULONGLONG)kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        ULONGLONG uTime = (((ULONGLONG)userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
        double totalSeconds = (double)(kTime + uTime) / 10000000.0;
        printf("CPU Time Used by Process: %.2f seconds\n", totalSeconds);
    } else {
        printf("Failed to get process CPU times. Error: %lu\n", GetLastError());
    }

    CloseHandle(hProcess);
}

void printProgramUtilization() {
    // List all processes and print their PID and Memory Usage (Working Set)
    DWORD processes[1024], needed, count;
    if (!EnumProcesses(processes, sizeof(processes), &needed)) {
        printf("Failed to enumerate processes.\n");
        return;
    }
    count = needed / sizeof(DWORD);
    printf("Running processes and memory usage:\n");
    printf("%-10s %-30s %12s\n", "PID", "Process Name", "Memory (MB)");

    for (unsigned int i = 0; i < count; i++) {
        if (processes[i] != 0) {
            DWORD pid = processes[i];
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcess) {
                HMODULE hMod;
                DWORD cbNeeded;
                TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
                }

                PROCESS_MEMORY_COUNTERS pmc;
                double memMB = 0.0;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    memMB = (double)pmc.WorkingSetSize / (1024 * 1024);
                }

#ifdef UNICODE
                char processName[256];
                wcstombs(processName, szProcessName, sizeof(processName));
                printf("%-10lu %-30s %12.2f\n", pid, processName, memMB);
#else
                printf("%-10lu %-30s %12.2f\n", pid, szProcessName, memMB);
#endif

                CloseHandle(hProcess);
            }
        }
    }
}

void searchUtilization(const char* resource) {
    if (_stricmp(resource, "CPU") == 0) {
        printTotalUtilization(); // print CPU and RAM, etc.
    } else if (_stricmp(resource, "RAM") == 0) {
        // Print RAM utilization only from printTotalUtilization code
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            double ramUsed = (double)(memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);
            double ramTotal = (double)(memStatus.ullTotalPhys) / (1024 * 1024);
            double ramUtilization = ((ramTotal - ramUsed) / ramTotal) * 100.0;
            printf("RAM Utilization: %.2f%% (%.0f MB used of %.0f MB)\n", ramUtilization, ramUsed, ramTotal);
        } else {
            printf("Failed to get RAM status.\n");
        }
    } else if (_stricmp(resource, "Network") == 0) {
        printf("Network Utilization: Feature not implemented in this demo.\n");
    } else if (_stricmp(resource, "Disk") == 0) {
        printf("Disk Utilization (Free Space on drives):\n");
        DWORD drives = GetLogicalDrives();
        for (char drive = 'C'; drive <= 'Z'; ++drive) {
            if (drives & (1 << (drive - 'A'))) {
                char rootPath[4] = { drive, ':', '\\', 0 };
                ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
                if (GetDiskFreeSpaceEx(rootPath, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                    double freeMB = (double)totalNumberOfFreeBytes.QuadPart / (1024 * 1024);
                    double totalMB = (double)totalNumberOfBytes.QuadPart / (1024 * 1024);
                    double usedMB = totalMB - freeMB;
                    double usagePercent = (usedMB / totalMB) * 100.0;
                    printf(" %c:\\ %.2f%% used (%.0f MB free of %.0f MB total)\n", drive, usagePercent, freeMB, totalMB);
                }
            }
        }
    } else {
        printf("Unknown resource type '%s'. Valid options: CPU, RAM, Network, Disk.\n", resource);
    }
}

int main() {
    int choice;
    do {
        printf("\nSelect an option:\n");
        printf("1. Print Total Utilization\n");
        printf("2. Print Utilization for a Process\n");
        printf("3. Print Utilization for a Program\n");
        printf("4. Search Utilization (CPU, RAM, Network, Disk)\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\\n'); // clear input buffer
            printf("Invalid input, try again.\n");
            continue;
        }

        switch (choice) {
            case 1:
                printTotalUtilization();
                break;
            case 2: {
                DWORD processID;
                printf("Enter Process ID: ");
                if (scanf("%lu", &processID) == 1) {
                    printProcessUtilization(processID);
                } else {
                    printf("Invalid process ID.\n");
                    while(getchar() != '\\n');
                }
                break;
            }
            case 3:
                printProgramUtilization();
                break;
            case 4: {
                char resource[20];
                printf("Enter resource to search (CPU, RAM, Network, Disk): ");
                scanf("%19s", resource);
                searchUtilization(resource);
                break;
            }
            case 5:
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 5);

    return 0;
}

