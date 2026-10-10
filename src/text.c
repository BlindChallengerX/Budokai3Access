/*
 * Screen text. Budokai 3 draws its text (dialogue, descriptions, messages)
 * as pre-rendered images in #AZT texture packs. Every 2D sprite gets its
 * texture through 820..96ff0 (r5 = texture number in the current pack, global
 * 8238ce10 = pack, 8238ce14 = its index table), every frame.
 *
 * DBZ3Access\text.tsv maps (pack checksum, texture index) to the words in the
 * image; it was made by OCR of every text image on the disc. The hook notes
 * which known texts are drawn; the mod thread speaks texts that appear and
 * forgets texts that have not been drawn for a moment.
 */
#include "access.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- text table ---------- */
typedef struct { uint64_t key; char *text; } TEnt;
static TEnt *g_tab; static uint32_t g_tabcap;

static uint32_t hkey(uint64_t k) { k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33; return (uint32_t)k; }
static uint64_t mkkey(uint32_t sum, uint32_t idx) { return ((uint64_t)sum << 32) | idx; }

const char *text_lookup(uint32_t sum, uint32_t idx) {
    if (!g_tab) return NULL;
    uint64_t k = mkkey(sum, idx);
    for (uint32_t i = hkey(k) & (g_tabcap - 1);; i = (i + 1) & (g_tabcap - 1)) {
        if (!g_tab[i].text) return NULL;
        if (g_tab[i].key == k) return g_tab[i].text;
    }
}

static void load_table(void) {
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%lstext.tsv", g_mod_dir);
    FILE *f = _wfopen(path, L"rb");
    if (!f) { log_line("text.tsv missing: screen text will not be read"); return; }
    g_tabcap = 1 << 15;
    g_tab = (TEnt *)calloc(g_tabcap, sizeof(TEnt));
    char line[4096]; int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = 0;
        char *t1 = strchr(line, '\t'); if (!t1) continue;
        char *t2 = strchr(t1 + 1, '\t'); if (!t2) continue;
        uint32_t sum = (uint32_t)strtoul(line, NULL, 16), idx = (uint32_t)strtoul(t1 + 1, NULL, 10);
        uint64_t k = mkkey(sum, idx);
        uint32_t i = hkey(k) & (g_tabcap - 1);
        while (g_tab[i].text && g_tab[i].key != k) i = (i + 1) & (g_tabcap - 1);
        if (!g_tab[i].text && n < (int)g_tabcap * 3 / 4) { g_tab[i].key = k; g_tab[i].text = _strdup(t2 + 1); n++; }
    }
    fclose(f);
    log_line("text table: %d entries", n);
}

/* ---------- what is on screen ---------- */
#define VCAP 128
typedef struct { uint64_t key; volatile DWORD last; DWORD first; int spoken; const char *text; } Vis;
static Vis g_vis[VCAP];
static volatile LONG g_vlock;
static volatile LONG g_unknown_logged;
static int g_debug_tex = 1;            /* log each new texture key (development) */
static uint32_t g_cur_azt, g_cur_sprite;

/* The last two known texts bound (newest first), for the skill list rows. */
static volatile uint64_t g_recent[2];

static void vis_note(uint32_t sum, uint32_t idx) {
    uint64_t k = mkkey(sum, idx);
    if (k != g_recent[0] && text_lookup(sum, idx)) { g_recent[1] = g_recent[0]; g_recent[0] = k; }
    DWORD now = GetTickCount();
    while (InterlockedExchange(&g_vlock, 1)) YieldProcessor();
    int freei = -1;
    for (int i = 0; i < VCAP; i++) {
        if (g_vis[i].key == k && g_vis[i].text) { g_vis[i].last = now; g_vlock = 0; return; }
        if (freei < 0 && !g_vis[i].text) freei = i;
    }
    const char *t = text_lookup(sum, idx);
    if (g_debug_tex && t && freei >= 0) log_line("tex %08x/%u azt %08x sprite %08x: %s", sum, idx, g_cur_azt, g_cur_sprite, t);
    if (t && freei >= 0) {
        g_vis[freei].key = k; g_vis[freei].last = now; g_vis[freei].first = now;
        g_vis[freei].spoken = 0; g_vis[freei].text = t;
    }
    g_vlock = 0;
}

static inline uint32_t r32(uint32_t a) { return _byteswap_ulong(*(uint32_t *)gptr(a)); }

/* Development: "texlog N" in cmd.txt logs every texture bound during the
 * next N frames-worth of binds, in order, with its key. */
