#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>
#define MAX_PROCESSES 1024
#define MAX_NAME_LENGTH 256
#define ID_LISTBOX 1
#define ID_SEARCH 2
#define ID_BUTTON 3

HWND hListBox;
char searchQuery[MAX_NAME_LENGTH];

void populateProcessList() {
    DWORD aProcesses[MAX_PROCESSES], cbNeeded, cProcesses;
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return;
    }

    cProcesses = cbNeeded / sizeof(DWORD);
    for (unsigned int i = 0; i < cProcesses; i++) {
        if (aProcesses[i] != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
            if (hProcess) {
                TCHAR processName[MAX_NAME_LENGTH] = TEXT("<unknown>");
                HMODULE hMod;
                DWORD cbNeeded;

                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleFileNameEx(hProcess, hMod, processName, sizeof(processName) / sizeof(TCHAR));
                }

                // Get memory usage
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    // Check if the process name matches the search query
                    if (strstr(processName, searchQuery) != NULL || strlen(searchQuery) == 0) {
                        char buffer[MAX_NAME_LENGTH + 50];
                        sprintf(buffer, "%s - Memory: %lu KB", processName, pmc.WorkingSetSize / 1024);
                        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)buffer);
                    }
                }

                CloseHandle(hProcess);
            }
        }
    }
}

// Window procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            hListBox = CreateWindow("LISTBOX", NULL,
                                     WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_SORT,
                                     10, 10, 400, 300,
                                     hwnd, (HMENU)ID_LISTBOX, NULL, NULL);
            CreateWindow("EDIT", NULL,
                         WS_CHILD | WS_VISIBLE | WS_BORDER,
                         10, 320, 300, 25,
                         hwnd, (HMENU)ID_SEARCH, NULL, NULL);
            CreateWindow("BUTTON", "Search",
                         WS_CHILD | WS_VISIBLE,
                         320, 320, 90, 25,
                         hwnd, (HMENU)ID_BUTTON, NULL, NULL);
            populateProcessList();
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BUTTON) { // Search button clicked
                GetDlgItemText(hwnd, ID_SEARCH, searchQuery, sizeof(searchQuery));
                populateProcessList();
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "TaskManagerClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Task Manager",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               450, 400, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}