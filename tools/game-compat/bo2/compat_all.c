#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define APS5_VABI __attribute__((sysv_abi))

// Original libSceWebBrowserDialog exports
static int g_web_dialog_status = 0;
int APS5_VABI impl_web_dialog_init(void) { g_web_dialog_status = 1; return 0; }
int APS5_VABI impl_web_dialog_term(void) { g_web_dialog_status = 0; return 0; }
int APS5_VABI impl_web_dialog_open(const void* p) { (void)p; g_web_dialog_status = 3; return 0; }
int APS5_VABI impl_web_dialog_status(void) { return g_web_dialog_status; }
int APS5_VABI jqb7HntFQFc(void) { return impl_web_dialog_init(); }
int APS5_VABI ocHtyBwHfys(void) { return impl_web_dialog_term(); }

typedef int (APS5_VABI *fn_dialog_open)(const void*);
static fn_dialog_open g_commerce_open = NULL;
int APS5_VABI impl_sceNpCommerceDialogOpen2(const void* param) {
    if (!g_commerce_open) {
        HMODULE h = GetModuleHandleA("libSceNpCommerce.prx");
        if (!h) h = LoadLibraryA("libs\\libSceNpCommerce.prx");
        if (h) g_commerce_open = (fn_dialog_open)GetProcAddress(h, "DfSCDRA3EjY");
    }
    if (g_commerce_open) return g_commerce_open(param);
    return 0;
}

int APS5_VABI impl_sceNpGetUserIdByAccountId(uint64_t account_id, int32_t* user_id) {
    (void)account_id;
    if (user_id) *user_id = 0x10000000;
    return 0;
}

// Dedicated implementations
typedef int32_t (APS5_VABI *fn_map_flex)(void**, size_t, int, int, const char*);
static fn_map_flex g_map_flex = NULL;
int32_t APS5_VABI impl_map_flex_internal(void** addr, size_t len, int prot, int flags, const char* name) {
    if (!g_map_flex) {
        HMODULE h = GetModuleHandleA("libkernel.prx");
        if (h) g_map_flex = (fn_map_flex)GetProcAddress(h, "mL8NDH86iQI");
    }
    if (g_map_flex) return g_map_flex(addr, len, prot, flags & ~0x8000, name);
    return 0;
}

// Semaphore implementations (Win32 backed)
int APS5_VABI impl_sem_init(void** sem, int flag, unsigned int value, const char* name) {
    if (!sem) return 0x80020016;
    HANDLE h = CreateSemaphoreA(NULL, (LONG)value, 0x7FFFFFFF, NULL);
    if (!h) return 0x8002000c;
    *sem = (void*)h;
    return 0;
}
int APS5_VABI impl_sem_destroy(void** sem) {
    if (!sem || !*sem) return 0x80020016;
    CloseHandle((HANDLE)*sem);
    *sem = NULL;
    return 0;
}
int APS5_VABI impl_sem_post(void** sem) {
    if (!sem || !*sem) return 0x80020016;
    ReleaseSemaphore((HANDLE)*sem, 1, NULL);
    return 0;
}
int APS5_VABI impl_sem_wait(void** sem) {
    if (!sem || !*sem) return 0x80020016;
    DWORD r = WaitForSingleObject((HANDLE)*sem, INFINITE);
    return (r == WAIT_OBJECT_0) ? 0 : 0x8002000b;
}
int APS5_VABI impl_sem_trywait(void** sem) {
    if (!sem || !*sem) return 0x80020016;
    DWORD r = WaitForSingleObject((HANDLE)*sem, 0);
    return (r == WAIT_OBJECT_0) ? 0 : (int)0x80020010;
}
int APS5_VABI impl_sem_timedwait(void** sem, uint64_t usec) {
    if (!sem || !*sem) return 0x80020016;
    DWORD ms = (DWORD)(usec / 1000);
    DWORD r = WaitForSingleObject((HANDLE)*sem, ms);
    return (r == WAIT_OBJECT_0) ? 0 : (int)0x8002003c;
}
int APS5_VABI impl_sem_getvalue(void** sem, int* value) {
    if (!sem || !*sem || !value) return 0x80020016;
    *value = 0;
    return 0;
}

static void install_hook(void* target, void* replacement) {
    if (!target || !replacement) return;
    DWORD oldProt;
    if (VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &oldProt)) {
        uint8_t* p = (uint8_t*)target;
        p[0] = 0xFF;
        p[1] = 0x25;
        p[2] = 0x00;
        p[3] = 0x00;
        p[4] = 0x00;
        p[5] = 0x00;
        *(uint64_t*)(p + 6) = (uint64_t)replacement;
        VirtualProtect(target, 14, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), target, 14);
    }
}
void install_kernel_hooks(void) {
    HMODULE h = GetModuleHandleA("libkernel.prx");
    if (!h) return;
    void* p_destroy   = (void*)GetProcAddress(h, "Vwc+L05e6oE");
    void* p_getvalue  = (void*)GetProcAddress(h, "DjpBvGlaWbQ");
    void* p_init      = (void*)GetProcAddress(h, "GEnUkDZoUwY");
    void* p_post      = (void*)GetProcAddress(h, "aishVAiFaYM");
    void* p_timedwait = (void*)GetProcAddress(h, "fjN6NQHhK8k");
    void* p_trywait   = (void*)GetProcAddress(h, "H2a+IN9TP0E");
    void* p_wait      = (void*)GetProcAddress(h, "C36iRE0F5sE");
    if (p_destroy)   install_hook(p_destroy, (void*)impl_sem_destroy);
    if (p_getvalue)  install_hook(p_getvalue, (void*)impl_sem_getvalue);
    if (p_init)      install_hook(p_init, (void*)impl_sem_init);
    if (p_post)      install_hook(p_post, (void*)impl_sem_post);
    if (p_timedwait) install_hook(p_timedwait, (void*)impl_sem_timedwait);
    if (p_trywait)   install_hook(p_trywait, (void*)impl_sem_trywait);
    if (p_wait)      install_hook(p_wait, (void*)impl_sem_wait);
}
typedef struct {
    void* ptr;
    size_t size;
} AjmBuffer;

