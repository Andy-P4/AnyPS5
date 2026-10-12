#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int SaveBitmap(HBITMAP hBmp, HDC hdc, int width, int height, const char* filename) {
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = height; // bottom-up
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    int rowSize = ((width * 3 + 3) / 4) * 4;
    int imageSize = rowSize * height;
    BYTE* pixels = (BYTE*)malloc(imageSize);
    if (!pixels) return 0;

    if (!GetDIBits(hdc, hBmp, 0, height, pixels, &bi, DIB_RGB_COLORS)) {
        free(pixels);
        return 0;
    }

    BITMAPFILEHEADER bfh;
    memset(&bfh, 0, sizeof(bfh));
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        free(pixels);
        return 0;
    }

    fwrite(&bfh, sizeof(bfh), 1, fp);
    fwrite(&bi.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
    fwrite(pixels, imageSize, 1, fp);
    fclose(fp);
    free(pixels);
    return 1;
}

static HWND g_targetHwnd = NULL;
static DWORD g_targetPid = 0;

static BOOL CALLBACK FindGameWndProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == g_targetPid) {
        char cls[256] = {0};
        GetClassNameA(hwnd, cls, sizeof(cls));
        if (strcmp(cls, "SDL_app") == 0) {
            g_targetHwnd = hwnd;
            return FALSE; // stop search
        }
    }
    return TRUE;
}

int main(int argc, char* argv[]) {
    HWINSTA hw = OpenWindowStationA("WinSta0", FALSE, GENERIC_ALL);
    if (hw) SetProcessWindowStation(hw);
    HDESK hd = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (hd) SetThreadDesktop(hd);

    g_targetPid = 3056;
    if (argc > 1) g_targetPid = (DWORD)atoi(argv[1]);

    EnumWindows(FindGameWndProc, 0);

    if (!g_targetHwnd) {
        printf("Error: SDL_app window for PID %lu not found\n", g_targetPid);
        return 1;
    }

    RECT rcWin;
    GetWindowRect(g_targetHwnd, &rcWin);
    int width = rcWin.right - rcWin.left;
    int height = rcWin.bottom - rcWin.top;

    printf("Game HWND=%p: Rect=(%ld,%ld,%ld,%ld), size: %dx%d\n",
           g_targetHwnd, rcWin.left, rcWin.top, rcWin.right, rcWin.bottom, width, height);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Capture screen area
    BOOL ok = BitBlt(hdcMem, 0, 0, width, height, hdcScreen, rcWin.left, rcWin.top, SRCCOPY | CAPTUREBLT);
    printf("BitBlt from screen DC result: %d\n", ok);

    const char* outPath = "S:\\AnyPS5\\extracted\\screenshot.bmp";
    if (argc > 2) outPath = argv[2];

    SaveBitmap(hBmp, hdcMem, width, height, outPath);
    printf("Saved screen BitBlt to %s\n", outPath);

    // Also PrintWindow
    char printPath[512];
    snprintf(printPath, sizeof(printPath), "%s.print.bmp", outPath);
    BOOL pwOk = PrintWindow(g_targetHwnd, hdcMem, 2 /* PW_RENDERFULLCONTENT */);
    printf("PrintWindow result: %d\n", pwOk);
    if (pwOk) {
        SaveBitmap(hBmp, hdcMem, width, height, printPath);
        printf("Saved PrintWindow capture to %s\n", printPath);
    }

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return 0;
}