volatile LONG g_texlog_left;
static void texlog(uint32_t sum, uint32_t idx, uint32_t sprite) {
    if (g_texlog_left > 0 && InterlockedDecrement(&g_texlog_left) >= 0) {
        const char *t = text_lookup(sum, idx);
        log_line("BIND %08x/%u spr %08x %s", sum, idx, sprite, t ? t : "");
    }
}

static guest_fn o_bind_tex;
static void h_bind_tex(PPCContext *c, uint8_t *b) {
    /* Same lookup the game does at the start of this function. */
    extern volatile LONG g_bind_calls, g_bind_sane;
    uint32_t azt = r32(0x8238ce10), id = (uint32_t)c->r5;
    InterlockedIncrement(&g_bind_calls);
    /* "#AZT", or "!AZT" once the game has loaded the pack's pictures (most
     * packs on screen; only a few stay "#"). */
    if (!azt || !guest_ok(azt, 0x20) || (r32(azt) & 0x00FFFFFFu) != 0x00415A54u) azt = 0;
    else InterlockedIncrement(&g_bind_sane);
    if (azt && id != 0xFFFFFFFFu) {
        uint32_t first = r32(azt + 0x18), tab = r32(0x8238ce14);
        if (tab && id - first < r32(azt + 0x10)) {
            uint32_t ent = azt + r32(tab + 4 * (id - first));
            g_cur_azt = azt;
            g_cur_sprite = (uint32_t)c->r3;
            texlog(r32(azt + 0x1c), r32(ent), (uint32_t)c->r4);
            vis_note(r32(azt + 0x1c), r32(ent));
        }
    }
    CALL_ORIG(o_bind_tex, c, b);
}

/* Packs whose texts a menu reader speaks itself (with position and price),
 * so the text reader stays quiet about them while that screen is open. */
static volatile uint32_t g_muted_pack;
static volatile int g_mute_all;
static volatile uint32_t g_muted_keep = 0xFFFFFFFFu;    /* one picture of the muted pack still read */
void text_mute_pack(uint32_t sum) { g_muted_pack = sum; g_muted_keep = 0xFFFFFFFFu; }
void text_mute_pack_except(uint32_t sum, uint32_t keep_idx) { g_muted_pack = sum; g_muted_keep = keep_idx; }
static volatile uint32_t g_muted_pack2;                 /* a second muted pack (Options screen) */
void text_mute_pack2(uint32_t sum) { g_muted_pack2 = sum; }
void text_mute_all(int on) { g_mute_all = on; }

/* Texts that are only control hints (the button bar) are not read
 * automatically; they would repeat on every screen. */
static int is_hint(const char *t) {
    return t[0] == ':' || !strncmp(t, ": ", 2);
}

/* ---------- skill list rows ----------
 * Command icons come from the atlas c8a71fa1/0. Each icon's sprite
 * descriptor has +0x2c = pointer to the row's command bytes and +0x3c = icon
 * number in the row. The texts bound just before a row's first icon are its
 * name and its cost or condition line. */
#define ICON_PACK 0xc8a71fa1u
typedef struct { uint64_t name, desc; uint32_t cmd; } SkillRow;
static SkillRow g_rows[16];
static volatile int g_nrows;
static volatile DWORD g_rows_seen;

static void row_from_icon(uint32_t d) {
    if (!guest_ok(d, 0x40) || g8(d + 0x3c) != 0) return;
    uint32_t cmd = g32(d + 0x2c);
    if (!cmd) return;
    DWORD now = GetTickCount();
    if (now - g_rows_seen > 500) g_nrows = 0;        /* list was closed: start over */
    g_rows_seen = now;
    for (int i = 0; i < g_nrows; i++) if (g_rows[i].cmd == cmd) return;
    if (g_nrows >= 16) return;
    g_rows[g_nrows].name = g_recent[1]; g_rows[g_nrows].desc = g_recent[0]; g_rows[g_nrows].cmd = cmd;
    g_nrows++;
}

static const char *k_cmd_words[] = {
    "back", "forward", "punch", "kick", "guard", "ki",
    "back punch", "back kick", "back guard", "forward punch", "forward kick", "forward guard",
    "back punch and kick", "back punch and guard", "back kick and guard",
    "forward punch and kick", "forward punch and guard", "forward kick and guard",
    "punch and kick", "punch and guard", "kick and guard", "punch, kick and guard",
    "punch, kick, guard and ki", "up or down and guard", "back ki", "forward ki", "punch, kick, guard and ki",
};