typedef int (APS5_VABI *fn_sceAjmBatchJobRunSplit)(
    void* info,
    uint32_t instance,
    uint64_t flags,
    const AjmBuffer* input_buffers,
    size_t input_buffers_num,
    const AjmBuffer* output_buffers,
    size_t output_buffers_num,
    void* sideband_output,
    size_t sideband_output_size
);

static fn_sceAjmBatchJobRunSplit g_sceAjmBatchJobRunSplit = NULL;

int APS5_VABI impl_sceAjmBatchJobDecodeSingle(
    void* info,
    uint32_t instance,
    const void* bitstream_input,
    size_t bitstream_input_size,
    void* pcm_output,
    size_t pcm_output_size,
    void* result
) {
    if (!g_sceAjmBatchJobRunSplit) {
        HMODULE h = GetModuleHandleA("libSceAjm.native.prx");
        if (!h) h = LoadLibraryA("libSceAjm.native.prx");
        if (h) g_sceAjmBatchJobRunSplit = (fn_sceAjmBatchJobRunSplit)GetProcAddress(h, "Z9NVCesiP0Q");
    }
    if (g_sceAjmBatchJobRunSplit) {
        const AjmBuffer in_buf = { (void*)bitstream_input, bitstream_input_size };
        const AjmBuffer out_buf = { pcm_output, pcm_output_size };
        return g_sceAjmBatchJobRunSplit(
            info,
            instance,
            (uint64_t)1 << 47,
            &in_buf,
            1,
            &out_buf,
            1,
            result,
            24
        );
    }
    return 0;
}

int APS5_VABI impl_sceAjmBatchJobDecode(
    void* info,
    uint32_t instance,
    const void* bitstream_input,
    size_t bitstream_input_size,
    void* pcm_output,
    size_t pcm_output_size,
    void* result
) {
    return impl_sceAjmBatchJobDecodeSingle(info, instance, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result);
}

int APS5_VABI impl_sceAjmBatchJobDecodeSplit(
    void* info,
    uint32_t instance,
    const AjmBuffer* input_buffers,
    size_t input_buffers_num,
    const AjmBuffer* output_buffers,
    size_t output_buffers_num,
    void* result
) {
    if (!g_sceAjmBatchJobRunSplit) {
        HMODULE h = GetModuleHandleA("libSceAjm.native.prx");
        if (!h) h = LoadLibraryA("libSceAjm.native.prx");
        if (h) g_sceAjmBatchJobRunSplit = (fn_sceAjmBatchJobRunSplit)GetProcAddress(h, "Z9NVCesiP0Q");
    }
    if (g_sceAjmBatchJobRunSplit) {
        return g_sceAjmBatchJobRunSplit(
            info,
            instance,
            (uint64_t)1 << 47,
            input_buffers,
            input_buffers_num,
            output_buffers,
            output_buffers_num,
            result,
            24
        );
    }
    return 0;
}

void install_ajm_hooks(void) {
    HMODULE hAjm = GetModuleHandleA("libSceAjm.native.prx");
    if (!hAjm) hAjm = LoadLibraryA("libSceAjm.native.prx");
    if (hAjm) {
        void* p_single = (void*)GetProcAddress(hAjm, "5LLWbpP5xi8");
        void* p_decode = (void*)GetProcAddress(hAjm, "39WxhR-ePew");
        void* p_split  = (void*)GetProcAddress(hAjm, "SJ3i0DXP8vg");
        if (p_single) install_hook(p_single, (void*)impl_sceAjmBatchJobDecodeSingle);
        if (p_decode) install_hook(p_decode, (void*)impl_sceAjmBatchJobDecode);
        if (p_split)  install_hook(p_split,  (void*)impl_sceAjmBatchJobDecodeSplit);
    }
}

int APS5_VABI hook_bink_audio_mix(void* src, uint32_t size) {
    char* base = (char*)GetModuleHandleA(NULL);
    uint32_t expected = *(uint32_t*)(base + 0x31ea490);
    void* mutex = (void*)(base + 0x31ea4a0);
    uint8_t* ring_buf = *(uint8_t**)(base + 0x31ea498);
    int32_t* p_write_off = (int32_t*)(base + 0x31ea4b0);
    uint8_t blocks = *(uint8_t*)(base + 0x31ea494);
    uint32_t max_size = (uint32_t)blocks * expected * 4;
    typedef int (APS5_VABI *fn_lock)(void*);
    typedef int (APS5_VABI *fn_unlock)(void*);
    fn_lock lock = (fn_lock)(base + 0x7c2b20);
    fn_unlock unlock = (fn_unlock)(base + 0x7c21e0);
    if (lock) lock(mutex);
    if (ring_buf && p_write_off && max_size > 0 && src && size > 0) {
        uint32_t to_copy = size;
        uint32_t cur_off = (uint32_t)*p_write_off;
        if (cur_off + to_copy > max_size) {
            uint32_t part1 = max_size - cur_off;
            memcpy(ring_buf + cur_off, src, part1);
            memcpy(ring_buf, (uint8_t*)src + part1, to_copy - part1);
            *p_write_off = (int32_t)(to_copy - part1);
        } else {
            memcpy(ring_buf + cur_off, src, to_copy);
            cur_off += to_copy;
            if (cur_off >= max_size) cur_off = 0;
            *p_write_off = (int32_t)cur_off;
        }
    }
    if (unlock) unlock(mutex);
    return 1;
}
void install_bink_hooks(void) {
    char* base = (char*)GetModuleHandleA(NULL);
    if (!base) return;
    void* p_mix = (void*)(base + 0x6efe30);
    install_hook(p_mix, (void*)hook_bink_audio_mix);
}

