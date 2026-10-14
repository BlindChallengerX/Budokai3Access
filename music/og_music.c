/* Budokai Original Music: puts the PS2 (USA) soundtrack back into the
   DBZ Budokai 1 and Budokai 3 HD ports, built from the player's own PS2 disc.
   The game is the one next to the program (dbz1.exe or dbz3.exe).

   Budokai 1 (port 0.5.1): mods/og_music/us/adx_us.afs from the HD
   assets/us/adx_us.afs with the 116 entries that differ from the PS2
   USR/ADX_US.AFS taken from the disc (same 1541 entries in the same order;
   all are 48 kHz stereo music, and entry 112 a PS2 track the HD game cut
   to a 5 second stub). Budokai 1 has no opening movie.

   Budokai 3:

   Writes <game>/mods/og_music/ (us/adx_usa.afs, us/opening.sfd, manifest.txt),
   the port's own mod format. The HD sound archive is copied and only the
   music entries are swapped for the PS2 ones, so voices and HD sounds stay.
   Mapping (same as tools/build_og_music.py):
   - music table HD 124-146 = PS2 122-144 (HD 122/123 are HD-only, kept)
   - jingle HD 307 = PS2 305
   - stage music HD 4187-4215 = PS2 4179-4207
   - 48 kHz themes HD 4267-4276 = PS2 4259-4268

   Mods only load when the game plays from extracted files, so a player who
   plays straight from the HD Collection ISO is offered to copy the Budokai 3
   files out of it (default.xex + us\ next to dbz3.exe) and the
   dbz3_iso_path line is taken out of dbz3_user.toml.

   Console program: plain lines and y/n questions, for screen readers. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>

#define VERSION L"2.0"

/* Budokai 1: HD entries replaced by the PS2 entry with the same number. */
static const unsigned short k_b1_swap[] = { 62,63,64,70,71,72,73,74,75,76,77,78,79,80,81,82,83,112,113,158,165,205,208,215,222,227,230,239,241,255,269,301,308,321,337,346,353,361,368,375,382,388,418,428,441,448,451,456,464,469,477,486,489,496,499,502,512,524,530,536,545,705,753,763,784,799,819,851,861,874,882,895,914,926,937,951,969,984,993,1011,1020,1031,1040,1049,1074,1082,1116,1128,1136,1147,1148,1149,1150,1151,1152,1153,1154,1155,1156,1157,1158,1159,1160,1161,1186,1242,1254,1255,1256,1257,1258,1259,1260,1261,1262,1269 };

typedef struct {
    const wchar_t *exe;          /* the port's program */
    const wchar_t *title;        /* "Budokai 3" */
    const wchar_t *afs;          /* the HD sound archive's file name */
    const wchar_t *ps2_name;     /* the PS2 sound archive's file name */
    const char *ps2_path;        /* its path on the PS2 disc */
    unsigned long hd_n, ps2_n;   /* entries in each */
    int iso_support;             /* the port can play from the HD Collection ISO */
    int digit;                   /* the number in the game's name (disc name check) */
} Game;
static const Game k_b1 = { L"dbz1.exe", L"Budokai 1", L"adx_us.afs", L"ADX_US.AFS", "USR/ADX_US.AFS", 1541, 1541, 0, 1 };
static const Game k_b3 = { L"dbz3.exe", L"Budokai 3", L"adx_usa.afs", L"ADX_USA.AFS", "USR/ADX_USA.AFS", 4549, 4541, 1, 3 };
static const Game *G = &k_b3;
#define HD_ENTRIES (G->hd_n)
#define PS2_ENTRIES (G->ps2_n)

/* ---------- console ---------- */

static void say(const wchar_t *fmt, ...)
{
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 2047, fmt, ap);
    buf[2047] = 0;
    va_end(ap);
    wcscat(buf, L"\r\n");
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD n, mode;
    if (GetConsoleMode(h, &mode)) WriteConsoleW(h, buf, (DWORD)wcslen(buf), &n, 0);
    else {
        char u[6144];
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, u, sizeof u, 0, 0);
        if (len > 0) WriteFile(h, u, len - 1, &n, 0);
    }
}

static void read_line(wchar_t *out, int max)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD n = 0;
    out[0] = 0;
    DWORD mode;
    if (GetConsoleMode(h, &mode)) {
        if (!ReadConsoleW(h, out, max - 1, &n, 0)) n = 0;
    } else {                    /* piped input (testing) */
        char c;
        DWORD got;
        while (n < (DWORD)max - 1 && ReadFile(h, &c, 1, &got, 0) && got && c != '\n') out[n++] = (unsigned char)c;
    }
    out[n] = 0;
    while (n && (out[n - 1] == L'\r' || out[n - 1] == L'\n')) out[--n] = 0;
}

/* Returns the first letter typed, lower case, or 0 for an empty line. */
static wchar_t ask(const wchar_t *question)
{
    wchar_t line[256];
    say(L"%ls", question);
    read_line(line, 256);
    for (wchar_t *p = line; *p; p++)
        if (*p != L' ') return towlower(*p);
    return 0;
}

static int ask_yes(const wchar_t *question)
{
    for (;;) {
        wchar_t c = ask(question);
        if (c == L'y') return 1;
        if (c == L'n') return 0;
        say(L"Please type Y for yes or N for no, then press Enter.");
    }
}

static int g_exit_code;

static void finish(int code)
{
    wchar_t line[16];
    g_exit_code = code;
    say(L"");
    say(L"Press Enter to close this window.");
    read_line(line, 16);
    ExitProcess(code);
}

