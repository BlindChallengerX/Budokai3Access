/*
 * Speech for Budokai3Access: Prism (screen readers) loaded straight into the
 * game, which is 64-bit like Prism. Falls back to the Windows SAPI voice if
 * Prism is missing or finds nothing. All speaking happens on one worker
 * thread; game hooks only queue text.
 */
#define COBJMACROS
#include "access.h"
#include <sapi.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prism.h"

typedef PrismConfig (*fn_config_init)(void);
typedef PrismContext *(*fn_init)(PrismConfig *);
typedef PrismBackend *(*fn_acquire_best)(PrismContext *);
typedef PrismError (*fn_initialize)(PrismBackend *);
typedef PrismError (*fn_output)(PrismBackend *, const char *, bool);
typedef PrismError (*fn_stop)(PrismBackend *);
typedef const char *(*fn_name)(PrismBackend *);

#define QCAP 64
typedef struct { wchar_t *text; int interrupt; int stop; } Item;

static CRITICAL_SECTION g_qlock;
static HANDLE g_qevent;
static Item g_queue[QCAP];
static int g_qhead, g_qcount;
static int g_mode;             /* 0 none, 1 prism, 2 sapi */
static ISpVoice *g_sapi;
static PrismBackend *g_be;
static fn_output p_output;
static fn_stop p_stop;
static wchar_t g_last[2048];
static volatile int g_speech_stop;
static HANDLE g_speech_thread;

static int start_prism(void) {
    wchar_t dll[MAX_PATH];
    swprintf(dll, MAX_PATH, L"%lsprism.dll", g_mod_dir);
    HMODULE h = LoadLibraryExW(dll, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) { log_line("prism.dll not found in the Budokai3Access folder"); return 0; }
    fn_config_init config_init = (fn_config_init)GetProcAddress(h, "prism_config_init");
    fn_init init = (fn_init)GetProcAddress(h, "prism_init");
    fn_acquire_best acquire_best = (fn_acquire_best)GetProcAddress(h, "prism_registry_acquire_best");
    fn_initialize initialize = (fn_initialize)GetProcAddress(h, "prism_backend_initialize");
    fn_name name = (fn_name)GetProcAddress(h, "prism_backend_name");
    p_output = (fn_output)GetProcAddress(h, "prism_backend_output");
    p_stop = (fn_stop)GetProcAddress(h, "prism_backend_stop");
    if (!config_init || !init || !acquire_best || !initialize || !p_output || !p_stop) {
        log_line("prism.dll is missing functions"); return 0;
    }
    PrismConfig cfg = config_init();
    PrismContext *ctx = init(&cfg);
    if (!ctx) { log_line("prism_init failed"); return 0; }
    g_be = acquire_best(ctx);
    if (!g_be) { log_line("Prism found no screen reader or voice"); return 0; }
    if (initialize(g_be) != PRISM_OK) log_line("Prism backend initialize returned an error; trying anyway");
    log_line("speech: Prism, speaking through %s", name ? name(g_be) : "?");
    return 1;
}

static int start_sapi(void) {
    if (FAILED(CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL, &IID_ISpVoice, (void **)&g_sapi)) || !g_sapi) {
        log_line("SAPI voice could not be created");
        return 0;
    }
    log_line("speech: using the Windows SAPI voice");
    return 1;
}

static DWORD WINAPI speech_thread(LPVOID unused) {
    (void)unused;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (start_prism()) g_mode = 1;
    else if (start_sapi()) g_mode = 2;
    while (!g_speech_stop) {
        WaitForSingleObject(g_qevent, INFINITE);
        for (;;) {
            Item it; int have = 0;
            EnterCriticalSection(&g_qlock);
            if (g_qcount) { it = g_queue[g_qhead]; g_qhead = (g_qhead + 1) % QCAP; g_qcount--; have = 1; }
            LeaveCriticalSection(&g_qlock);
            if (!have) break;
            if (g_mode == 1) {
                if (it.stop) p_stop(g_be);
                else { char *u = to_utf8(it.text); if (u) { p_output(g_be, u, it.interrupt != 0); free(u); } }
            } else if (g_mode == 2 && g_sapi) {
                if (it.stop) ISpVoice_Speak(g_sapi, NULL, SPF_PURGEBEFORESPEAK, NULL);
                else ISpVoice_Speak(g_sapi, it.text, SPF_ASYNC | SPF_IS_NOT_XML | (it.interrupt ? SPF_PURGEBEFORESPEAK : 0), NULL);
            }
            free(it.text);
        }
    }
    return 0;
}

void speech_init(void) {
    InitializeCriticalSection(&g_qlock);
    g_qevent = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_speech_stop = 0;
    g_speech_thread = CreateThread(NULL, 0, speech_thread, NULL, 0, NULL);
}

void speech_shutdown(void) {
    g_speech_stop = 1;
    SetEvent(g_qevent);
    WaitForSingleObject(g_speech_thread, 2000);
    CloseHandle(g_speech_thread);
    if (g_sapi) { ISpVoice_Release(g_sapi); g_sapi = NULL; }
    g_mode = 0; g_qcount = 0; g_qhead = 0;
    CloseHandle(g_qevent);
    DeleteCriticalSection(&g_qlock);
}

static void enqueue(const wchar_t *text, int interrupt, int stop) {
    Item it; it.text = _wcsdup(text ? text : L""); it.interrupt = interrupt; it.stop = stop;
    EnterCriticalSection(&g_qlock);
    if (interrupt) {
        while (g_qcount) { free(g_queue[g_qhead].text); g_qhead = (g_qhead + 1) % QCAP; g_qcount--; }
    }
    if (g_qcount == QCAP) { free(g_queue[g_qhead].text); g_qhead = (g_qhead + 1) % QCAP; g_qcount--; }
    g_queue[(g_qhead + g_qcount) % QCAP] = it; g_qcount++;
    LeaveCriticalSection(&g_qlock);
    SetEvent(g_qevent);
}

void speak(const wchar_t *text, int interrupt) {
    if (!text || !*text) return;
    wcsncpy(g_last, text, 2047); g_last[2047] = 0;
    char *u = to_utf8(text);
    log_line("SAY%s: %s", interrupt ? "" : "+", u ? u : "?");
    free(u);
    enqueue(text, interrupt, 0);
}

void speak_utf8(const char *text, int interrupt) {
    wchar_t *w = from_utf8(text);
    if (w) { speak(w, interrupt); free(w); }
}

void speakf(int interrupt, const char *fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    speak_utf8(buf, interrupt);
}

void speak_repeat(void) { if (g_last[0]) enqueue(g_last, 1, 0); }
void speech_stop(void) { enqueue(L"", 1, 1); }