static DWORD WINAPI second_monitor_thread(LPVOID p) {
    (void)p;
    HWINSTA hw = OpenWindowStationA("WinSta0", FALSE, GENERIC_ALL);
    if (hw) SetProcessWindowStation(hw);
    HDESK hd = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (hd) SetThreadDesktop(hd);
    DWORD pid = GetCurrentProcessId();
    for (int i = 0; i < 300; i++) {
        Sleep(50);
        HWND hwnd = FindWindowA("SDL_app", NULL);
        if (!hwnd) hwnd = FindWindowA(NULL, "Call of Duty: Black Ops II");
        if (hwnd) {
            DWORD winPid = 0;
            GetWindowThreadProcessId(hwnd, &winPid);
            if (winPid == pid) {
                RECT rc;
                if (GetWindowRect(hwnd, &rc)) {
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;
                    int x = 2560 + (2560 - w) / 2;
                    int y = (1440 - h) / 2;
                    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
                }
                break;
            }
        }
    }
    return 0;
}
static int g_monitor_thread_started = 0;
static void start_monitor_thread(void) {
    if (!g_monitor_thread_started) {
        g_monitor_thread_started = 1;
        CreateThread(NULL, 0, second_monitor_thread, NULL, 0, NULL);
    }
}
static void ensure_audioout_loaded(void);
static void switch_to_default_desktop(void) {
    HWINSTA hw = OpenWindowStationA("WinSta0", FALSE, GENERIC_ALL);
    if (hw) SetProcessWindowStation(hw);
    HDESK hd = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (hd) SetThreadDesktop(hd);
}
__attribute__((constructor))
void on_init(void) {
    switch_to_default_desktop();
    install_kernel_hooks();
    install_ajm_hooks();
    install_bink_hooks();
    ensure_audioout_loaded();
    start_monitor_thread();
}
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        switch_to_default_desktop();
        install_kernel_hooks();
        install_ajm_hooks();
        install_bink_hooks();
        ensure_audioout_loaded();
        start_monitor_thread();
    }
    return TRUE;
}