static void fail(const wchar_t *fmt, ...)
{
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 2047, fmt, ap);
    buf[2047] = 0;
    va_end(ap);
    MessageBeep(MB_ICONWARNING);
    say(L"%ls", buf);
    finish(1);
}

/* ---------- small file helpers ---------- */

static int file_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int dir_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static void join(wchar_t *out, const wchar_t *a, const wchar_t *b)
{
    _snwprintf(out, MAX_PATH * 2, L"%ls\\%ls", a, b);
    out[MAX_PATH * 2 - 1] = 0;
}

static const wchar_t *base_name(const wchar_t *p)
{
    const wchar_t *s = wcsrchr(p, L'\\');
    const wchar_t *t = wcsrchr(p, L'/');
    if (t > s) s = t;
    return s ? s + 1 : p;
}

static int ends_with(const wchar_t *s, const wchar_t *suf)
{
    size_t a = wcslen(s), b = wcslen(suf);
    return a >= b && !_wcsicmp(s + a - b, suf);
}

static HANDLE open_read(const wchar_t *p)
{
    return CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                       OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
}

static int read_at(HANDLE h, unsigned long long off, void *buf, DWORD n)
{
    LARGE_INTEGER li;
    DWORD got = 0;
    li.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(h, li, 0, FILE_BEGIN)) return 0;
    return ReadFile(h, buf, n, &got, 0) && got == n;
}

static unsigned long long file_size(HANDLE h)
{
    LARGE_INTEGER li;
    return GetFileSizeEx(h, &li) ? (unsigned long long)li.QuadPart : 0;
}

static unsigned long long free_space(const wchar_t *dir)
{
    ULARGE_INTEGER a;
    return GetDiskFreeSpaceExW(dir, &a, 0, 0) ? a.QuadPart : 0;
}

static void make_dirs(const wchar_t *path)
{
    wchar_t tmp[MAX_PATH * 2];
    wcsncpy(tmp, path, MAX_PATH * 2 - 1);
    tmp[MAX_PATH * 2 - 1] = 0;
    for (wchar_t *p = tmp + 3; *p; p++)
        if (*p == L'\\') { *p = 0; CreateDirectoryW(tmp, 0); *p = L'\\'; }
    CreateDirectoryW(tmp, 0);
}

static int process_running(const wchar_t *exe)
{
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e;
    int found = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    e.dwSize = sizeof e;
    if (Process32FirstW(s, &e))
        do if (!_wcsicmp(e.szExeFile, exe)) found = 1; while (!found && Process32NextW(s, &e));
    CloseHandle(s);
    return found;
}

/* ---------- a file somewhere: plain file, or inside a disc image ---------- */

typedef struct {
    HANDLE h;
    unsigned long long base, size;
} Src;

/* Copies n bytes from src offset off into out, counting progress. */
static unsigned long long g_done, g_total;
static int g_last_tenth;

static void progress(unsigned long long add)
{
    g_done += add;
    if (!g_total) return;
    int tenth = (int)(g_done * 10 / g_total);
    if (tenth > g_last_tenth) {
        g_last_tenth = tenth;
        if (tenth < 10) say(L"%d percent", tenth * 10);
    }
}

static void progress_start(unsigned long long total)
{
    g_done = 0;
    g_total = total;
    g_last_tenth = 0;
}

static unsigned char *g_buf;
#define BUFSZ (4u << 20)

static int copy_range(HANDLE in, unsigned long long off, unsigned long long n, HANDLE out)
{
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(in, li, 0, FILE_BEGIN)) return 0;
    while (n) {
        DWORD chunk = n > BUFSZ ? BUFSZ : (DWORD)n, got = 0, put = 0;
        if (!ReadFile(in, g_buf, chunk, &got, 0) || got != chunk) return 0;
        if (!WriteFile(out, g_buf, chunk, &put, 0) || put != chunk) return 0;
        n -= chunk;
        progress(chunk);
    }
    return 1;
}

static int write_zeros(HANDLE out, DWORD n)
{
    static const unsigned char z[2048];
    DWORD put;
    while (n) {
        DWORD c = n > sizeof z ? sizeof z : n;
        if (!WriteFile(out, z, c, &put, 0) || put != c) return 0;
        n -= c;
    }
    return 1;
}

/* ---------- ISO 9660 (PS2 disc) ---------- */

/* Finds path like "USR/ADX_USA.AFS" in an ISO 9660 image. */
static int iso9660_find(HANDLE h, const char *path, Src *out)
{
    unsigned char sec[2048];
    if (!read_at(h, 16 * 2048, sec, 2048)) return 0;
    if (sec[0] != 1 || memcmp(sec + 1, "CD001", 5)) return 0;
    unsigned long dir_lba = *(unsigned long *)(sec + 156 + 2);
    unsigned long dir_size = *(unsigned long *)(sec + 156 + 10);
    char part[64];
    const char *p = path;
    for (;;) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        int found = 0;
        if (len >= sizeof part) return 0;
        memcpy(part, p, len);
        part[len] = 0;
        for (unsigned long o = 0; o < dir_size && !found; o += 2048) {
            if (!read_at(h, (unsigned long long)dir_lba * 2048 + o, sec, 2048)) return 0;
            for (int i = 0; i < 2048 && sec[i];) {
                int rl = sec[i], nl = sec[i + 32];
                if (rl < 34 || i + rl > 2048) break;
                const char *name = (const char *)sec + i + 33;
                int match = nl >= (int)len && !_strnicmp(name, part, len) &&
                            (nl == (int)len || name[len] == ';');
                if (match) {
                    dir_lba = *(unsigned long *)(sec + i + 2);
                    dir_size = *(unsigned long *)(sec + i + 10);
                    int is_dir = sec[i + 25] & 2;
                    if (!slash) {
                        if (is_dir) return 0;
                        out->h = h;
                        out->base = (unsigned long long)dir_lba * 2048;
                        out->size = dir_size;
                        return 1;
                    }
                    if (!is_dir) return 0;
                    found = 1;
                    break;
                }
                i += rl;
            }
        }
        if (!found) return 0;
        p = slash + 1;
    }
}

