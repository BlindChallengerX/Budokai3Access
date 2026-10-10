/*
 * "Play DBZ3 Access.exe": checks the mod is installed, repairs it, starts the
 * game.
 *
 * The mod's loader replaces the port's graphics plugin rexgpu-xenos.dll (the
 * port's own file is kept as rexgpu-xenos-original.dll). A port update, or
 * the launcher's "Repair install", puts the port's file back and the mod
 * would stop loading. Every time this program runs it:
 *   1. looks at rexgpu-xenos.dll: if it is not ours (no "DBZ 3 Access"
 *      product name), it is a new port file, so it becomes the original;
 *   2. puts the current loader (DBZ3Access\loader\rexgpu-xenos.dll) in place
 *      if it is missing or different;
 *   3. makes sure dbz3_user.toml has "audio_maxqframes = 3" (see below);
 *   4. starts dbz3.exe, unless run with --repair (used by the build scripts).
 * It only copies and renames files inside the game folder. What it did is
 * written to DBZ3Access\launch.log.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

static wchar_t g_dir[MAX_PATH];
static FILE *g_log;

static void logw(const wchar_t *fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fwprintf(g_log, L"%04d-%02d-%02d %02d:%02d:%02d ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    va_list ap; va_start(ap, fmt); vfwprintf(g_log, fmt, ap); va_end(ap);
    fputwc(L'\n', g_log); fflush(g_log);
}

static void fail(const wchar_t *msg) {
    logw(L"error: %ls", msg);
    MessageBoxW(NULL, msg, L"DBZ 3 Access", MB_OK | MB_ICONWARNING);
}

static int exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

/* Is this our loader? Our files carry the product name "DBZ 3 Access". */
static int is_ours(const wchar_t *path) {
    DWORD h = 0, n = GetFileVersionInfoSizeW(path, &h);
    if (!n) return 0;
    void *buf = HeapAlloc(GetProcessHeap(), 0, n);
    wchar_t *name = NULL; UINT len = 0; int ours = 0;
    if (buf && GetFileVersionInfoW(path, 0, n, buf) &&
        VerQueryValueW(buf, L"\\StringFileInfo\\040904B0\\ProductName", (void **)&name, &len) && name)
        ours = wcscmp(name, L"DBZ 3 Access") == 0;
    if (buf) HeapFree(GetProcessHeap(), 0, buf);
    return ours;
}

static int same_file(const wchar_t *a, const wchar_t *b) {
    HANDLE fa = CreateFileW(a, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE fb = CreateFileW(b, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    int same = 0;
    if (fa != INVALID_HANDLE_VALUE && fb != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sa, sb;
        if (GetFileSizeEx(fa, &sa) && GetFileSizeEx(fb, &sb) && sa.QuadPart == sb.QuadPart && sa.QuadPart < (64 << 20)) {
            static char ba[1 << 16], bb[1 << 16];
            DWORD ra, rb;
            same = 1;
            while (same && ReadFile(fa, ba, sizeof ba, &ra, NULL) && ReadFile(fb, bb, sizeof bb, &rb, NULL) && ra) {
                if (ra != rb || memcmp(ba, bb, ra)) same = 0;
            }
        }
    }
    if (fa != INVALID_HANDLE_VALUE) CloseHandle(fa);
    if (fb != INVALID_HANDLE_VALUE) CloseHandle(fb);
    return same;
}

/* Returns 1 when the mod's loader is in place. */
static int repair(void) {
    wchar_t plugin[MAX_PATH], orig[MAX_PATH], ours[MAX_PATH];
    swprintf(plugin, MAX_PATH, L"%lsrexgpu-xenos.dll", g_dir);
    swprintf(orig, MAX_PATH, L"%lsrexgpu-xenos-original.dll", g_dir);
    swprintf(ours, MAX_PATH, L"%lsDBZ3Access\\loader\\rexgpu-xenos.dll", g_dir);
    if (!exists(ours)) { fail(L"The DBZ 3 Access loader is missing (DBZ3Access\\loader\\rexgpu-xenos.dll). Please reinstall the mod."); return 0; }
    if (exists(plugin) && !is_ours(plugin)) {
        /* The port's own graphics plugin (new install, port update or Repair install). */
        if (!MoveFileExW(plugin, orig, MOVEFILE_REPLACE_EXISTING)) {
            fail(L"Could not set up DBZ 3 Access: the game's graphics plugin is in use. Close the game and try again.");
            return 0;
        }
        logw(L"kept the port's graphics plugin as rexgpu-xenos-original.dll");
    }
    if (!exists(orig)) {
        fail(L"The game's graphics plugin is missing (rexgpu-xenos-original.dll). Use Repair install in the game's launcher, then start DBZ 3 Access again.");
        return 0;
    }
    if (!exists(plugin) || !same_file(plugin, ours)) {
        if (!CopyFileW(ours, plugin, FALSE)) {
            if (exists(plugin)) { logw(L"loader in use; keeping the installed one"); return 1; }
            fail(L"Could not install the DBZ 3 Access loader in the game folder.");
            return 0;
        }
        logw(L"installed the DBZ 3 Access loader");
    }
    return 1;
}

/* Sound delay: the port queues audio_maxqframes sound frames of 5.3 ms
 * (default 8). Measured key press to menu click: 130 ms with 8, 105 ms with
 * 3, no dropouts in menus or fights. The port rewrites dbz3_user.toml when
 * its settings change, so the line is added again if it went missing; a
 * value the player set by hand is left alone. */
static void audio_queue_setting(void) {
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%lsdbz3_user.toml", g_dir);
    FILE *f = _wfopen(path, L"rb");
    if (f) {
        char buf[16384]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
        if (strstr(buf, "audio_maxqframes")) return;
        f = _wfopen(path, L"ab");
        if (!f) return;
        if (n && buf[n - 1] != '\n') fputs("\n", f);
    } else {
        f = _wfopen(path, L"wb");
        if (!f) return;
    }
    fputs("audio_maxqframes = 3\n", f);
    fclose(f);
    logw(L"set audio_maxqframes = 3 in dbz3_user.toml (less sound delay)");
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show) {
    (void)inst; (void)prev; (void)show;
    GetModuleFileNameW(NULL, g_dir, MAX_PATH);
    wchar_t *slash = wcsrchr(g_dir, L'\\');
    if (slash) slash[1] = 0;
    wchar_t logp[MAX_PATH];
    swprintf(logp, MAX_PATH, L"%lsDBZ3Access\\launch.log", g_dir);
    g_log = _wfopen(logp, L"a, ccs=UTF-8");
    int repair_only = cmd && wcsstr(cmd, L"--repair") != NULL;
    wchar_t exe[MAX_PATH];
    swprintf(exe, MAX_PATH, L"%lsdbz3.exe", g_dir);
    if (!exists(exe)) { fail(L"Put Play DBZ3 Access.exe in the game folder, next to dbz3.exe."); return 1; }
    int ok = repair();
    audio_queue_setting();
    if (repair_only) return ok ? 0 : 1;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    if (!CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, g_dir, &si, &pi)) {
        fail(L"Could not start dbz3.exe.");
        return 1;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    logw(L"started the game%ls", ok ? L"" : L" (without the mod)");
    if (g_log) fclose(g_log);
    return 0;
}
