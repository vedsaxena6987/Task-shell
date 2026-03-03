#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <string>

#pragma comment(lib, "comctl32.lib")

typedef struct {
    double cpuTimeSeconds;
    SIZE_T ramUsageBytes;
    ULONGLONG diskReadBytes;
    ULONGLONG diskWriteBytes;
    int processCount;
} ProgramStats;

HWND hListView;

void InitListViewColumns(HWND hwndListView);
void InsertListViewItem(HWND hwndListView, int index, const char* programName, ProgramStats* stats);
void PopulateProgramData(HWND hwndListView);

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "ProcessUtilApp_Class";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Program Utilization Monitoring",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetTimer(hwnd, 1, 500, NULL);  // Set a timer to update every 0.5 seconds (500 ms)

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

void InitListViewColumns(HWND hwndListView) {
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    const char* columns[] = {
        "Program Name", "CPU Time (s)", "RAM (MB)", "Disk Read (MB)", "Disk Write (MB)", "Process Count"
    };
    int widths[] = { 220, 100, 100, 120, 120, 120 };
    int colCount = sizeof(columns) / sizeof(columns[0]);

    for (int i = 0; i < colCount; i++) {
        lvc.iSubItem = i;
        lvc.cx = widths[i];
        lvc.pszText = (LPSTR)columns[i];
        ListView_InsertColumn(hwndListView, i, &lvc);
    }
}

void InsertListViewItem(HWND hwndListView, int index, const char* programName, ProgramStats* stats) {
    char buf[256];

    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    lvi.pszText = (LPSTR)programName;
    ListView_InsertItem(hwndListView, &lvi);

    sprintf_s(buf, sizeof(buf), "%.2f", stats->cpuTimeSeconds);
    ListView_SetItemText(hwndListView, index, 1, buf);

    sprintf_s(buf, sizeof(buf), "%.2f", stats->ramUsageBytes / (1024.0*1024.0));
    ListView_SetItemText(hwndListView, index, 2, buf);

    sprintf_s(buf, sizeof(buf), "%.2f", stats->diskReadBytes / (1024.0*1024.0));
    ListView_SetItemText(hwndListView, index, 3, buf);

    sprintf_s(buf, sizeof(buf), "%.2f", stats->diskWriteBytes / (1024.0*1024.0));
    ListView_SetItemText(hwndListView, index, 4, buf);

    sprintf_s(buf, sizeof(buf), "%d", stats->processCount);
    ListView_SetItemText(hwndListView, index, 5, buf);
}

void PopulateProgramData(HWND hwndListView) {
    ListView_DeleteAllItems(hwndListView);

    DWORD processes[1024], cbNeeded, count;
    if (!EnumProcesses(processes, sizeof(processes), &cbNeeded)) {
        MessageBox(hwndListView, "Failed to enumerate processes!", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    count = cbNeeded / sizeof(DWORD);

    std::map<std::string, ProgramStats> programStatsMap;

    for (DWORD i = 0; i < count; i++) {
        DWORD pid = processes[i];
        if (pid == 0) continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) continue;

        HMODULE hMod;
        DWORD cbNeededMod;
        char procName[MAX_PATH] = "<unknown>";
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeededMod)) {
            GetModuleBaseNameA(hProcess, hMod, procName, sizeof(procName));
        }

        FILETIME creationTime, exitTime, kernelTime, userTime;
        double cpuSecs = 0.0;
        if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULONGLONG kTime = (((ULONGLONG)kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
            ULONGLONG uTime = (((ULONGLONG)userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;
            cpuSecs = (double)(kTime + uTime) / 10000000.0;
        }

        PROCESS_MEMORY_COUNTERS pmc = {0};
        SIZE_T ramBytes = 0;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            ramBytes = pmc.WorkingSetSize;
        }

        IO_COUNTERS ioCounters;
        ULONGLONG readBytes = 0, writeBytes = 0;
        if (GetProcessIoCounters(hProcess, &ioCounters)) {
            readBytes = ioCounters.ReadTransferCount;
            writeBytes = ioCounters.WriteTransferCount;
        }

        // Add stats to the program entry
        std::string programName(procName);
        if (programStatsMap.find(programName) == programStatsMap.end()) {
            ProgramStats stats = {0.0, 0, 0, 0, 0};
            programStatsMap[programName] = stats;
        }

        programStatsMap[programName].cpuTimeSeconds += cpuSecs;
        programStatsMap[programName].ramUsageBytes += ramBytes;
        programStatsMap[programName].diskReadBytes += readBytes;
        programStatsMap[programName].diskWriteBytes += writeBytes;
        programStatsMap[programName].processCount++;

        CloseHandle(hProcess);
    }

    int idx = 0;
    for (const auto& entry : programStatsMap) {
        InsertListViewItem(hwndListView, idx++, entry.first.c_str(), &entry.second);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            hListView = CreateWindowEx(
                WS_EX_CLIENTEDGE, WC_LISTVIEW, "",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL | WS_HSCROLL,
                10, 10, 860, 540,
                hwnd, NULL, GetModuleHandle(NULL), NULL);
            if (!hListView) {
                MessageBox(hwnd, "Failed to create ListView", "Error", MB_OK | MB_ICONERROR);
                return -1;
            }
            InitListViewColumns(hListView);
            PopulateProgramData(hListView);
            return 0;

        case WM_SIZE: {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            SetWindowPos(hListView, NULL, 10, 10, rcClient.right - 20, rcClient.bottom - 20, SWP_NOZORDER);
            return 0;
        }

        case WM_TIMER: {
            PopulateProgramData(hListView);  // Update the program data every 0.5 second
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);  // Stop the timer when the window is destroyed
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
