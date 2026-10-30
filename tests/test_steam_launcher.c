#define main patch_test_unused
#include "../src/offline_launcher.c"
#undef main
#include <assert.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" --game-dir \"%s\" --windowed --wait", argv[1], argv[2]);
    STARTUPINFOA startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION helper = {0};
    assert(CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &helper));
    HANDLE game = NULL;
    DWORD started = GetTickCount();
    while (GetTickCount() - started < 60000 && !game) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W entry = {.dwSize = sizeof(entry)};
        if (Process32FirstW(snap, &entry)) do {
            if (entry.th32ParentProcessID == helper.dwProcessId && !wcscmp(entry.szExeFile, L"Doom Rails.exe"))
                game = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
        } while (Process32NextW(snap, &entry));
        CloseHandle(snap); Sleep(100);
    }
    assert(game);
    BOOL patched = FALSE;
    while (GetTickCount() - started < 90000) {
        BYTE host[2], join[2]; SIZE_T n;
        if (ReadProcessMemory(game, (void *)0x494340, host, 2, &n) &&
            ReadProcessMemory(game, (void *)0x496678, join, 2, &n) &&
            host[0] == 0xeb && host[1] == 0x6a && join[0] == 0xeb && join[1] == 0x60) {
            patched = TRUE; break;
        }
        Sleep(100);
    }
    assert(patched);
    assert(WaitForSingleObject(helper.hProcess, 1000) == WAIT_TIMEOUT);
    assert(TerminateProcess(game, 42));
    assert(WaitForSingleObject(helper.hProcess, 10000) == WAIT_OBJECT_0);
    DWORD code; assert(GetExitCodeProcess(helper.hProcess, &code) && code == 42);
    CloseHandle(game); CloseHandle(helper.hProcess); CloseHandle(helper.hThread);
    puts("PASS: external --game-dir, Steam patch sites, --wait lifetime, game exit code propagation");
    return 0;
}
