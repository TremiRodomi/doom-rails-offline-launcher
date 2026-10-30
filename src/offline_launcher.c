/* Doom Rails 1.1.0 offline launcher. Build instructions are in ../README.md. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static DWORD game_pid;
static BOOL steam_build;
static HWND registration, visible_game;

static void error(const wchar_t *message) {
    MessageBoxW(NULL, message, L"Doom Rails offline launcher", MB_OK | MB_ICONERROR);
}

static BOOL correct_build(const wchar_t *path) {
    static const BYTE expected[32] = {
        0xa0,0x9b,0x55,0x02,0x34,0x33,0x9b,0xac,0x08,0x77,0x68,0xfc,0x5f,0x95,0xe9,0xa0,
        0x26,0xab,0x8b,0x23,0xf4,0x19,0x5c,0x25,0x7f,0x82,0x42,0xdc,0x69,0xaa,0x33,0x9f
    };
    static const BYTE steam_expected[32] = {
        0xfb,0xba,0x9d,0x10,0xff,0x5d,0xa3,0xeb,0x00,0x6f,0xe6,0xf9,0xca,0x02,0x14,0xd4,
        0x1b,0xcf,0xb4,0x2a,0x34,0x3e,0x2f,0x81,0xd9,0x9a,0xd4,0x7d,0xee,0xc6,0xd8,0xe9
    };
    steam_build = FALSE;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BOOL ok = CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    if (ok) ok = CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
    BYTE buffer[65536], digest[32];
    DWORD count = 0;
    while (ok) {
        ok = ReadFile(file, buffer, sizeof(buffer), &count, NULL);
        if (!ok || !count) break;
        ok = CryptHashData(hash, buffer, count, 0);
    }
    DWORD length = sizeof(digest);
    if (ok) {
        ok = CryptGetHashParam(hash, HP_HASHVAL, digest, &length, 0) && length == sizeof(expected);
        if (ok) {
            steam_build = !memcmp(digest, steam_expected, sizeof(steam_expected));
            ok = steam_build || !memcmp(digest, expected, sizeof(expected));
        }
    }
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
}

static BOOL CALLBACK find_windows(HWND window, LPARAM unused) {
    (void)unused;
    DWORD pid;
    GetWindowThreadProcessId(window, &pid);
    if (pid != game_pid) return TRUE;
    wchar_t name[128] = {0}, cls[128] = {0};
    GetWindowTextW(window, name, 128);
    GetClassNameW(window, cls, 128);
    if (!wcscmp(cls, L"#32770") && !wcscmp(name, L"Registration Dialog")) registration = window;
    if (!wcscmp(cls, L"DOOM_RAILS_WIN") && IsWindowVisible(window)) visible_game = window;
    return TRUE;
}

static BOOL complete_registration(HANDLE process, HWND dialog) {
    /* Resolve the remote address by module and offset, including forwarded exports. */
    FARPROC end_dialog = GetProcAddress(GetModuleHandleW(L"user32.dll"), "EndDialog");
    HMODULE owner = NULL;
    if (!end_dialog || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)end_dialog, &owner)) return FALSE;
    wchar_t owner_path[MAX_PATH];
    if (!GetModuleFileNameW(owner, owner_path, MAX_PATH)) return FALSE;
    const wchar_t *owner_name = wcsrchr(owner_path, L'\\');
    owner_name = owner_name ? owner_name + 1 : owner_path;
    DWORD offset = (DWORD)(uintptr_t)end_dialog - (DWORD)(uintptr_t)owner;
    DWORD destination = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, game_pid);
    if (snapshot == INVALID_HANDLE_VALUE) return FALSE;
    MODULEENTRY32W module = {.dwSize = sizeof(module)};
    if (Module32FirstW(snapshot, &module)) do {
        if (!_wcsicmp(module.szModule, owner_name) && offset < module.modBaseSize)
            destination = (DWORD)(uintptr_t)module.modBaseAddr + offset;
    } while (Module32NextW(snapshot, &module));
    CloseHandle(snapshot);
    if (!destination) return FALSE;

    /* EndDialog(dialog, 1): the game handles this as registration succeeded.
       No original game instructions or files are patched. */
    BYTE stub[] = {0x6a,1,0x68,0,0,0,0,0xb8,0,0,0,0,0xff,0xd0,0xc2,4,0};
    DWORD window_value = (DWORD)(uintptr_t)dialog;
    memcpy(stub + 3, &window_value, 4);
    memcpy(stub + 8, &destination, 4);
    void *remote = VirtualAllocEx(process, NULL, sizeof(stub), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remote) return FALSE;
    SIZE_T written = 0;
    DWORD old_protection;
    BOOL ok = WriteProcessMemory(process, remote, stub, sizeof(stub), &written) && written == sizeof(stub)
              && VirtualProtectEx(process, remote, sizeof(stub), PAGE_EXECUTE_READ, &old_protection)
              && FlushInstructionCache(process, remote, sizeof(stub));
    HANDLE worker = ok ? CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)remote, NULL, 0, NULL) : NULL;
    if (!worker) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return FALSE;
    }
    DWORD result = 0;
    DWORD waited = WaitForSingleObject(worker, 10000);
    ok = waited == WAIT_OBJECT_0 && GetExitCodeThread(worker, &result) && result != 0;
    CloseHandle(worker);
    if (waited == WAIT_OBJECT_0) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return ok;
}

