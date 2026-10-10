/*
 * Budokai3Access loader. This DLL is named version.dll and sits next to dbz3.exe.
 * Windows loads it instead of the system copy; it passes the version
 * functions through to the real version.dll and loads the mod itself,
 * Budokai3Access\Budokai3Access.dll.
 *
 * For development, if Budokai3Access\devreload.txt exists, the loader watches
 * Budokai3Access.dll and swaps in a rebuilt copy while the game keeps running.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#define VERSION_EXPORTS(X) X(GetFileVersionInfoA) X(GetFileVersionInfoByHandle) X(GetFileVersionInfoExA) \
    X(GetFileVersionInfoExW) X(GetFileVersionInfoSizeA) X(GetFileVersionInfoSizeExA) X(GetFileVersionInfoSizeExW) \
    X(GetFileVersionInfoSizeW) X(GetFileVersionInfoW) X(VerFindFileA) X(VerFindFileW) X(VerInstallFileA) \
    X(VerInstallFileW) X(VerLanguageNameA) X(VerLanguageNameW) X(VerQueryValueA) X(VerQueryValueW)

#define DECL_PTR(n) void *p_##n;
VERSION_EXPORTS(DECL_PTR)
/* Each export jumps through the pointer to the real function. */
#define STUB(n) __asm__(".globl " #n "\n" #n ":\n\tjmp *p_" #n "(%rip)\n");
VERSION_EXPORTS(STUB)

static void load_real_version(void) {
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\version.dll");
    HMODULE h = LoadLibraryW(path);
#define RESOLVE(n) p_##n = h ? (void *)GetProcAddress(h, #n) : NULL;
    VERSION_EXPORTS(RESOLVE)
}

typedef void (*core_fn)(void);
static wchar_t g_dir[MAX_PATH];

static FILETIME mtime(const wchar_t *p) {
    WIN32_FILE_ATTRIBUTE_DATA a; FILETIME z = { 0, 0 };
    return GetFileAttributesExW(p, GetFileExInfoStandard, &a) ? a.ftLastWriteTime : z;
}

static HMODULE load_core(int dev, unsigned n) {
    wchar_t src[MAX_PATH], run[MAX_PATH];
    swprintf(src, MAX_PATH, L"%lsBudokai3Access.dll", g_dir);
    if (!dev) return LoadLibraryW(src);
    /* Load a copy so the build can overwrite Budokai3Access.dll. */
    swprintf(run, MAX_PATH, L"%lsBudokai3Access_run%u.dll", g_dir, n);
    if (!CopyFileW(src, run, FALSE)) return NULL;
    return LoadLibraryW(run);
}

static DWORD WINAPI loader_thread(LPVOID unused) {
    (void)unused;
    wchar_t src[MAX_PATH], flag[MAX_PATH], old[MAX_PATH];
    swprintf(src, MAX_PATH, L"%lsBudokai3Access.dll", g_dir);
    swprintf(flag, MAX_PATH, L"%lsdevreload.txt", g_dir);
    int dev = GetFileAttributesW(flag) != INVALID_FILE_ATTRIBUTES;
    wchar_t logp[MAX_PATH];
    swprintf(logp, MAX_PATH, L"%lsBudokai3Access.log", g_dir);
    DeleteFileW(logp);
    for (unsigned i = 0; i < 64; i++) { swprintf(old, MAX_PATH, L"%lsBudokai3Access_run%u.dll", g_dir, i); DeleteFileW(old); }
    unsigned n = 0;
    HMODULE core = load_core(dev, n);
    if (!core) return 0;
    core_fn start = (core_fn)GetProcAddress(core, "core_start");
    if (start) start();
    if (!dev) return 0;
    FILETIME t = mtime(src);
    for (;;) {
        Sleep(1000);
        FILETIME t2 = mtime(src);
        if (CompareFileTime(&t, &t2) == 0) continue;
        Sleep(500);                       /* let the build finish writing */
        t = mtime(src);
        core_fn stop = (core_fn)GetProcAddress(core, "core_stop");
        if (stop) stop();
        FreeLibrary(core);
        n = (n + 1) % 64;
        core = load_core(dev, n);
        if (!core) continue;
        start = (core_fn)GetProcAddress(core, "core_start");
        if (start) start();
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        load_real_version();
        GetModuleFileNameW(NULL, g_dir, MAX_PATH);
        wchar_t *slash = wcsrchr(g_dir, L'\\');
        if (slash) slash[1] = 0;
        wcscat(g_dir, L"Budokai3Access\\");
        CloseHandle(CreateThread(NULL, 0, loader_thread, NULL, 0, NULL));
    }
    return TRUE;
}