/* ---------- XDVDFS (Xbox 360 HD Collection disc) ---------- */

static unsigned long long g_xbase;

static int xdvd_open(HANDLE h, unsigned long *root_sec, unsigned long *root_size)
{
    static const unsigned long long offs[] = {0, 0xFD90000ull, 0x18300000ull, 0x2080000ull};
    unsigned char b[28];
    for (int i = 0; i < 4; i++) {
        if (read_at(h, offs[i] + 32 * 2048, b, 28) && !memcmp(b, "MICROSOFT*XBOX*MEDIA", 20)) {
            g_xbase = offs[i];
            *root_sec = *(unsigned long *)(b + 20);
            *root_size = *(unsigned long *)(b + 24);
            return 1;
        }
    }
    return 0;
}

typedef struct {
    char name[256];
    unsigned long sec, size;
    int is_dir;
} XEnt;

/* Reads a whole directory into ents (tree walk); returns count. */
static int xdvd_list(HANDLE h, unsigned long sec, unsigned long size, XEnt *ents, int max)
{
    if (size == 0 || size > (16u << 20)) return 0;
    unsigned char *d = malloc(size);
    int n = 0, stack[512], sp = 0;
    if (!d || !read_at(h, g_xbase + (unsigned long long)sec * 2048, d, size)) { free(d); return 0; }
    stack[sp++] = 0;
    while (sp && n < max) {
        int o = stack[--sp] * 4;
        if (o + 14 > (int)size) continue;
        unsigned short l = *(unsigned short *)(d + o), r = *(unsigned short *)(d + o + 2);
        if (l == 0xFFFF && r == 0xFFFF) continue;
        int nl = d[o + 13];
        if (o + 14 + nl > (int)size) continue;
        XEnt *e = &ents[n++];
        memcpy(e->name, d + o + 14, nl);
        e->name[nl] = 0;
        e->sec = *(unsigned long *)(d + o + 4);
        e->size = *(unsigned long *)(d + o + 8);
        e->is_dir = d[o + 12] & 0x10;
        if (l && sp < 512) stack[sp++] = l;
        if (r && sp < 512) stack[sp++] = r;
    }
    free(d);
    return n;
}

static XEnt *xdvd_child(XEnt *ents, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (!_stricmp(ents[i].name, name)) return &ents[i];
    return 0;
}

/* ---------- AFS ---------- */

typedef struct {
    unsigned long n;
    unsigned long *off, *size;
    unsigned long meta_off, meta_size;
} Afs;

static int afs_read(Src *s, Afs *a)
{
    unsigned char h[8];
    if (s->size < 16 || !read_at(s->h, s->base, h, 8) || memcmp(h, "AFS\0", 4)) return 0;
    a->n = *(unsigned long *)(h + 4);
    if (a->n == 0 || a->n > 100000) return 0;
    unsigned long *t = malloc(8 * (a->n + 1));
    if (!t || !read_at(s->h, s->base + 8, t, 8 * (a->n + 1))) { free(t); return 0; }
    a->off = malloc(4 * a->n);
    a->size = malloc(4 * a->n);
    for (unsigned long i = 0; i < a->n; i++) {
        a->off[i] = t[2 * i];
        a->size[i] = t[2 * i + 1];
        if ((unsigned long long)a->off[i] + a->size[i] > s->size) { free(t); return 0; }
    }
    a->meta_off = t[2 * a->n];
    a->meta_size = t[2 * a->n + 1];
    free(t);
    return 1;
}

static long ps2_index(unsigned long i)
{
    if (G == &k_b1) {
        for (unsigned k = 0; k < sizeof k_b1_swap / sizeof k_b1_swap[0]; k++)
            if (k_b1_swap[k] == i) return (long)i;
        return -1;
    }
    if (i >= 124 && i <= 146) return (long)i - 2;
    if (i == 307) return 305;
    if (i >= 4187 && i <= 4215) return (long)i - 8;
    if (i >= 4267 && i <= 4276) return (long)i - 8;
    return -1;
}

static unsigned long align(unsigned long long x) { return (unsigned long)((x + 0x7ff) & ~0x7ffull); }

