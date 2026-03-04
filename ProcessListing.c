#include <windows.h>
#include<stdio.h>
#include <tchar.h>
#include<tlhelp32.h>
int main()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
     
        printf(GetLastError());
        return 1;
    }
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snapshot, &entry)) {
        do {
            _tprintf(_T("Process Name: %s\n"), entry.szExeFile);
        } while (Process32Next(snapshot, &entry));
    }
    printf("\n-------------------------\n");
    printf("Enter a process to be searched:-\n");
    char process_search[30];
    scanf("%s",&process_search);

    if (Process32First(snapshot, &entry)) {
        do {
            if(strcmp(process_search, entry.szExeFile) == 0)
            {
                _tprintf(_T("Process Name found : %s\n"), entry.szExeFile);
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}