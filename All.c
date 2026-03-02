#include <windows.h>
#include <psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char processName[MAX_PATH];
    DWORD pid;
    double cpuTimeSeconds;
    SIZE_T ramUsageBytes;
    ULONGLONG diskReadBytes;
    ULONGLONG diskWriteBytes;
} ProcessUtilInfo;

void PrintAllProcessUtilization() {
    DWORD processIDs[1024], cbNeeded, numProcesses;
    if (!EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded)) {
        printf("Failed to enumerate processes.\n");
        return;
    }
    numProcesses = cbNeeded / sizeof(DWORD);

    ProcessUtilInfo* infos = (ProcessUtilInfo*)malloc(numProcesses * sizeof(ProcessUtilInfo));
    if (!infos) {
        printf("Out of memory.\n");
        return;
    }
    int count = 0;

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

        processName[MAX_PATH - 1] = '\0';

        FILETIME creationTime, exitTime, kernelTime, userTime;
        double cpuTimeSeconds = 0.0;
        if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULONGLONG kTime = (((ULONGLONG)kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
            ULONGLONG uTime = (((ULONGLONG)userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
            cpuTimeSeconds = (double)(kTime + uTime) / 10000000.0;
        }

        PROCESS_MEMORY_COUNTERS pmc;
        SIZE_T workingSet = 0;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            workingSet = pmc.WorkingSetSize;
        }

        IO_COUNTERS ioCounters;
        ULONGLONG readBytes = 0, writeBytes = 0;
        if (GetProcessIoCounters(hProcess, &ioCounters)) {
            readBytes = ioCounters.ReadTransferCount;
            writeBytes = ioCounters.WriteTransferCount;
        }

        strncpy(infos[count].processName, processName, MAX_PATH);
        infos[count].pid = pid;
        infos[count].cpuTimeSeconds = cpuTimeSeconds;
        infos[count].ramUsageBytes = workingSet;
        infos[count].diskReadBytes = readBytes;
        infos[count].diskWriteBytes = writeBytes;
        count++;

        CloseHandle(hProcess);
    }

    printf("%-25s %-6s %12s %12s %15s %15s %10s\n",
           "Process Name", "PID", "CPU (s)", "RAM (MB)", "Disk Read (MB)", "Disk Write (MB)", "Network");
    printf("-------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-25s %-6u %12.2f %12.2f %15.2f %15.2f %10s\n",
            infos[i].processName,
            infos[i].pid,
            infos[i].cpuTimeSeconds,
            infos[i].ramUsageBytes / (1024.0 * 1024.0),
            infos[i].diskReadBytes / (1024.0 * 1024.0),
            infos[i].diskWriteBytes / (1024.0 * 1024.0),
            "N/A");
    }
    free(infos);
}

int main(){
    PrintAllProcessUtilization();
    return 0;
}