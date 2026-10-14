/*
 * Button names: Xbox (the port's own) or PlayStation, for players with a
 * PlayStation controller. Shift+F12 switches; the choice is kept in
 * buttons.txt in the mod folder ("playstation" or "xbox").
 *
 * The game's texts name Xbox buttons ("Press START button", "Skills: X
 * button"), so with PlayStation names on, every spoken line is rewritten:
 * "<button> button" for the face, Start and Back buttons, and the shoulder
 * and trigger names LB RB LT RT (also "R B" / "L B") wherever they stand
 * alone. Readers that build their own lines name buttons with btn_name().
 */
#include "access.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>

volatile int g_ps_buttons;

typedef struct { const wchar_t *xbox, *ps; int need_button; } Btn;
static const Btn k_btn[] = {
    { L"A", L"Cross", 1 }, { L"B", L"Circle", 1 }, { L"X", L"Square", 1 }, { L"Y", L"Triangle", 1 },
    { L"START", L"OPTIONS", 1 }, { L"Start", L"Options", 1 }, { L"BACK", L"SHARE", 1 }, { L"Back", L"Share", 1 },
    { L"LB", L"L1", 0 }, { L"RB", L"R1", 0 }, { L"LT", L"L2", 0 }, { L"RT", L"R2", 0 },
    { L"L B", L"L1", 0 }, { L"R B", L"R1", 0 }, { L"L T", L"L2", 0 }, { L"R T", L"R2", 0 },
};
#define NBTN (int)(sizeof k_btn / sizeof k_btn[0])

static void path(wchar_t *p) { swprintf(p, MAX_PATH, L"%lsbuttons.txt", g_mod_dir); }

void buttons_load(void) {
    wchar_t p[MAX_PATH]; path(p);
    FILE *f = _wfopen(p, L"r");
    char line[32] = "";
    if (f) { if (!fgets(line, sizeof line, f)) line[0] = 0; fclose(f); }
    g_ps_buttons = !strncmp(line, "playstation", 11);
}

void buttons_toggle(void) {
    g_ps_buttons = !g_ps_buttons;
    wchar_t p[MAX_PATH]; path(p);
    FILE *f = _wfopen(p, L"w");
    if (f) { fputs(g_ps_buttons ? "playstation\n" : "xbox\n", f); fclose(f); }
    speak(g_ps_buttons ? L"PlayStation button names" : L"Xbox button names", 1);
}

/* The name to say for an Xbox button name ("A", "RB", "Start"...). */
const char *btn_name(const char *xbox) {
    static const struct { const char *x, *p; } k[] = {
        { "A", "Cross" }, { "B", "Circle" }, { "X", "Square" }, { "Y", "Triangle" },
        { "LB", "L1" }, { "RB", "R1" }, { "LT", "L2" }, { "RT", "R2" },
        { "L B", "L1" }, { "R B", "R1" }, { "L T", "L2" }, { "R T", "R2" },
        { "Start", "Options" }, { "Back", "Share" },
    };
    if (!g_ps_buttons) return xbox;
    for (unsigned i = 0; i < sizeof k / sizeof k[0]; i++) if (!strcmp(xbox, k[i].x)) return k[i].p;
    return xbox;
}

static int word_char(wchar_t c) { return iswalnum(c) || c == L'\''; }

/* PlayStation names in a spoken line; returns a new string (free it) or NULL if unchanged. */
wchar_t *buttons_rewrite(const wchar_t *in) {
    if (!g_ps_buttons || !in) return NULL;
    size_t n = wcslen(in), cap = n * 2 + 64, o = 0;
    wchar_t *out = malloc(cap * sizeof(wchar_t));
    if (!out) return NULL;
    int changed = 0;
    for (size_t i = 0; i < n;) {
        int hit = -1; size_t len = 0;
        if (i == 0 || !word_char(in[i - 1])) {
            for (int b = 0; b < NBTN; b++) {
                size_t l = wcslen(k_btn[b].xbox);
                if (wcsncmp(in + i, k_btn[b].xbox, l) || (i + l < n && word_char(in[i + l]))) continue;
                if (k_btn[b].need_button && _wcsnicmp(in + i + l, L" button", 7)) continue;
                hit = b; len = l; break;
            }
        }
        if (hit >= 0) {
            const wchar_t *r = k_btn[hit].ps; size_t rl = wcslen(r);
            if (o + rl + 1 >= cap) break;
            wmemcpy(out + o, r, rl); o += rl; i += len; changed = 1;
        } else {
            if (o + 2 >= cap) break;
            out[o++] = in[i++];
        }
    }
    out[o] = 0;
    if (!changed) { free(out); return NULL; }
    return out;
}