static int build_afs(Src *hd, Afs *ha, Src *ps2, Afs *pa, const wchar_t *out_path)
{
    unsigned long long total = 0;
    for (unsigned long i = 0; i < ha->n; i++) {
        long p = ps2_index(i);
        total += p >= 0 ? pa->size[p] : ha->size[i];
    }
    progress_start(total);
    HANDLE o = CreateFileW(out_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (o == INVALID_HANDLE_VALUE) return 0;
    unsigned long first = align(8 + 8ull * ha->n + 8);
    if (ha->n && ha->off[0] > first) first = ha->off[0];
    unsigned long *toc = calloc(2 * ha->n + 2, 4);
    unsigned long long pos = first;
    int ok = toc && write_zeros(o, first);
    for (unsigned long i = 0; ok && i < ha->n; i++) {
        long p = ps2_index(i);
        Src *s = p >= 0 ? ps2 : hd;
        unsigned long off = p >= 0 ? pa->off[p] : ha->off[i];
        unsigned long size = p >= 0 ? pa->size[p] : ha->size[i];
        if (pos > 0xFFFFFFFFull) { ok = 0; break; }
        toc[2 * i] = (unsigned long)pos;
        toc[2 * i + 1] = size;
        unsigned long long end = pos + size, next = (end + 0x7ff) & ~0x7ffull;
        ok = copy_range(s->h, s->base + off, size, o) && write_zeros(o, (DWORD)(next - end));
        pos = next;
    }
    if (ok && ha->meta_off && ha->meta_size) {      /* name table, copied unchanged */
        toc[2 * ha->n] = (unsigned long)pos;
        toc[2 * ha->n + 1] = ha->meta_size;
        ok = copy_range(hd->h, hd->base + ha->meta_off, ha->meta_size, o) &&
             write_zeros(o, (DWORD)(((pos + ha->meta_size + 0x7ff) & ~0x7ffull) - pos - ha->meta_size));
    }
    if (ok) {
        unsigned char head[8] = {'A', 'F', 'S', 0};
        DWORD put;
        LARGE_INTEGER z = {0};
        *(unsigned long *)(head + 4) = ha->n;
        ok = SetFilePointerEx(o, z, 0, FILE_BEGIN) && WriteFile(o, head, 8, &put, 0) &&
             WriteFile(o, toc, 8 * (ha->n + 1), &put, 0);
    }
    free(toc);
    CloseHandle(o);
    if (!ok) DeleteFileW(out_path);
    return ok;
}

static int copy_src_to_file(Src *s, const wchar_t *out_path)
{
    HANDLE o = CreateFileW(out_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (o == INVALID_HANDLE_VALUE) return 0;
    int ok = copy_range(s->h, s->base, s->size, o);
    CloseHandle(o);
    if (!ok) DeleteFileW(out_path);
    return ok;
}

/* ---------- game folder ---------- */

static wchar_t g_game[MAX_PATH * 2];

static int find_game(void)
{
    wchar_t p[MAX_PATH * 2], t[MAX_PATH * 2];
    GetModuleFileNameW(0, p, MAX_PATH * 2);
    *wcsrchr(p, L'\\') = 0;
    for (int pass = 0; pass < 2; pass++) {
        if (pass) GetCurrentDirectoryW(MAX_PATH * 2, p);
        join(t, p, L"dbz3.exe");
        if (file_exists(t)) { wcscpy(g_game, p); G = &k_b3; return 1; }
        join(t, p, L"dbz1.exe");
        if (file_exists(t)) { wcscpy(g_game, p); G = &k_b1; return 1; }
    }
    return 0;
}

/* HD data folder holding us\<sound archive>: the ports accept these layouts. */
static int find_hd_folder(wchar_t *afs_out)
{
    static const wchar_t *dirs[] = {L"us", L"assets\\us", L"DBZ3\\us"};
    for (int i = 0; i < 3; i++) {
        wchar_t d[MAX_PATH * 2];
        join(d, g_game, dirs[i]);
        join(afs_out, d, G->afs);
        if (file_exists(afs_out)) return 1;
    }
    return 0;
}

static wchar_t g_toml[MAX_PATH * 2];

/* dbz3_iso_path from dbz3_user.toml, or empty. */
static void toml_iso_path(wchar_t *out)
{
    out[0] = 0;
    if (!G->iso_support) return;          /* Budokai 1's port plays from its assets folder */
    join(g_toml, g_game, L"dbz3_user.toml");
    FILE *f = _wfopen(g_toml, L"rb");
    char line[2048];
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "dbz3_iso_path", 13)) continue;
        char *q = strchr(p, '"');
        if (!q) break;
        char val[2048], *v = val;
        for (q++; *q && *q != '"'; q++) {
            if (*q == '\\' && q[1]) q++;
            *v++ = *q;
        }
        *v = 0;
        MultiByteToWideChar(CP_UTF8, 0, val, -1, out, MAX_PATH * 2);
        break;
    }
    fclose(f);
}

/* Takes dbz3_iso_path out of dbz3_user.toml (backup kept). */
static int toml_drop_iso(void)
{
    FILE *f = _wfopen(g_toml, L"rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *d = malloc(n + 1), *o = malloc(n + 1), *w = o;
    if (!d || !o || fread(d, 1, n, f) != (size_t)n) { fclose(f); return 0; }
    fclose(f);
    d[n] = 0;
    for (char *p = d; *p;) {
        char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p + 1) : strlen(p);
        char *s = p;
        while (*s == ' ' || *s == '\t') s++;
        if (strncmp(s, "dbz3_iso_path", 13)) { memcpy(w, p, len); w += len; }
        p += len;
    }
    wchar_t bak[MAX_PATH * 2];
    _snwprintf(bak, MAX_PATH * 2, L"%ls.bak-music", g_toml);
    CopyFileW(g_toml, bak, FALSE);
    f = _wfopen(g_toml, L"wb");
    if (!f) return 0;
    fwrite(o, 1, w - o, f);
    fclose(f);
    free(d);
    free(o);
    return 1;
}

/* Is this file the HD Collection disc with Budokai 3 (USA) on it? */
static int hd_iso_check(HANDLE h, XEnt **dbz3_files, int *dbz3_n, XEnt **us_files, int *us_n)
{
    unsigned long rs, rz;
    static XEnt root[256], d3[256], us[256];
    if (!xdvd_open(h, &rs, &rz)) return 0;
    int n = xdvd_list(h, rs, rz, root, 256);
    XEnt *e = xdvd_child(root, n, "DBZ3");
    if (!e || !e->is_dir) return 0;
    *dbz3_n = xdvd_list(h, e->sec, e->size, d3, 256);
    e = xdvd_child(d3, *dbz3_n, "us");
    if (!e || !e->is_dir) return 0;
    *us_n = xdvd_list(h, e->sec, e->size, us, 256);
    if (!xdvd_child(us, *us_n, "adx_usa.afs")) return 0;
    *dbz3_files = d3;
    *us_files = us;
    return 1;
}