/* These two independent protected checks guard Listen() and Connect().
   Only the conditional branch opcode changes; original files stay intact. */
static BOOL write_branch(HANDLE process, DWORD address, BYTE opcode) {
    void *site = (void *)(uintptr_t)address;
    DWORD old, unused;
    SIZE_T count = 0;
    BYTE verified = 0;
    if (!VirtualProtectEx(process, site, 1, PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    BOOL ok = WriteProcessMemory(process, site, &opcode, 1, &count) && count == 1;
    BOOL flushed = FlushInstructionCache(process, site, 1);
    BOOL restored = VirtualProtectEx(process, site, 1, old, &unused);
    return ok && flushed && restored &&
        ReadProcessMemory(process, site, &verified, 1, &count) && count == 1 && verified == opcode;
}

static BOOL enable_multiplayer_at(HANDLE process, DWORD timeout_ms, const DWORD sites[2]) {
    static const BYTE offsets[2] = {0x6a, 0x60};
    BYTE original[2][2];
    DWORD started = GetTickCount();
    for (;;) {
        BOOL ready = TRUE;
        for (int i = 0; i < 2; ++i) {
            SIZE_T count = 0;
            if (!ReadProcessMemory(process, (void *)(uintptr_t)sites[i], original[i], 2, &count) ||
                count != 2 || original[i][1] != offsets[i] ||
                (original[i][0] != 0x75 && original[i][0] != 0xeb)) ready = FALSE;
        }
        if (ready) break;
        if (GetTickCount() - started >= timeout_ms ||
            WaitForSingleObject(process, 100) == WAIT_OBJECT_0) return FALSE;
    }
    for (int i = 0; i < 2; ++i) {
        if (!write_branch(process, sites[i], 0xeb)) {
            for (int j = 0; j <= i; ++j) write_branch(process, sites[j], original[j][0]);
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL enable_multiplayer(HANDLE process, DWORD timeout_ms) {
    static const DWORD sites[2] = {0x4929e0, 0x494d18};
    static const DWORD steam_sites[2] = {0x494340, 0x496678};
    return enable_multiplayer_at(process, timeout_ms, steam_build ? steam_sites : sites);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show) {
    (void)instance; (void)previous; (void)show; (void)arguments;
    wchar_t directory[MAX_PATH], executable[MAX_PATH], command[MAX_PATH + 4];
    DWORD length = GetModuleFileNameW(NULL, directory, MAX_PATH);
    if (!length || length >= MAX_PATH) { error(L"The launcher path is too long."); return 1; }
    wchar_t *slash = wcsrchr(directory, L'\\');
    if (!slash) return 1;
    *slash = 0;
    BOOL windowed = FALSE, wait_for_game = FALSE;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    for (int i = 1; i < argc; ++i) {
        if (!wcscmp(argv[i], L"--windowed")) windowed = TRUE;
        else if (!wcscmp(argv[i], L"--wait")) wait_for_game = TRUE;
        else if (!wcscmp(argv[i], L"--game-dir") && i + 1 < argc) {
            if (wcslen(argv[++i]) >= MAX_PATH) {
                LocalFree(argv); error(L"The game path is too long."); return 1;
            }
            wcscpy(directory, argv[i]);
        } else {
            LocalFree(argv); error(L"Unknown option. Use --game-dir PATH, --windowed, or --wait."); return 1;
        }
    }
    LocalFree(argv);
    if (wcslen(directory) + 17 >= MAX_PATH) { error(L"The game path is too long."); return 1; }
    swprintf(executable, MAX_PATH, L"%ls\\Doom Rails.exe", directory);
    if (!correct_build(executable)) {
        error(L"Place this launcher beside the original Doom Rails.exe from the supported standalone or Steam version 1.1.0.\n\nThe executable is missing or its SHA-256 does not match the tested build.");
        return 2;
    }
    if (windowed) {
        HKEY key;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\DreamForge Entertainment\\Doom Rails\\Global Settings",
                0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) == ERROR_SUCCESS) {
            DWORD enabled = 1;
            RegSetValueExW(key, L"Windowed", 0, REG_DWORD, (BYTE *)&enabled, sizeof(enabled));
            RegCloseKey(key);
        }
    }
    swprintf(command, MAX_PATH + 4, L"\"%ls\"", executable);
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process = {0};
    if (!CreateProcessW(executable, command, NULL, NULL, FALSE, 0, NULL, directory, &startup, &process)) {
        error(L"Windows could not start Doom Rails.exe.");
        return 3;
    }
    CloseHandle(process.hThread);
    game_pid = process.dwProcessId;
    int outcome = 4;
    DWORD started = GetTickCount();
    while (GetTickCount() - started < 60000) {
        if (WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0) break;
        registration = visible_game = NULL;
        EnumWindows(find_windows, 0);
        if (registration) {
            outcome = complete_registration(process.hProcess, registration) ? 0 : 5;
            break;
        }
        if (visible_game) { outcome = 0; break; }
    }
    if (!outcome && !enable_multiplayer(process.hProcess, 60000)) outcome = 6;
    if (!outcome && wait_for_game) {
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD game_exit = 0;
        GetExitCodeProcess(process.hProcess, &game_exit);
        CloseHandle(process.hProcess);
        return (int)game_exit;
    }
    if (outcome) {
        DWORD game_exit = STILL_ACTIVE;
        GetExitCodeProcess(process.hProcess, &game_exit);
        if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
            wchar_t message[384];
            swprintf(message, 384,
                L"Doom Rails exited during startup (exit code 0x%08lX).\n\n"
                L"The game stopped before startup completed. Check its runtime or graphics error log.", game_exit);
            error(message);
        } else {
            if (outcome == 6)
                error(L"The multiplayer checks could not be updated. Close the game and retry the launcher.\n\nThe original executable has not been modified.");
            else if (outcome == 5)
                error(L"The registration helper failed. The game is still running; inspect its dialog and close the game before retrying.");
            else
                error(L"Timed out waiting for the registration dialog or game window. The game is still running; inspect its message and close it before retrying.");
            /* Keep Proton alive while the user inspects a failed startup. */
            if (wait_for_game) WaitForSingleObject(process.hProcess, INFINITE);
        }
    }
    CloseHandle(process.hProcess);
    return outcome;
}
