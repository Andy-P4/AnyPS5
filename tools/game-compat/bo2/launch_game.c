#include <windows.h>
#include <stdio.h>

static HWND g_foundHwnd = NULL;
static DWORD g_targetPid = 0;
static volatile BOOL g_running = TRUE;

static BOOL CALLBACK EnumFindGameWnd(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == g_targetPid) {
        char cls[256] = {0};
        GetClassNameA(hwnd, cls, sizeof(cls));
        if (strcmp(cls, "SDL_app") == 0) {
            g_foundHwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

static DWORD WINAPI MonitorWindowThread(LPVOID p) {
    DWORD pid = (DWORD)(uintptr_t)p;
    g_targetPid = pid;
    printf("[launcher] Window watcher active for PID %lu on WinSta0\\Default\n", pid);
    fflush(stdout);

    HWINSTA hw = OpenWindowStationA("WinSta0", FALSE, GENERIC_ALL);
    if (hw) SetProcessWindowStation(hw);
    HDESK hd = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (hd) SetThreadDesktop(hd);

    BOOL windowPositioned = FALSE;
    DWORD startTick = GetTickCount();

    while (g_running) {
        Sleep(250);
        if (!windowPositioned) {
            g_foundHwnd = NULL;
            EnumWindows(EnumFindGameWnd, 0);
            if (g_foundHwnd) {
                RECT rc;
                if (GetWindowRect(g_foundHwnd, &rc)) {
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;
                    int x = 2560 + (2560 - w) / 2;
                    int y = (1440 - h) / 2;
                    printf("[launcher] Located window %p (%dx%d). Moving to monitor 2: X=%d, Y=%d\n", g_foundHwnd, w, h, x, y);
                    fflush(stdout);
                    SetWindowPos(g_foundHwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                    ShowWindow(g_foundHwnd, SW_SHOWNORMAL);
                    SetForegroundWindow(g_foundHwnd);
                    windowPositioned = TRUE;
                }
            }
        } else {
            // Send Enter only at specific key milestones to dismiss cutscenes and Start prompt without selecting Exit
            DWORD elapsed = GetTickCount() - startTick;
            static DWORD lastSend = 0;
            if ((elapsed >= 4000 && elapsed < 4500 && lastSend < 4000) ||
                (elapsed >= 7000 && elapsed < 7500 && lastSend < 7000) ||
                (elapsed >= 10000 && elapsed < 10500 && lastSend < 10000) ||
                (elapsed >= 14000 && elapsed < 14500 && lastSend < 14000)) {
                lastSend = elapsed;
                printf("[launcher] Sending discrete Enter key at %lu ms to advance...\n", elapsed);
                fflush(stdout);
                SetForegroundWindow(g_foundHwnd);
                keybd_event(VK_RETURN, 0x1C, 0, 0);
                Sleep(50);
                keybd_event(VK_RETURN, 0x1C, KEYEVENTF_KEYUP, 0);

                LPARAM lpRetDown = 1 | (0x1C << 16);
                LPARAM lpRetUp   = 1 | (0x1C << 16) | (1 << 30) | (1 << 31);
                PostMessageA(g_foundHwnd, WM_KEYDOWN, VK_RETURN, lpRetDown);
                PostMessageA(g_foundHwnd, WM_KEYUP, VK_RETURN, lpRetUp);
            }
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    printf("[launcher] Initializing display station WinSta0\\Default...\n");
    fflush(stdout);

    HWINSTA hw = OpenWindowStationA("WinSta0", FALSE, GENERIC_ALL);
    if (hw) SetProcessWindowStation(hw);
    HDESK hd = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (hd) SetThreadDesktop(hd);

    // Set environment variables
    SetEnvironmentVariableA("VK_LOADER_DRIVERS_DISABLE", "*dzn*");
    SetEnvironmentVariableA("APS5_NO_DRAW_PRECHECK", "1");
    SetEnvironmentVariableA("APS5_DUMP_FRAMES", "500");
    SetEnvironmentVariableA("APS5_DUMP_FRAMES_EVERY", "20");

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hOut = CreateFileA("S:\\AnyPS5\\extracted\\game_stdout.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hErr = CreateFileA("S:\\AnyPS5\\extracted\\game_stderr.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = (hOut != INVALID_HANDLE_VALUE) ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = (hErr != INVALID_HANDLE_VALUE) ? hErr : GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.lpDesktop = "WinSta0\\Default";

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    char cmdLine[512] = "S:\\AnyPS5\\extracted\\eboot.exe";

    printf("[launcher] Spawning eboot.exe on interactive desktop WinSta0\\Default...\n");
    fflush(stdout);

    BOOL ok = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        "S:\\AnyPS5\\extracted",
        &si,
        &pi
    );

    if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
    if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);

    if (!ok) {
        printf("[launcher] Failed to create process: error %lu\n", GetLastError());
        return 1;
    }

    printf("[launcher] eboot.exe started successfully with PID %lu\n", pi.dwProcessId);
    fflush(stdout);

    HANDLE hWatcher = CreateThread(NULL, 0, MonitorWindowThread, (LPVOID)(uintptr_t)pi.dwProcessId, 0, NULL);

    // Wait for eboot to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    g_running = FALSE;
    if (hWatcher) {
        WaitForSingleObject(hWatcher, 2000);
        CloseHandle(hWatcher);
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    printf("[launcher] eboot.exe terminated with exit code %lu\n", exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
}