/* Copies Budokai 3 out of the HD Collection ISO: the .xex as default.xex,
   us\* into us\. Files already there with the right size are skipped. */
static int extract_hd(HANDLE h, XEnt *d3, int d3n, XEnt *us, int usn)
{
    wchar_t usdir[MAX_PATH * 2], path[MAX_PATH * 2], wname[256];
    unsigned long long total = 0;
    XEnt *xex = 0;
    for (int i = 0; i < d3n; i++)
        if (!d3[i].is_dir && strlen(d3[i].name) > 4 && !_stricmp(d3[i].name + strlen(d3[i].name) - 4, ".xex"))
            xex = &d3[i];
    if (!xex) return 0;
    join(usdir, g_game, L"us");
    make_dirs(usdir);
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) progress_start(total);
        for (int i = -1; i < usn; i++) {
            XEnt *e = i < 0 ? xex : &us[i];
            if (e->is_dir) continue;
            if (i < 0) join(path, g_game, L"default.xex");
            else {
                MultiByteToWideChar(CP_ACP, 0, e->name, -1, wname, 256);
                join(path, usdir, wname);
            }
            HANDLE t = open_read(path);
            int have = 0;
            if (t != INVALID_HANDLE_VALUE) { have = file_size(t) == e->size; CloseHandle(t); }
            if (have) continue;
            if (pass == 0) { total += e->size; continue; }
            Src s = {h, g_xbase + (unsigned long long)e->sec * 2048, e->size};
            wchar_t part[MAX_PATH * 2];
            _snwprintf(part, MAX_PATH * 2, L"%ls.part", path);
            if (!copy_src_to_file(&s, part)) return 0;
            if (!MoveFileExW(part, path, MOVEFILE_REPLACE_EXISTING)) return 0;
        }
    }
    return 1;
}

/* ---------- finding the PS2 disc ---------- */

static wchar_t g_7z[MAX_PATH * 2];

static int find_7zip(void)
{
    static const wchar_t *keys[] = {L"Path64", L"Path"};
    for (int i = 0; i < 2; i++) {
        wchar_t v[MAX_PATH];
        DWORD sz = sizeof v;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\7-Zip", keys[i], RRF_RT_REG_SZ, 0, v, &sz) == ERROR_SUCCESS ||
            (sz = sizeof v, RegGetValueW(HKEY_CURRENT_USER, L"SOFTWARE\\7-Zip", keys[i], RRF_RT_REG_SZ, 0, v, &sz)) == ERROR_SUCCESS) {
            join(g_7z, v, L"7z.exe");
            if (file_exists(g_7z)) return 1;
        }
    }
    static const wchar_t *env[] = {L"ProgramFiles", L"ProgramW6432", L"ProgramFiles(x86)"};
    for (int i = 0; i < 3; i++) {
        wchar_t pf[MAX_PATH];
        if (!GetEnvironmentVariableW(env[i], pf, MAX_PATH)) continue;
        _snwprintf(g_7z, MAX_PATH * 2, L"%ls\\7-Zip\\7z.exe", pf);
        if (file_exists(g_7z)) return 1;
    }
    g_7z[0] = 0;
    return 0;
}