typedef BOOL (WINAPI *pfn_wait_addr)(volatile VOID*, PVOID, SIZE_T, DWORD);
typedef VOID (WINAPI *pfn_wake_addr)(PVOID);
static pfn_wait_addr g_pfn_wait = NULL;
static pfn_wake_addr g_pfn_wake = NULL;
int APS5_VABI impl_sync_wait(void* addr, uint32_t exp, void* to, const char* name) {
    if (!addr) return -1;
    if (!g_pfn_wait) {
        HMODULE k = GetModuleHandleA("kernel32.dll");
        if (k) g_pfn_wait = (pfn_wait_addr)GetProcAddress(k, "WaitOnAddress");
    }
    if (g_pfn_wait) {
        uint32_t e = exp;
        DWORD ms = to ? 100 : INFINITE;
        g_pfn_wait(addr, &e, sizeof(uint32_t), ms);
    }
    return 0;
}
int APS5_VABI impl_sync_wake(void* addr, int count) {
    if (!addr) return -1;
    if (!g_pfn_wake) {
        HMODULE k = GetModuleHandleA("kernel32.dll");
        if (k) g_pfn_wake = (pfn_wake_addr)GetProcAddress(k, "WakeByAddressAll");
    }
    if (g_pfn_wake) g_pfn_wake(addr);
    return 0;
}
void APS5_VABI impl_json_clear(void* this_ptr) {
    if (this_ptr) memset(this_ptr, 0, 16);
}
int64_t APS5_VABI impl_xtime_ticks(void) {
    return (int64_t)(GetTickCount64() * 1000000ULL);
}
void APS5_VABI impl_thrd_sleep(void* xt) {
    Sleep(1);
}
void* APS5_VABI impl_tls_addr(void* ti) {
    return NULL;
}
typedef int (APS5_VABI *fn_audio_init)(void);
typedef int (APS5_VABI *fn_audio_open)(int, int, int, uint32_t, uint32_t, uint32_t);
typedef int (APS5_VABI *fn_audio_output)(int, const void*);
typedef int (APS5_VABI *fn_audio_outputs)(void*, uint32_t);
typedef int (APS5_VABI *fn_audio_close)(int);
typedef int (APS5_VABI *fn_audio_get_port_state)(int, void*);
static fn_audio_init g_real_audio_init = NULL;
static fn_audio_open g_real_audio_open = NULL;
static fn_audio_output g_real_audio_output = NULL;
static fn_audio_outputs g_real_audio_outputs = NULL;
static fn_audio_close g_real_audio_close = NULL;
static fn_audio_get_port_state g_real_audio_get_port_state = NULL;
static void ensure_audioout_loaded(void) {
    if (g_real_audio_open) return;
    SetDllDirectoryA("S:\\AnyPS5\\extracted\\libs");
    HMODULE h = GetModuleHandleA("libSceAudioOut.prx");
    if (!h) h = LoadLibraryA("S:\\AnyPS5\\extracted\\libs\\libSceAudioOut.prx");
    if (!h) h = LoadLibraryA("libs\\libSceAudioOut.prx");
    if (!h) h = LoadLibraryA("libSceAudioOut.prx");
    if (h) {
        g_real_audio_init = (fn_audio_init)GetProcAddress(h, "JfEPXVxhFqA");
        g_real_audio_open = (fn_audio_open)GetProcAddress(h, "ekNvsT22rsY");
        g_real_audio_output = (fn_audio_output)GetProcAddress(h, "QOQtbeDqsT4");
        g_real_audio_outputs = (fn_audio_outputs)GetProcAddress(h, "w3PdaSTSwGE");
        g_real_audio_close = (fn_audio_close)GetProcAddress(h, "s1--uE9mBFw");
        g_real_audio_get_port_state = (fn_audio_get_port_state)GetProcAddress(h, "GrQ9s4IrNaQ");
    }
}
int APS5_VABI impl_audio_init(void) {
    ensure_audioout_loaded();
    if (g_real_audio_init) return g_real_audio_init();
    return 0;
}
int APS5_VABI impl_audio_open(int u, int t, int i, uint32_t l, uint32_t f, uint32_t p) {
    ensure_audioout_loaded();
    // Game data is genuinely F32 (param 5); pass the format through unchanged.
    if (g_real_audio_open) return g_real_audio_open(u, t, i, l, f, p);
    return 0;
}
int APS5_VABI impl_audio_output(int h, const void* p) {
    ensure_audioout_loaded();
    if (g_real_audio_output) return g_real_audio_output(h, p);
    return 0;
}
int APS5_VABI impl_audio_outputs(void* param, uint32_t num) {
    ensure_audioout_loaded();
    if (g_real_audio_outputs) return g_real_audio_outputs(param, num);
    return 512;
}
int APS5_VABI impl_audio_close(int h) {
    ensure_audioout_loaded();
    if (g_real_audio_close) return g_real_audio_close(h);
    return 0;
}
int APS5_VABI impl_audio_get_port_state(int h, void* state) {
    ensure_audioout_loaded();
    if (g_real_audio_get_port_state) return g_real_audio_get_port_state(h, state);
    return 0;
}
void* APS5_VABI nid_stub_0(void) { return 0; }
void* APS5_VABI nid_stub_1(void) { return 0; }
void* APS5_VABI nid_stub_2(void) { return 0; }
void* APS5_VABI nid_stub_3(void) { return 0; }
void* APS5_VABI nid_stub_4(void) { return 0; }
void* APS5_VABI nid_stub_5(void) { return 0; }
void* APS5_VABI nid_stub_6(void) { return 0; }
void* APS5_VABI nid_stub_7(void) { return 0; }
void* APS5_VABI nid_stub_8(void) { return 0; }
void* APS5_VABI nid_stub_9(void) { return 0; }
void* APS5_VABI nid_stub_10(void) { return 0; }
void* APS5_VABI nid_stub_11(void) { return 0; }
void* APS5_VABI nid_stub_12(void) { return 0; }
void* APS5_VABI nid_stub_13(void) { return 0; }
void* APS5_VABI nid_stub_14(void) { return 0; }
void* APS5_VABI nid_stub_15(void) { return 0; }
void* APS5_VABI nid_stub_16(void) { return 0; }
void* APS5_VABI nid_stub_17(void) { return 0; }
void* APS5_VABI nid_stub_18(void) { return 0; }
void* APS5_VABI nid_stub_19(void) { return 0; }
void* APS5_VABI nid_stub_20(void) { return 0; }
void* APS5_VABI nid_stub_21(void) { return 0; }
void* APS5_VABI nid_stub_22(void) { return 0; }
void* APS5_VABI nid_stub_23(void) { return 0; }
void* APS5_VABI nid_stub_24(void) { return 0; }
void* APS5_VABI nid_stub_25(void) { return 0; }
void* APS5_VABI nid_stub_26(void) { return 0; }
void* APS5_VABI nid_stub_27(void) { return 0; }
void* APS5_VABI nid_stub_28(void) { return 0; }
void* APS5_VABI nid_stub_29(void) { return 0; }
void* APS5_VABI nid_stub_30(void) { return 0; }
void* APS5_VABI nid_stub_31(void) { return 0; }
void* APS5_VABI nid_stub_32(void) { return 0; }
void* APS5_VABI nid_stub_33(void) { return 0; }
void* APS5_VABI nid_stub_34(void) { return 0; }
void* APS5_VABI nid_stub_35(void) { return 0; }
void* APS5_VABI nid_stub_36(void) { return 0; }
void* APS5_VABI nid_stub_37(void) { return 0; }
int32_t APS5_VABI nid_stub_38(void** a, size_t l, int p, int f, const char* n) { return impl_map_flex_internal(a, l, p, f, n); }
void* APS5_VABI nid_stub_39(void) { return 0; }
void* APS5_VABI nid_stub_40(void) { return 0; }
void* APS5_VABI nid_stub_41(void) { return 0; }
void* APS5_VABI nid_stub_42(void) { return 0; }
void* APS5_VABI nid_stub_43(void) { return 0; }
void* APS5_VABI nid_stub_44(void) { return 0; }
void* APS5_VABI nid_stub_45(void) { return 0; }
void* APS5_VABI nid_stub_46(void) { return 0; }
void* APS5_VABI nid_stub_47(void) { return 0; }
void* APS5_VABI nid_stub_48(void) { return 0; }
void* APS5_VABI nid_stub_49(void) { return 0; }
void* APS5_VABI nid_stub_50(void) { return 0; }
void* APS5_VABI nid_stub_51(void) { return 0; }
void* APS5_VABI nid_stub_52(void) { return 0; }
void* APS5_VABI nid_stub_53(void) { return 0; }
void* APS5_VABI nid_stub_54(void) { return 0; }
void* APS5_VABI nid_stub_55(void) { return 0; }
void* APS5_VABI nid_stub_56(void) { return 0; }
void* APS5_VABI nid_stub_57(void) { return 0; }
void* APS5_VABI nid_stub_58(void) { return 0; }
void* APS5_VABI nid_stub_59(void) { return 0; }
void* APS5_VABI nid_stub_60(void) { return 0; }
void* APS5_VABI nid_stub_61(void) { return 0; }
void* APS5_VABI nid_stub_62(void) { return 0; }
void* APS5_VABI nid_stub_63(void) { return 0; }
void* APS5_VABI nid_stub_64(void) { return 0; }
void* APS5_VABI nid_stub_65(void) { return 0; }
void* APS5_VABI nid_stub_66(void) { return 0; }
void* APS5_VABI nid_stub_67(void) { return 0; }
void* APS5_VABI nid_stub_68(void) { return 0; }
void* APS5_VABI nid_stub_69(void) { return 0; }
int APS5_VABI nid_stub_70(void) { return 0; }
void* APS5_VABI nid_stub_71(void) { return 0; }
void* APS5_VABI nid_stub_72(void) { return 0; }
void* APS5_VABI nid_stub_73(void) { return 0; }
void* APS5_VABI nid_stub_74(void) { return 0; }
void* APS5_VABI nid_stub_75(void) { return 0; }
void* APS5_VABI nid_stub_76(void) { return 0; }
int APS5_VABI nid_stub_77(void* a, uint32_t e, void* t, const char* n) { return impl_sync_wait(a, e, t, n); }
void* APS5_VABI nid_stub_78(void) { return 0; }
void* APS5_VABI nid_stub_79(void) { return 0; }
void* APS5_VABI nid_stub_80(void) { return 0; }
void* APS5_VABI nid_stub_81(void) { return 0; }
void* APS5_VABI nid_stub_82(void) { return 0; }
void* APS5_VABI nid_stub_83(void) { return 0; }
void* APS5_VABI nid_stub_84(void) { return 0; }
void* APS5_VABI nid_stub_85(void) { return 0; }
void* APS5_VABI nid_stub_86(void) { return 0; }
int64_t APS5_VABI nid_stub_87(void) { return impl_xtime_ticks(); }
void* APS5_VABI nid_stub_88(void) { return 0; }
void* APS5_VABI nid_stub_89(void) { return 0; }
void* APS5_VABI nid_stub_90(void) { return 0; }
void* APS5_VABI nid_stub_91(void) { return 0; }
void* APS5_VABI nid_stub_92(void) { return 0; }
void* APS5_VABI nid_stub_93(void) { return 0; }
void* APS5_VABI nid_stub_94(void) { return 0; }
void* APS5_VABI nid_stub_95(void) { return 0; }
void* APS5_VABI nid_stub_96(void) { return 0; }
void* APS5_VABI nid_stub_97(void) { return 0; }
void* APS5_VABI nid_stub_98(void) { return 0; }
void* APS5_VABI nid_stub_99(void) { return 0; }
void* APS5_VABI nid_stub_100(void) { return 0; }
void* APS5_VABI nid_stub_101(void) { return 0; }
void* APS5_VABI nid_stub_102(void) { return 0; }
void* APS5_VABI nid_stub_103(void) { return 0; }
void* APS5_VABI nid_stub_104(void) { return 0; }
void APS5_VABI nid_stub_105(void* p) { impl_json_clear(p); }
int APS5_VABI nid_stub_106(void) { return 0; }
void* APS5_VABI nid_stub_107(void) { return 0; }
void* APS5_VABI nid_stub_108(void) { return 0; }
void* APS5_VABI nid_stub_109(void) { return 0; }
void* APS5_VABI nid_stub_110(void) { return 0; }
void* APS5_VABI nid_stub_111(void) { return 0; }
void* APS5_VABI nid_stub_112(void) { return 0; }
void* APS5_VABI nid_stub_113(void) { return 0; }
void* APS5_VABI nid_stub_114(void) { return 0; }
void* APS5_VABI nid_stub_115(void) { return 0; }
void* APS5_VABI nid_stub_116(void) { return 0; }
void* APS5_VABI nid_stub_117(void) { return 0; }
void* APS5_VABI nid_stub_118(void) { return 0; }
void* APS5_VABI nid_stub_119(void) { return 0; }
void* APS5_VABI nid_stub_120(void) { return 0; }
void* APS5_VABI nid_stub_121(void) { return 0; }
void* APS5_VABI nid_stub_122(void) { return 0; }
void* APS5_VABI nid_stub_123(void) { return 0; }
void* APS5_VABI nid_stub_124(void) { return 0; }
void* APS5_VABI nid_stub_125(void) { return 0; }
int APS5_VABI nid_stub_126(void* a, uint32_t e, void* t, const char* n) { return impl_sync_wait(a, e, t, n); }
void* APS5_VABI nid_stub_127(void) { return 0; }
void* APS5_VABI nid_stub_128(void) { return 0; }
void* APS5_VABI nid_stub_129(void) { return 0; }
void* APS5_VABI nid_stub_130(void) { return 0; }
void* APS5_VABI nid_stub_131(void) { return 0; }
void* APS5_VABI nid_stub_132(void) { return 0; }
void* APS5_VABI nid_stub_133(void) { return 0; }
int APS5_VABI nid_stub_134(const void* p) { return impl_sceNpCommerceDialogOpen2(p); }
void* APS5_VABI nid_stub_135(void) { return 0; }
void* APS5_VABI nid_stub_136(void) { return 0; }
void* APS5_VABI nid_stub_137(void) { return 0; }
void* APS5_VABI nid_stub_138(void) { return 0; }
void* APS5_VABI nid_stub_139(void) { return 0; }
void* APS5_VABI nid_stub_140(void) { return 0; }
void* APS5_VABI nid_stub_141(void) { return 0; }
void* APS5_VABI nid_stub_142(void) { return 0; }
void* APS5_VABI nid_stub_143(void) { return 0; }
void* APS5_VABI nid_stub_144(void) { return 0; }
void* APS5_VABI nid_stub_145(void) { return 0; }
void* APS5_VABI nid_stub_146(void) { return 0; }
void* APS5_VABI nid_stub_147(void) { return 0; }
void* APS5_VABI nid_stub_148(void) { return 0; }
void* APS5_VABI nid_stub_149(void) { return 0; }
void* APS5_VABI nid_stub_150(void) { return 0; }
void* APS5_VABI nid_stub_151(void) { return 0; }
void* APS5_VABI nid_stub_152(void) { return 0; }
void* APS5_VABI nid_stub_153(void) { return 0; }
void* APS5_VABI nid_stub_154(void) { return 0; }
void* APS5_VABI nid_stub_155(void) { return 0; }
void* APS5_VABI nid_stub_156(void) { return 0; }
void* APS5_VABI nid_stub_157(void) { return 0; }
void* APS5_VABI nid_stub_158(void) { return 0; }
void* APS5_VABI nid_stub_159(void) { return 0; }
void* APS5_VABI nid_stub_160(void) { return 0; }
void* APS5_VABI nid_stub_161(void) { return 0; }
void* APS5_VABI nid_stub_162(void) { return 0; }
void* APS5_VABI nid_stub_163(void) { return 0; }
void* APS5_VABI nid_stub_164(void) { return 0; }
void* APS5_VABI nid_stub_165(void) { return 0; }
void* APS5_VABI nid_stub_166(void) { return 0; }
void* APS5_VABI nid_stub_167(void) { return 0; }
void* APS5_VABI nid_stub_168(void) { return 0; }
void* APS5_VABI nid_stub_169(void) { return 0; }
void* APS5_VABI nid_stub_170(void) { return 0; }
void* APS5_VABI nid_stub_171(void) { return 0; }
void* APS5_VABI nid_stub_172(void) { return 0; }
void* APS5_VABI nid_stub_173(void) { return 0; }
int APS5_VABI nid_stub_174(const void* p) { return impl_web_dialog_open(p); }
void* APS5_VABI nid_stub_175(void) { return 0; }
void* APS5_VABI nid_stub_176(void) { return 0; }
void* APS5_VABI nid_stub_177(void) { return 0; }
void* APS5_VABI nid_stub_178(void) { return 0; }
void* APS5_VABI nid_stub_179(void) { return 0; }
int APS5_VABI nid_stub_180(void* a, uint32_t e, void* t, const char* n) { return impl_sync_wait(a, e, t, n); }
void* APS5_VABI nid_stub_181(void) { return 0; }
void* APS5_VABI nid_stub_182(void) { return 0; }
void* APS5_VABI nid_stub_183(void) { return 0; }
void* APS5_VABI nid_stub_184(void) { return 0; }
void* APS5_VABI nid_stub_185(void) { return 0; }
void* APS5_VABI nid_stub_186(void) { return 0; }
void* APS5_VABI nid_stub_187(void) { return 0; }
void* APS5_VABI nid_stub_188(void) { return 0; }
void* APS5_VABI nid_stub_189(void) { return 0; }
void* APS5_VABI nid_stub_190(void) { return 0; }
void* APS5_VABI nid_stub_191(void) { return 0; }
void* APS5_VABI nid_stub_192(void) { return 0; }
void* APS5_VABI nid_stub_193(void) { return 0; }
void* APS5_VABI nid_stub_194(void) { return 0; }
void* APS5_VABI nid_stub_195(void) { return 0; }
void* APS5_VABI nid_stub_196(void) { return 0; }
void* APS5_VABI nid_stub_197(void) { return 0; }
void* APS5_VABI nid_stub_198(void) { return 0; }
void* APS5_VABI nid_stub_199(void) { return 0; }
void* APS5_VABI nid_stub_200(void) { return 0; }
void* APS5_VABI nid_stub_201(void) { return 0; }
void* APS5_VABI nid_stub_202(void) { return 0; }
void* APS5_VABI nid_stub_203(void) { return 0; }
void* APS5_VABI nid_stub_204(void) { return 0; }
void* APS5_VABI nid_stub_205(void) { return 0; }
void* APS5_VABI nid_stub_206(void) { return 0; }
void* APS5_VABI nid_stub_207(void) { return 0; }
void* APS5_VABI nid_stub_208(void) { return 0; }
void* APS5_VABI nid_stub_209(void) { return 0; }
void* APS5_VABI nid_stub_210(void) { return 0; }
int APS5_VABI nid_stub_211(uint64_t a, int32_t* u) { return impl_sceNpGetUserIdByAccountId(a, u); }
void* APS5_VABI nid_stub_212(void) { return 0; }
void* APS5_VABI nid_stub_213(void) { return 0; }
void* APS5_VABI nid_stub_214(void) { return 0; }
void* APS5_VABI nid_stub_215(void) { return 0; }
void* APS5_VABI nid_stub_216(void) { return 0; }
void* APS5_VABI nid_stub_217(void) { return 0; }
void* APS5_VABI nid_stub_218(void) { return 0; }
void* APS5_VABI nid_stub_219(void) { return 0; }
void* APS5_VABI nid_stub_220(void) { return 0; }
void* APS5_VABI nid_stub_221(void) { return 0; }
void* APS5_VABI nid_stub_222(void) { return 0; }
void* APS5_VABI nid_stub_223(void) { return 0; }
void* APS5_VABI nid_stub_224(void) { return 0; }
void* APS5_VABI nid_stub_225(void) { return 0; }
void* APS5_VABI nid_stub_226(void) { return 0; }
void* APS5_VABI nid_stub_227(void) { return 0; }
void* APS5_VABI nid_stub_228(void) { return 0; }
void* APS5_VABI nid_stub_229(void) { return 0; }
void* APS5_VABI nid_stub_230(void) { return 0; }
void* APS5_VABI nid_stub_231(void) { return 0; }
void* APS5_VABI nid_stub_232(void) { return 0; }
void* APS5_VABI nid_stub_233(void) { return 0; }
void* APS5_VABI nid_stub_234(void) { return 0; }
void* APS5_VABI nid_stub_235(void) { return 0; }
void* APS5_VABI nid_stub_236(void) { return 0; }
void* APS5_VABI nid_stub_237(void) { return 0; }
void* APS5_VABI nid_stub_238(void) { return 0; }
void* APS5_VABI nid_stub_239(void) { return 0; }
void* APS5_VABI nid_stub_240(void) { return 0; }
void* APS5_VABI nid_stub_241(void) { return 0; }
void* APS5_VABI nid_stub_242(void) { return 0; }
void* APS5_VABI nid_stub_243(void) { return 0; }
void* APS5_VABI nid_stub_244(void) { return 0; }
void* APS5_VABI nid_stub_245(void) { return 0; }
void* APS5_VABI nid_stub_246(void) { return 0; }
void* APS5_VABI nid_stub_247(void) { return 0; }
void* APS5_VABI nid_stub_248(void) { return 0; }
void* APS5_VABI nid_stub_249(void) { return 0; }
void* APS5_VABI nid_stub_250(void) { return 0; }
void* APS5_VABI nid_stub_251(void) { return 0; }
void* APS5_VABI nid_stub_252(void) { return 0; }
void* APS5_VABI nid_stub_253(void) { return 0; }
void* APS5_VABI nid_stub_254(void) { return 0; }
void* APS5_VABI nid_stub_255(void) { return 0; }
void* APS5_VABI nid_stub_256(void) { return 0; }
void* APS5_VABI nid_stub_257(void) { return 0; }
void* APS5_VABI nid_stub_258(void) { return 0; }
void* APS5_VABI nid_stub_259(void) { return 0; }
void* APS5_VABI nid_stub_260(void) { return 0; }
void* APS5_VABI nid_stub_261(void) { return 0; }
void* APS5_VABI nid_stub_262(void) { return 0; }
void* APS5_VABI nid_stub_263(void) { return 0; }
void* APS5_VABI nid_stub_264(void) { return 0; }
void* APS5_VABI nid_stub_265(void) { return 0; }
void* APS5_VABI nid_stub_266(void) { return 0; }
void* APS5_VABI nid_stub_267(void) { return 0; }
void* APS5_VABI nid_stub_268(void) { return 0; }
void* APS5_VABI nid_stub_269(void) { return 0; }
void* APS5_VABI nid_stub_270(void) { return 0; }
void* APS5_VABI nid_stub_271(void) { return 0; }
void* APS5_VABI nid_stub_272(void) { return 0; }
void* APS5_VABI nid_stub_273(void) { return 0; }
void* APS5_VABI nid_stub_274(void) { return 0; }
void* APS5_VABI nid_stub_275(void) { return 0; }
void* APS5_VABI nid_stub_276(void) { return 0; }
void* APS5_VABI nid_stub_277(void) { return 0; }
void* APS5_VABI nid_stub_278(void) { return 0; }
void* APS5_VABI nid_stub_279(void) { return 0; }
void* APS5_VABI nid_stub_280(void) { return 0; }
void* APS5_VABI nid_stub_281(void) { return 0; }
void* APS5_VABI nid_stub_282(void) { return 0; }
void* APS5_VABI nid_stub_283(void) { return 0; }
int APS5_VABI nid_stub_284(void) { return impl_web_dialog_status(); }
void* APS5_VABI nid_stub_285(void) { return 0; }
void* APS5_VABI nid_stub_286(void) { return 0; }
void* APS5_VABI nid_stub_287(void) { return 0; }
void* APS5_VABI nid_stub_288(void) { return 0; }
void* APS5_VABI nid_stub_289(void) { return 0; }
void* APS5_VABI nid_stub_290(void) { return 0; }
void APS5_VABI nid_stub_291(void* xt) { impl_thrd_sleep(xt); }
void* APS5_VABI nid_stub_292(void) { return 0; }
void* APS5_VABI nid_stub_293(void) { return 0; }
void* APS5_VABI nid_stub_294(void) { return 0; }
void* APS5_VABI nid_stub_295(void) { return 0; }
void* APS5_VABI nid_stub_296(void) { return 0; }
void* APS5_VABI nid_stub_297(void) { return 0; }
void* APS5_VABI nid_stub_298(void) { return 0; }
void* APS5_VABI nid_stub_299(void) { return 0; }
void* APS5_VABI nid_stub_300(void) { return 0; }
void* APS5_VABI nid_stub_301(void) { return 0; }
int APS5_VABI nid_stub_302(void) { return 0; }
void* APS5_VABI nid_stub_303(void) { return 0; }
void* APS5_VABI nid_stub_304(void) { return 0; }
void* APS5_VABI nid_stub_305(void) { return 0; }
void* APS5_VABI nid_stub_306(void) { return 0; }
void* APS5_VABI nid_stub_307(void) { return 0; }
void* APS5_VABI nid_stub_308(void) { return 0; }
void* APS5_VABI nid_stub_309(void) { return 0; }
void* APS5_VABI nid_stub_310(void) { return 0; }
void* APS5_VABI nid_stub_311(void) { return 0; }
void* APS5_VABI nid_stub_312(void) { return 0; }
void* APS5_VABI nid_stub_313(void) { return 0; }
void* APS5_VABI nid_stub_314(void) { return 0; }
void* APS5_VABI nid_stub_315(void) { return 0; }
void* APS5_VABI nid_stub_316(void) { return 0; }
void* APS5_VABI nid_stub_317(void) { return 0; }
void* APS5_VABI nid_stub_318(void) { return 0; }
void* APS5_VABI nid_stub_319(void) { return 0; }
void* APS5_VABI nid_stub_320(void) { return 0; }
void* APS5_VABI nid_stub_321(void) { return 0; }
void* APS5_VABI nid_stub_322(void) { return 0; }
void* APS5_VABI nid_stub_323(void) { return 0; }
void* APS5_VABI nid_stub_324(void) { return 0; }
void* APS5_VABI nid_stub_325(void) { return 0; }
void* APS5_VABI nid_stub_326(void) { return 0; }
int APS5_VABI nid_stub_327(void* a, int c) { return impl_sync_wake(a, c); }
void* APS5_VABI nid_stub_328(void) { return 0; }
void* APS5_VABI nid_stub_329(void) { return 0; }
void* APS5_VABI nid_stub_330(void) { return 0; }
void* APS5_VABI nid_stub_331(void) { return 0; }
void* APS5_VABI nid_stub_332(void) { return 0; }
void* APS5_VABI nid_stub_333(void) { return 0; }
void* APS5_VABI nid_stub_334(void) { return 0; }
void* APS5_VABI nid_stub_335(void) { return 0; }
void* APS5_VABI nid_stub_336(void) { return 0; }
void* APS5_VABI nid_stub_337(void) { return 0; }
void* APS5_VABI nid_stub_338(void) { return 0; }
void* APS5_VABI nid_stub_339(void) { return 0; }
void* APS5_VABI nid_stub_340(void) { return 0; }
void* APS5_VABI nid_stub_341(void) { return 0; }
void* APS5_VABI nid_stub_342(void) { return 0; }
void* APS5_VABI nid_stub_343(void) { return 0; }
void* APS5_VABI nid_stub_344(void) { return 0; }
void* APS5_VABI nid_stub_345(void) { return 0; }
void* APS5_VABI nid_stub_346(void) { return 0; }
void* APS5_VABI nid_stub_347(void) { return 0; }
void* APS5_VABI nid_stub_348(void) { return 0; }
void* APS5_VABI nid_stub_349(void) { return 0; }
void* APS5_VABI nid_stub_350(void) { return 0; }
void* APS5_VABI nid_stub_351(void) { return 0; }
void* APS5_VABI nid_stub_352(void) { return 0; }
void* APS5_VABI nid_stub_353(void) { return 0; }
int APS5_VABI nid_stub_354(void) { return 0; }
void* APS5_VABI nid_stub_355(void) { return 0; }
void* APS5_VABI nid_stub_356(void) { return 0; }
void* APS5_VABI nid_stub_357(void) { return 0; }
void* APS5_VABI nid_stub_358(void) { return 0; }
void* APS5_VABI nid_stub_359(void* ti) { return impl_tls_addr(ti); }
void* APS5_VABI nid_stub_360(void) { return 0; }
void* APS5_VABI nid_stub_361(void) { return 0; }
void* APS5_VABI nid_stub_362(void) { return 0; }
void* APS5_VABI nid_stub_363(void) { return 0; }
void* APS5_VABI nid_stub_364(void) { return 0; }
void* APS5_VABI nid_stub_365(void) { return 0; }
void* APS5_VABI nid_stub_366(void) { return 0; }
void* APS5_VABI nid_stub_367(void) { return 0; }
void* APS5_VABI nid_stub_368(void) { return 0; }
void* APS5_VABI nid_stub_369(void) { return 0; }
void* APS5_VABI nid_stub_370(void) { return 0; }
void* APS5_VABI nid_stub_371(void) { return 0; }
void* APS5_VABI nid_stub_372(void) { return 0; }
void* APS5_VABI nid_stub_373(void) { return 0; }
void* APS5_VABI nid_stub_374(void) { return 0; }
void* APS5_VABI nid_stub_375(void) { return 0; }
void* APS5_VABI nid_stub_376(void) { return 0; }
void* APS5_VABI nid_stub_377(void) { return 0; }
void* APS5_VABI nid_stub_378(void) { return 0; }
int APS5_VABI nid_stub_379(void) { return impl_audio_init(); }
int APS5_VABI nid_stub_380(int u, int t, int i, uint32_t l, uint32_t f, uint32_t p) { return impl_audio_open(u, t, i, l, f, p); }
int APS5_VABI nid_stub_381(int h, const void* p) { return impl_audio_output(h, p); }
int APS5_VABI nid_stub_382(int h) { return impl_audio_close(h); }
int APS5_VABI nid_stub_383(void* param, uint32_t num) { return impl_audio_outputs(param, num); }
int APS5_VABI nid_stub_384(int h, void* state) { return impl_audio_get_port_state(h, state); }