/* Number of skill rows on screen; fills *out with the spoken description of row i. */
int skill_rows(void) { return GetTickCount() - g_rows_seen < 300 ? g_nrows : 0; }
int skill_row_text(int i, char *out, int cap) {
    if (i < 0 || i >= g_nrows) return 0;
    SkillRow r = g_rows[i];
    const char *n = text_lookup((uint32_t)(r.name >> 32), (uint32_t)r.name);
    const char *d = text_lookup((uint32_t)(r.desc >> 32), (uint32_t)r.desc);
    int len = snprintf(out, cap, "%s. %s. ", n ? n : "Skill", d ? d : "");
    for (int k = 0; k < 7 && len < cap - 40; k++) {
        int c = g8(r.cmd + k);
        if (c == 0x1b) break;
        len += snprintf(out + len, cap - len, "%s%s", k ? ", " : "", c < (int)(sizeof k_cmd_words / sizeof k_cmd_words[0]) ? k_cmd_words[c] : "?");
    }
    return 1;
}

/* 820959b0: draw a 2D sprite. r7 = sprite descriptor, r8 = texture number in
 * the current pack. Development logging of descriptors ("sprlog N"). */
volatile LONG g_sprlog_left;
static guest_fn o_draw_sprite;
static void h_draw_sprite(PPCContext *c, uint8_t *b) {
    {
        uint32_t azt = r32(0x8238ce10);
        if (azt && r32(azt + 0x1c) == ICON_PACK && c->r8 == 0) row_from_icon((uint32_t)c->r7);
    }
    if (g_sprlog_left > 0 && InterlockedDecrement(&g_sprlog_left) >= 0) {
        uint32_t d = (uint32_t)c->r7, azt = r32(0x8238ce10);
        char hex[3 * 0x40 + 1] = "";
        if (guest_ok(d, 0x40)) for (int i = 0; i < 0x40; i++) sprintf(hex + 3 * i, "%02x ", g8(d + i));
        log_line("SPR pack %08x tex %d r6 %08x r9 %08x r10 %08x d %08x: %s", azt ? r32(azt + 0x1c) : 0,
                 (int)c->r8, (uint32_t)c->r6, (uint32_t)c->r9, (uint32_t)c->r10, d, hex);
    }
    CALL_ORIG(o_draw_sprite, c, b);
}

void text_init(void) {
    load_table();
    guest_hook(0x82096ff0, (void *)h_bind_tex, (void **)&o_bind_tex);
    guest_hook(0x820959b0, (void *)h_draw_sprite, (void **)&o_draw_sprite);
}

/* Called ~60 times a second from the mod thread. */
void text_poll(void) {
    DWORD now = GetTickCount();
    const char *say[16]; int nsay = 0;
    while (InterlockedExchange(&g_vlock, 1)) YieldProcessor();
    for (int i = 0; i < VCAP; i++) {
        Vis *v = &g_vis[i];
        if (!v->text) continue;
        if (now - v->last > 400) { v->text = NULL; v->key = 0; continue; }   /* gone from the screen */
        /* Wait until a text has been up for a moment so fades and flicker don't speak. */
        if (!v->spoken && now - v->first >= 120) {
            v->spoken = 1;
            int muted = ((uint32_t)(v->key >> 32) == g_muted_pack && (uint32_t)v->key != g_muted_keep)
                        || (g_muted_pack2 && (uint32_t)(v->key >> 32) == g_muted_pack2);
            if (!g_mute_all && !is_hint(v->text) && !muted && nsay < 16) say[nsay++] = v->text;
        }
    }
    g_vlock = 0;
    for (int i = 0; i < nsay; i++) speak_utf8(say[i], 0);
}

/* Was this text picture drawn in the last ms milliseconds? */
int text_visible_within(uint32_t sum, uint32_t idx, DWORD ms) {
    uint64_t k = mkkey(sum, idx);
    DWORD now = GetTickCount();
    int on = 0;
    while (InterlockedExchange(&g_vlock, 1)) YieldProcessor();
    for (int i = 0; i < VCAP; i++) if (g_vis[i].text && g_vis[i].key == k && now - g_vis[i].last < ms) { on = 1; break; }
    g_vlock = 0;
    return on;
}

/* Is this text picture on screen right now? (allows for a flicker) */
int text_visible(uint32_t sum, uint32_t idx) { return text_visible_within(sum, idx, 200); }

/* F2: read everything currently on screen again. */
void text_read_all(void) {
    DWORD now = GetTickCount();
    int any = 0;
    while (InterlockedExchange(&g_vlock, 1)) YieldProcessor();
    const char *say[VCAP]; int n = 0;
    for (int i = 0; i < VCAP; i++) if (g_vis[i].text && now - g_vis[i].last < 400) say[n++] = g_vis[i].text;
    g_vlock = 0;
    for (int i = 0; i < n; i++) { speak_utf8(say[i], !any); any = 1; }
    if (!any) speak(L"No text on screen", 1);
}