/* Runs a command hidden, waits; returns exit code or -1. */
static int run(wchar_t *cmd)
{
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    DWORD code = (DWORD)-1;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(0, cmd, 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

static int is_ps2_iso(const wchar_t *path)
{
    HANDLE h = open_read(path);
    Src s;
    int ok = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (iso9660_find(h, G->ps2_path, &s)) {
        Afs a;
        ok = afs_read(&s, &a) && a.n == PS2_ENTRIES;
    }
    CloseHandle(h);
    return ok;
}

/* An archive named like the PS2 disc of this game: "Budokai 3", or for
   Budokai 1 "Budokai" with no 2 or 3 (and not the HD Collection or a mod). */
static int name_looks_right(const wchar_t *name)
{
    wchar_t low[MAX_PATH];
    wcsncpy(low, name, MAX_PATH - 1);
    low[MAX_PATH - 1] = 0;
    _wcslwr(low);
    if (!wcsstr(low, L"budokai") || wcsstr(low, L"hd") || wcsstr(low, L"access") || wcsstr(low, L"tenkaichi")) return 0;
    if (G->digit == 3) return wcsstr(low, L"3") != NULL;
    return !wcsstr(low, L"2") && !wcsstr(low, L"3");
}

/* Looks in a few usual places for the PS2 disc image of the game: any .iso
   that really is one, or an archive (.7z/.zip) named like it. */
static int search_ps2(wchar_t *found)
{
    wchar_t dirs[6][MAX_PATH * 2];
    int nd = 0;
    wcscpy(dirs[nd++], g_game);
    {
        wchar_t p[MAX_PATH * 2];
        GetModuleFileNameW(0, p, MAX_PATH * 2);
        *wcsrchr(p, L'\\') = 0;
        if (_wcsicmp(p, g_game)) wcscpy(dirs[nd++], p);
    }
    wchar_t *k;
    if (SHGetKnownFolderPath(&FOLDERID_Downloads, 0, 0, &k) == S_OK) { wcscpy(dirs[nd++], k); CoTaskMemFree(k); }
    if (SHGetKnownFolderPath(&FOLDERID_Desktop, 0, 0, &k) == S_OK) { wcscpy(dirs[nd++], k); CoTaskMemFree(k); }
    if (SHGetKnownFolderPath(&FOLDERID_Documents, 0, 0, &k) == S_OK) { wcscpy(dirs[nd++], k); CoTaskMemFree(k); }
    {   /* the folder the game folder sits in */
        wchar_t p[MAX_PATH * 2];
        wcscpy(p, g_game);
        wchar_t *s = wcsrchr(p, L'\\');
        if (s && s - p > 2) { *s = 0; wcscpy(dirs[nd++], p); }
    }
    wchar_t archive[MAX_PATH * 2] = L"";
    for (int i = 0; i < nd; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) if (!_wcsicmp(dirs[i], dirs[j])) dup = 1;
        if (dup) continue;
        wchar_t pat[MAX_PATH * 2];
        WIN32_FIND_DATAW fd;
        join(pat, dirs[i], L"*");
        HANDLE f = FindFirstFileW(pat, &fd);
        if (f == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            unsigned long long sz = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            wchar_t full[MAX_PATH * 2];
            join(full, dirs[i], fd.cFileName);
            if (ends_with(fd.cFileName, L".iso") && sz > (100u << 20) && sz < (9ull << 30) && is_ps2_iso(full)) {
                wcscpy(found, full);
                FindClose(f);
                return 1;
            }
            if (!archive[0] && (ends_with(fd.cFileName, L".7z") || ends_with(fd.cFileName, L".zip")) &&
                name_looks_right(fd.cFileName))
                wcscpy(archive, full);
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    if (archive[0]) { wcscpy(found, archive); return 1; }
    return 0;
}

static int pick_file(wchar_t *out)
{
    OPENFILENAMEW ofn = {sizeof ofn};
    out[0] = 0;
    static wchar_t title[128];
    _snwprintf(title, 128, L"Choose your PS2 %ls (USA) disc image", G->title);
    ofn.lpstrFilter = L"PS2 disc image or archive (*.iso, *.7z, *.zip, *.afs)\0*.iso;*.7z;*.zip;*.afs\0All files\0*.*\0";
    ofn.lpstrFile = out;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.hwndOwner = GetConsoleWindow();
    return GetOpenFileNameW(&ofn);
}

/* Temp folder for unpacking an archive, removed at the end. */
static wchar_t g_tmp[MAX_PATH * 2];

static void remove_tree(const wchar_t *dir)
{
    wchar_t pat[MAX_PATH * 2], p[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    join(pat, dir, L"*");
    HANDLE f = FindFirstFileW(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            join(p, dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) remove_tree(p);
            else { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); DeleteFileW(p); }
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    RemoveDirectoryW(dir);
}

static HANDLE g_ps2_h = INVALID_HANDLE_VALUE, g_sfd_h = INVALID_HANDLE_VALUE;

static void cleanup_tmp(void)
{
    if (g_ps2_h != INVALID_HANDLE_VALUE) { CloseHandle(g_ps2_h); g_ps2_h = INVALID_HANDLE_VALUE; }
    if (g_sfd_h != INVALID_HANDLE_VALUE) { CloseHandle(g_sfd_h); g_sfd_h = INVALID_HANDLE_VALUE; }
    if (g_tmp[0]) { remove_tree(g_tmp); g_tmp[0] = 0; }
}

/* Finds the first file ending in suffix under dir (one level of folders). */
static int find_in(const wchar_t *dir, const wchar_t *suffix, wchar_t *out, int depth)
{
    wchar_t pat[MAX_PATH * 2], p[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    join(pat, dir, L"*");
    HANDLE f = FindFirstFileW(pat, &fd);
    int ok = 0;
    if (f == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        join(p, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 3 && find_in(p, suffix, out, depth + 1)) ok = 1;
        } else if (ends_with(fd.cFileName, suffix)) {
            wcscpy(out, p);
            ok = 1;
        }
    } while (!ok && FindNextFileW(f, &fd));
    FindClose(f);
    return ok;
}

/* Unpacks an archive into g_tmp. Returns 1 if done. */
static int unpack(const wchar_t *archive)
{
    wchar_t cmd[MAX_PATH * 6];
    int is7z = ends_with(archive, L".7z");
    _snwprintf(g_tmp, MAX_PATH * 2, L"%ls\\og_music_unpack", g_game);
    remove_tree(g_tmp);
    make_dirs(g_tmp);
    if (is7z) {
        if (!find_7zip()) {
            cleanup_tmp();
            fail(L"%ls is a 7z file, and 7-Zip is not installed, so I can't open it.\r\n"
                 L"Either install 7-Zip from https://www.7-zip.org and run this again, "
                 L"or unpack the 7z yourself and choose the .iso inside it.", base_name(archive));
        }
        _snwprintf(cmd, MAX_PATH * 6, L"\"%ls\" x -y \"-o%ls\" \"%ls\"", g_7z, g_tmp, archive);
    } else {
        wchar_t sys[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);
        _snwprintf(cmd, MAX_PATH * 6, L"\"%ls\\tar.exe\" -x -f \"%ls\" -C \"%ls\"", sys, archive, g_tmp);
    }
    say(L"Unpacking %ls. This can take a few minutes, please wait.", base_name(archive));
    int code = run(cmd);
    if (code != 0) {
        cleanup_tmp();
        fail(L"Unpacking %ls did not work (error %d). Try unpacking it yourself and choose the .iso inside.",
             base_name(archive), code);
    }
    return 1;
}

/* ---------- main ---------- */

static Src g_ps2_afs, g_ps2_sfd;
static int g_have_sfd;

/* Opens the PS2 music from: a .iso, an ADX_USA.AFS file, a folder, or an
   archive (unpacked first). Fails with a message if it isn't the right disc. */
static void open_ps2(const wchar_t *path)
{
    wchar_t p[MAX_PATH * 2];
    wcscpy(p, path);
    if (ends_with(p, L".7z") || ends_with(p, L".zip")) {
        unpack(p);
        if (!find_in(g_tmp, L".iso", p, 0) && !find_in(g_tmp, G->ps2_name, p, 0)) {
            cleanup_tmp();
            fail(L"I couldn't find a disc image inside %ls.", base_name(path));
        }
    }
    if (dir_exists(p)) {
        wchar_t f[MAX_PATH * 2];
        if (!find_in(p, G->ps2_name, f, 0) && !find_in(p, L".iso", f, 0))
            fail(L"I couldn't find %ls or a .iso in the folder %ls.", G->ps2_name, p);
        wcscpy(p, f);
    }
    g_ps2_h = open_read(p);
    if (g_ps2_h == INVALID_HANDLE_VALUE) fail(L"I can't open %ls.", p);
    if (ends_with(p, L".afs")) {
        g_ps2_afs.h = g_ps2_h;
        g_ps2_afs.base = 0;
        g_ps2_afs.size = file_size(g_ps2_h);
        /* OPENING.SFD sits next to it on the disc */
        wchar_t sfd[MAX_PATH * 2];
        wcscpy(sfd, p);
        wcscpy((wchar_t *)base_name(sfd), L"OPENING.SFD");
        g_sfd_h = G == &k_b3 ? open_read(sfd) : INVALID_HANDLE_VALUE;
        if (g_sfd_h != INVALID_HANDLE_VALUE) {
            g_ps2_sfd.h = g_sfd_h;
            g_ps2_sfd.base = 0;
            g_ps2_sfd.size = file_size(g_sfd_h);
            g_have_sfd = 1;
        }
    } else {
        if (!iso9660_find(g_ps2_h, G->ps2_path, &g_ps2_afs))
            fail(L"%ls is not the PS2 %ls disc (it has no USR\\%ls).", base_name(p), G->title, G->ps2_name);
        g_have_sfd = G == &k_b3 && iso9660_find(g_ps2_h, "USR/OPENING.SFD", &g_ps2_sfd);
    }
}

int wmain(int argc, wchar_t **argv)
{
    SetConsoleTitleW(L"Budokai Original Music " VERSION);
    CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    g_buf = malloc(BUFSZ);
    say(L"Budokai Original Music, version %ls.", VERSION);
    if (!find_game())
        fail(L"I can't find the game. Put this program in your game folder, next to dbz1.exe for Budokai 1 "
             L"or dbz3.exe for Budokai 3, and run it again.");
    say(L"This puts the original PS2 soundtrack back into the %ls HD game, using your own PS2 disc.", G->title);
    say(L"");
    if (process_running(G->exe))
        fail(L"The game is running. Close it first, then run this again.");

    wchar_t mod[MAX_PATH * 2], modus[MAX_PATH * 2], tmp[MAX_PATH * 2];
    join(mod, g_game, L"mods\\og_music");
    join(modus, mod, L"us");
    join(tmp, modus, G->afs);
    if (file_exists(tmp)) {
        say(L"The original music is already installed.");
        for (;;) {
            wchar_t c = ask(L"Type R and press Enter to remove it, B to build it again, or Q to quit.");
            if (c == L'q' || c == 0) finish(0);
            if (c == L'r') {
                remove_tree(mod);
                if (dir_exists(mod)) fail(L"I couldn't remove all of %ls. Is the game still open?", mod);
                MessageBeep(MB_OK);
                say(L"Removed. The game plays the HD music again.");
                finish(0);
            }
            if (c == L'b') break;
        }
    }

    /* ---- HD side: the game must play from extracted files ---- */
    wchar_t hd_afs[MAX_PATH * 2], iso[MAX_PATH * 2];
    int have_folder = find_hd_folder(hd_afs);
    toml_iso_path(iso);
    HANDLE hd_iso = INVALID_HANDLE_VALUE;
    XEnt *d3 = 0, *us = 0;
    int d3n = 0, usn = 0, need_extract = 0, need_toml = iso[0] != 0;
    if (!have_folder && !G->iso_support)
        fail(L"I can't find the game's data (assets\\us\\adx_us.afs next to dbz1.exe). Set up the Budokai 1 port "
             L"as its guide says and check that the game starts, then run this again.");
    if (!have_folder) {
        if (iso[0]) {
            hd_iso = open_read(iso);
            if (hd_iso != INVALID_HANDLE_VALUE && !hd_iso_check(hd_iso, &d3, &d3n, &us, &usn)) {
                CloseHandle(hd_iso);
                hd_iso = INVALID_HANDLE_VALUE;
            }
        }
        if (hd_iso == INVALID_HANDLE_VALUE) {   /* an .iso next to dbz3.exe */
            wchar_t pat[MAX_PATH * 2];
            WIN32_FIND_DATAW fd;
            join(pat, g_game, L"*.iso");
            HANDLE f = FindFirstFileW(pat, &fd);
            if (f != INVALID_HANDLE_VALUE) {
                do {
                    join(iso, g_game, fd.cFileName);
                    hd_iso = open_read(iso);
                    if (hd_iso == INVALID_HANDLE_VALUE) continue;
                    if (hd_iso_check(hd_iso, &d3, &d3n, &us, &usn)) break;
                    CloseHandle(hd_iso);
                    hd_iso = INVALID_HANDLE_VALUE;
                } while (FindNextFileW(f, &fd));
                FindClose(f);
            }
        }
        if (hd_iso == INVALID_HANDLE_VALUE)
            fail(L"I can't find the game's data. I need the USA version of the HD Collection, either extracted "
                 L"(a us folder with adx_usa.afs next to dbz3.exe) or as an .iso the game is set up to use.");
        need_extract = 1;
    }
    if (need_extract || need_toml) {
        say(L"Your game plays straight from the ISO. The game only loads mods, this one too, when it plays from extracted files.");
        if (need_extract) {
            unsigned long long sz = 0;
            for (int i = 0; i < usn; i++) sz += us[i].size;
            say(L"I can copy the Budokai 3 files out of your ISO into the game folder. That's about %.1f GB. Your ISO is not changed.",
                sz / 1073741824.0);
        }
        say(L"I'll also set the game to use the extracted files, and keep a backup of your settings in dbz3_user.toml.bak-music.");
        if (!ask_yes(L"Do that? Type Y or N and press Enter."))
            fail(L"OK, nothing was changed.");
    }

    /* ---- PS2 side ---- */
    wchar_t ps2[MAX_PATH * 2] = L"";
    if (argc > 1) wcscpy(ps2, argv[1]);
    else {
        say(L"Looking for your PS2 %ls disc image.", G->title);
        if (search_ps2(ps2)) {
            say(L"Found %ls.", base_name(ps2));
            if (!ask_yes(L"Use it? Type Y or N and press Enter.")) ps2[0] = 0;
        } else say(L"I didn't find it in the game folder, Downloads, Desktop or Documents.");
        if (!ps2[0]) {
            say(L"A window opens now. Choose your PS2 %ls (USA) disc image: a .iso, or a .7z or .zip it came in.", G->title);
            if (!pick_file(ps2)) fail(L"No file chosen. Nothing was changed.");
        }
    }
    open_ps2(ps2);
    Afs pa;
    if (!afs_read(&g_ps2_afs, &pa) || pa.n != PS2_ENTRIES) {
        cleanup_tmp();
        fail(L"That disc isn't the USA PS2 release of %ls. Only the USA version works.", G->title);
    }
    say(L"PS2 disc OK.%ls", g_have_sfd || G != &k_b3 ? L"" : L" No opening movie found on it, so the HD opening stays.");

    /* ---- space ---- */
    unsigned long long need = 800ull << 20;
    if (need_extract) for (int i = 0; i < usn; i++) need += us[i].size;
    if (free_space(g_game) < need) {
        cleanup_tmp();
        fail(L"There isn't enough free space on the drive with the game. I need about %.1f GB.", need / 1073741824.0);
    }

    /* ---- extract HD files if needed ---- */
    if (need_extract) {
        say(L"Copying the Budokai 3 files out of your ISO. This takes a few minutes.");
        if (!extract_hd(hd_iso, d3, d3n, us, usn)) {
            cleanup_tmp();
            fail(L"Copying the game files failed. Is there enough space, and is the folder writable?");
        }
        say(L"Game files copied.");
        if (!find_hd_folder(hd_afs)) fail(L"Something went wrong: the copied files are not where I expected.");
    }
    if (need_toml && !toml_drop_iso()) {
        cleanup_tmp();
        fail(L"I couldn't change dbz3_user.toml. In the game's launcher, choose Extracted folder as the data source.");
    }

    /* ---- build ---- */
    HANDLE hh = open_read(hd_afs);
    Src hd = {hh, 0, hh == INVALID_HANDLE_VALUE ? 0 : file_size(hh)};
    Afs ha;
    if (hh == INVALID_HANDLE_VALUE || !afs_read(&hd, &ha) || ha.n != HD_ENTRIES) {
        cleanup_tmp();
        fail(L"The game's us\\%ls isn't the one I expect (USA HD Collection). Nothing was changed.", G->afs);
    }
    make_dirs(modus);
    wchar_t out[MAX_PATH * 2], part[MAX_PATH * 2];
    join(out, modus, G->afs);
    _snwprintf(part, MAX_PATH * 2, L"%ls.part", out);
    say(L"Building the music. This takes a minute or two.");
    if (!build_afs(&hd, &ha, &g_ps2_afs, &pa, part) || !MoveFileExW(part, out, MOVEFILE_REPLACE_EXISTING)) {
        cleanup_tmp();
        remove_tree(mod);
        fail(L"Building the music failed. Is there enough space, and is the game closed?");
    }
    CloseHandle(hh);
    join(out, modus, L"opening.sfd");
    if (g_have_sfd) {
        say(L"Copying the PS2 opening movie.");
        progress_start(g_ps2_sfd.size);
        _snwprintf(part, MAX_PATH * 2, L"%ls.part", out);
        if (!copy_src_to_file(&g_ps2_sfd, part) || !MoveFileExW(part, out, MOVEFILE_REPLACE_EXISTING))
            say(L"Copying the opening movie failed, so the HD opening stays. The music is fine.");
    } else DeleteFileW(out);
    join(out, mod, L"manifest.txt");
    FILE *m = _wfopen(out, L"wb");
    if (m) {
        fprintf(m, "name=OG Music\r\n"
                   "description=Original PS2 (US) %s soundtrack by Kenji Yamamoto, put back into the HD game's music slots. "
                   "Built from your own PS2 disc; voices are unchanged.\r\n"
                   "author=Budokai Original Music (BlindChallengerX)\r\n"
                   "version=2.0\r\n", G == &k_b1 ? "Budokai 1" : "Budokai 3");
        fclose(m);
    }
    join(out, mod, L".disabled");
    DeleteFileW(out);
    cleanup_tmp();

    MessageBeep(MB_OK);
    say(L"");
    say(L"Done. The original PS2 music is installed and turned on. Start the game the way you always do.");
    say(L"To go back to the HD music, run this program again and choose Remove.");
    finish(0);
    return 0;
}
