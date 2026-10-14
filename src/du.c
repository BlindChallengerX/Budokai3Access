/*
 * Dragon Universe: flying over the world map.
 *
 * Field work struct at 82479a60 (task "# FLIGHT #" while flying):
 *   +0x0b4 pointer to the story's location table
 *   +0x150 your position (x, height, z)
 *   +0x1c0 facing vector; on the map, heading = (x, -z)
 *   +0x1e0 markers on screen, +0x1e4 pointer to the marker Ki Search has
 *          picked; the on-screen markers are entries of 0x30 bytes from
 *          +0x1f0, location record at +0x20
 * The world wraps around: it is a square of (byte at *(82479770)+0x1ae0)
 * times 4000 units, 24000 in the US release.
 *
 * Location table: records of 0x40 bytes at +0x80, 64 of them:
 *   +0 type (16 bit) and sub, -1 = unused   +4 flags   +8 name ID
 *   +0xc radius   +0x10 x, y, z   +0x20 condition var/op/value
 *   +0x2c party member needed   +0x30 item needed   +0x34/+0x38 chapters
 * Flags & 3: Ki Search can pick it (821c2d00). The map (Y, 821c8268) draws
 * every active record without flags 0xc0 as the same red dot: those are the
 * story events. Places carry 0x40, hidden spots 0x80. The type is 5000 +
 * 100 * kind + character number (Goku 5400, 5700, 5000, 5300...), so it is no
 * test for a story event. The game's own check of the conditions is
 * 821c1c80; marker_active() does the same.
 *
 * Ki Search (RB) picks the active marker within 3000 units nearest the
 * middle of the screen; A then enters it. Name ID + 2 is the texture of the
 * place name in pack 26da3539 on Earth, bfd36483 on Namek.
 */
#include "access.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FIELD       0x82479a60u
#define VARS        0x8247a9b0u
#define WORLD_PTR   0x82479770u
#define PLACE_NAMES place_names()
#define EARTH_MAP_FILE 0x7b0u        /* data_cmn.afs file of Earth's map (5 MB) */
#define KI_RANGE    3000.0f

/* Earth and Namek each have their own list of place names (same numbering).
 * FIELD+0x28..+0x40 point to the loader records (+0x20) of the field's
 * files, in no fixed order; Earth's map, file 7b0, is among them on Earth.
 * Once the field has its files and that one is missing, it is Namek, unless
 * the game has drawn an Earth place name since you came into the field (the
 * list drawn from last is in text.c g_place_pack). */
static DWORD g_field_since;
static uint32_t place_names(void) {
    static uint32_t last;
    uint32_t pack = g_place_pack;
    int files = 0, earth = 0;
    for (uint32_t o = 0x28; o <= 0x40; o += 4) {
        uint32_t h = g32(FIELD + o);
        if (!(h >= 0x40000020u && h < 0x50000000u && guest_ok(h - 0x20, 0x20))) continue;
        files++;
        if (g32(h - 0x20 + 4) == EARTH_MAP_FILE) earth = 1;
    }
    if (earth) pack = PLACE_NAMES_EARTH;
    else if (files >= 5) {
        int earth_drawn = g_place_pack == PLACE_NAMES_EARTH && g_field_since && (LONG)(g_place_pack_t - g_field_since) > 0;
        pack = earth_drawn ? PLACE_NAMES_EARTH : PLACE_NAMES_NAMEK;
    }
    if (pack != last) { log_line("planet: %s (%d files%s)", pack == PLACE_NAMES_NAMEK ? "Namek" : "Earth", files, earth ? ", Earth map" : ""); last = pack; }
    return pack;
}

static uint32_t g_target;            /* record address of the guide target, 0 = none */
static uint32_t g_last_ks;           /* last marker Ki Search picked */
static int g_last_place = -1;        /* last place banner (place_here) */
static int g_was_field, g_in_range;
static DWORD g_next_beep;

/* Field menu (Start): one picture, texture 9 of pack 700efb0b, cursor at 82479b44. */
#define FMENU_PACK   0x700efb0bu
#define FMENU_CURSOR 0x82479b44u
static const char *k_fmenu[] = { "Edit Skills", "Check Status", "Save or Quit" };
static int g_fmenu_last = -1;

static int fmenu_open(void) {
    return task_running("# FLIGHT #") && text_visible(FMENU_PACK, 9) && !task_running("SEDIT 0P MAIN") && !task_running("# STATUS #");
}

static int fmenu_say(int interrupt) {
    uint32_t c = g32(FMENU_CURSOR);
    if (!fmenu_open() || c > 2) return 0;
    speakf(interrupt, "%s, %d of 3", k_fmenu[c], (int)c + 1);
    return 1;
}

static void fmenu_poll(void) {
    int c = fmenu_open() ? (int)g32(FMENU_CURSOR) : -1;
    if (c > 2) c = -1;
    if (c >= 0) text_mute_pack(FMENU_PACK);        /* its picture is in the table only so it can be seen */
    if (c != g_fmenu_last && c >= 0) {
        if (g_fmenu_last < 0) speak(L"Menu", 0);
        fmenu_say(g_fmenu_last < 0 ? 0 : 1);
    }
    g_fmenu_last = c;
}

static int32_t s32(uint32_t a) { return (int32_t)g32(a); }
static uint32_t table(void) { return g32(FIELD + 0xb4); }

int du_in_field(void) {
    return task_running("# FLIGHT #") && !task_running("Task_ExeEvt") && table() && !fmenu_open()
        && !task_running("SEDIT 0P MAIN") && !task_running("# STATUS #");
}

/* 821c1c80: is the record's condition met in the current story state? */
static int marker_active(uint32_t r) {
    uint32_t T = table();
    if (s32(r) == -1) return 0;
    int32_t v = s32(r + 0x20);
    if (v != -1) {
        int kind = (uint32_t)v >> 28, val;
        if (kind < 2) val = s32(VARS + (v & 0x0ffffffc));
        else if (kind == 2) val = s32(T + (v & 0x0ffffffc));
        else if (kind < 4) val = 0;
        else val = v;
        int op = s32(r + 0x24), ref = s32(r + 0x28), ok;
        switch (op) {
        case 0: ok = val == ref; break;
        case 1: ok = val != ref; break;
        case 2: ok = val <= ref; break;
        case 3: ok = val < ref; break;
        case 4: ok = val >= ref; break;
        case 5: ok = val > ref; break;
        default: ok = val != 0;
        }
        if (!ok) return 0;
    }
    int32_t who = s32(r + 0x2c);
    if (who != -1) {
        int found = 0;
        for (int i = 0; i < 7; i++) if ((int16_t)g16(T + 0x11f0 + 2 * i) == who) found = 1;
        if (!found) return 0;
    }
    int32_t it = s32(r + 0x30);
    if (it != -1 && g8(VARS + 0x2af19 + it) == 0) return 0;
    int ch = (int8_t)g8(T + 0x11e7);
    return s32(r + 0x34) <= ch && ch <= s32(r + 0x38);
}

/* Not a Ki Search marker: a "???" spot (Dragon Ball, capsule, zenie...).
 * Fly over it (within its radius) and press A. */
static int is_hidden(uint32_t r) { return !(g32(r + 4) & 3); }

/* A red dot on the game's map: a story event. */
static int is_story(uint32_t r) { uint32_t f = g32(r + 4); return (f & 3) && !(f & 0xc0); }

static void marker_name(uint32_t r, char *out, int cap) {
    /* Flags 0xe00 pick the object shown at the spot (821bd710); 0xa00 is
     * the blinking orb. The two spots with no object and flag 0x10 match
     * the guide's Dragon Ball locations for the first saga. */
    if (is_hidden(r)) {
        uint32_t f = g32(r + 4);
        snprintf(out, cap, "%s", !(f & 0xe00) && (f & 0x10) ? "Dragon Ball spot" : "Hidden spot");
        return;
    }
    const char *n = text_lookup(PLACE_NAMES, g32(r + 8) + 2);
    if (!n || !strcmp(n, "???")) n = "unknown place";
    if (is_story(r)) snprintf(out, cap, "Story, %s", n);
    else snprintf(out, cap, "%s", n);
}

/* Horizontal offset to a point, the short way round the wrapping world. */
static void offset_to(float tx, float tz, float *dx, float *dz) {
    float size = 4000.0f * g8(g32(WORLD_PTR) + 0x1ae0);
    if (size < 4000.0f) size = 24000.0f;
    float px = gf(FIELD + 0x150), pz = gf(FIELD + 0x158);
    *dx = tx - px; *dz = tz - pz;
    *dx -= size * floorf(*dx / size + 0.5f);
    *dz -= size * floorf(*dz / size + 0.5f);
}

/* Distance and angle to a record; angle in degrees, positive = to your right. */
static void bearing(uint32_t r, float *dist, float *ang) {
    float dx, dz;
    offset_to(gf(r + 0x10), gf(r + 0x18), &dx, &dz);
    float fx = gf(FIELD + 0x1c0), fz = -gf(FIELD + 0x1c8), fl = sqrtf(fx * fx + fz * fz);
    *dist = sqrtf(dx * dx + dz * dz);
    if (fl < 1e-4f) { *ang = 0; return; }
    fx /= fl; fz /= fl;
    float ahead = dx * fx + dz * fz, right = dx * -fz + dz * fx;
    *ang = atan2f(right, ahead) * 57.29578f;
}

static void direction_words(float ang, char *out, int cap) {
    int a = (int)(fabsf(ang) + 0.5f);
    if (a <= 10) snprintf(out, cap, "straight ahead");
    else if (a >= 170) snprintf(out, cap, "behind you");
    else snprintf(out, cap, "turn %s %d", ang > 0 ? "right" : "left", a);
}

/* Story events come before other markers; nearest first within each. */
static int marker_before(uint32_t a, uint32_t b) {
    int sa = is_story(a), sb = is_story(b);
    if (sa != sb) return sa;
    float da, db, ang;
    bearing(a, &da, &ang); bearing(b, &db, &ang);
    return da < db;
}

/* Active markers: story events first, then the rest, nearest first. */
static int markers(uint32_t *out, int cap) {
    uint32_t T = table();
    int n = 0;
    if (!T) return 0;
    for (int i = 0; i < 64 && n < cap; i++) {
        uint32_t r = T + 0x80 + 0x40 * i;
        if (guest_ok(r, 0x40) && marker_active(r)) out[n++] = r;
    }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            if (marker_before(out[j], out[j - 1])) { uint32_t t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; } else break;
        }
    return n;
}

/* Name ID of the place banner shown now (FIELD+0xdc, -1 = none). The game
 * (821c2b80) picks the nearest active record whose radius you are inside
 * and that is in view of the camera; A then enters it. 29 = "???". */
#define HIDDEN_NAME 29
static int place_here(void) { return s32(FIELD + 0xdc); }

static void describe(uint32_t r, int interrupt, const char *prefix) {
    char name[96], dir[48];
    float d, a;
    marker_name(r, name, sizeof name);
    bearing(r, &d, &a);
    direction_words(a, dir, sizeof dir);
    speakf(interrupt, "%s%s, %d units, %s", prefix, name, (int)(d + 0.5f), dir);
}

/* The nearest story event if there is one, otherwise the nearest marker. */
static uint32_t default_target(void) {
    uint32_t m[64];
    int n = markers(m, 64);
    return n ? m[0] : 0;
}

/* Says the guide target and, when the story branches (two or more red dots,
 * e.g. Z or GT after Kid Buu), names the other story events too. */
static void announce_guide(void) {
    uint32_t m[64];
    int n = markers(m, 64), story = 0;
    for (int i = 0; i < n; i++) if (is_story(m[i])) story++;
    if (story < 2) { describe(g_target, 0, "Guide: "); return; }
    char pre[48];
    snprintf(pre, sizeof pre, "%d story events. Guide: ", story);
    describe(g_target, 0, pre);
    for (int i = 0; i < story; i++) {
        if (m[i] == g_target) continue;
        char name[96];
        marker_name(m[i], name, sizeof name);
        speakf(0, "Also %s, %d of %d, F5 or R3 to choose", name, i + 1, n);
    }
}

static int target_valid(void) { return g_target && guest_ok(g_target, 0x40) && marker_active(g_target); }

/* F5 / Shift+F5 in the field: next / previous marker (story events first). */
void du_cycle(int dir) {
    uint32_t m[64];
    int n = markers(m, 64), cur = -1;
    if (!n) { speak(L"No markers", 1); return; }
    for (int i = 0; i < n; i++) if (m[i] == g_target) cur = i;
    cur = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    g_target = m[cur];
    g_in_range = 0;
    char pre[32];
    snprintf(pre, sizeof pre, "%d of %d, ", cur + 1, n);
    describe(g_target, 1, pre);
}

/* F6 in the field: where the guide target is. */
void du_describe(void) {
    if (!target_valid()) g_target = default_target();
    if (!g_target) { speak(L"No markers", 1); return; }
    describe(g_target, 1, "");
}

/* Dragon Balls held: bits 0..6 of the byte at table + 0x11e4 (the
 * character's story block). All 7 makes Shenron appear at the next story
 * event (script op 8215c1b0), which then clears it. -1 if unknown. */
static int balls_held(void) {
    uint32_t T = table();
    if (!T || !guest_ok(T + 0x11e4, 1)) return -1;
    int m = g8(T + 0x11e4) & 0x7f, n = 0;
    for (; m; m >>= 1) n += m & 1;
    return n;
}

/* F8 in the field: the place you are in and the markers in Ki Search range. */
void du_status(void) {
    char buf[600], name[96];
    int n = 0;
    int balls = balls_held();
    if (balls > 0) n += snprintf(buf + n, sizeof buf - n, "Dragon Balls: %d of 7. ", balls);
    int place = place_here();
    if (place == HIDDEN_NAME) n += snprintf(buf + n, sizeof buf - n, "At a hidden spot: R B for Ki Search, then A. ");
    else if (place >= 0 && text_lookup(PLACE_NAMES, place + 2))
        n += snprintf(buf + n, sizeof buf - n, "At %s. ", text_lookup(PLACE_NAMES, place + 2));
    uint32_t m[64];
    int k = markers(m, 64), nclose = 0;
    for (int i = 0; i < k; i++) {
        float d, a;
        bearing(m[i], &d, &a);
        if (d > KI_RANGE) break;
        marker_name(m[i], name, sizeof name);
        n += snprintf(buf + n, sizeof buf - n, "%s%s %d", nclose ? ", " : "Nearby: ", name, (int)(d + 0.5f));
        nclose++;
        if (n > (int)sizeof buf - 120) break;
    }
    if (!nclose) n += snprintf(buf + n, sizeof buf - n, "Nothing within 3000 units.");
    speak_utf8(buf, 1);
}

/* Guide beep: from the target's side, high when it is ahead and low when
 * behind, faster as you get closer. Kept above 330 Hz so "behind" is not
 * mistaken for the fights' low ring-edge thump. */
static void guide_beep(void) {
    extern volatile int g_beeps_on;
    if (!g_beeps_on || !target_valid() || !game_has_focus()) return;
    DWORD now = GetTickCount();
    if ((int)(now - g_next_beep) < 0) return;
    float d, a;
    bearing(g_target, &d, &a);
    float facing = cosf(a / 57.29578f);                     /* 1 ahead, -1 behind */
    float f = 330.0f * powf(3.0f, (facing + 1.0f) * 0.5f);  /* 330 .. 990 Hz */
    float pan = sinf(a / 57.29578f);
    if (fabsf(a) <= 10) f *= 1.25f;                          /* on course: brighter */
    fight_tone(2, f, f, 0.06f, 0.18f, pan);
    float nclose = d > 6000 ? 1.0f : d / 6000.0f;
    g_next_beep = now + 250 + (DWORD)(900 * nclose);
}

/* ---------- Battle Estimation (experience after a story battle) ----------
 * Result object: pointer at 8247a8c8. +0x2f8 base experience, +0x2d8 total,
 * +0x410 rows still to add (reaches 32 when done), +0x41c number of rows,
 * rows at +0x424 of 12 bytes: label, value, points. Label - 41 indexes the
 * names below (the labels are one picture, texture 6 of pack 70c29e9c). */
#define EST_PTR  0x8247a8c8u
#define EST_PACK 0x70c29e9cu
static const char *k_est[] = {
    "Total Damage", "Highest Combo", "Highest Combo Damage", "Remaining Health", "Remaining Ki",
    "Taunting", "Counters", "Dodges", "Teleports", "Ki Blasts Deflected", "Ki Blasts Returned",
    "Teleport Hits", "Battle Time", "Burst Wins", "Most Burst Mode Hits", "Throws", "Blocked Throws",
    "Quick Recovery", "Walls Hit", "Hyper Modes", "Full Charge Attacks", "First Attack",
    "Perfect Victory", "Finish Move", "No Guard", "No Teleporting", "No Dodging", "Ring out Victory",
    "No Backpedalling", "Unused Item Skills", "Story Reenacted", "Finish by Death move",
    "Finish by Dragon Rush", "Finish by Ultimate move",
};
#define NEST (int)(sizeof k_est / sizeof k_est[0])
static int g_est_said;

static int est_open(void) { return task_running("# RESULT #") && text_visible(EST_PACK, 4); }

static int est_say(int interrupt) {
    uint32_t o = g32(EST_PTR);
    if (!est_open() || !o || !guest_ok(o, 0x600)) return 0;
    int rows = s32(o + 0x41c);
    if (rows < 0 || rows > 40) return 0;
    char buf[1500];
    int n = snprintf(buf, sizeof buf, "Battle Estimation. Base experience %d. ", s32(o + 0x2f8));
    for (int i = 0; i < rows && n < (int)sizeof buf - 100; i++) {
        uint32_t r = o + 0x424 + 0xc * i;
        int label = s32(r) - 41, val = s32(r + 4), pts = s32(r + 8);
        if (val == 0 && pts == 0) continue;
        const char *name = label >= 0 && label < NEST ? k_est[label] : "Bonus";
        if (label >= 21) n += snprintf(buf + n, sizeof buf - n, "%s, %d point%s. ", name, pts, pts == 1 ? "" : "s");   /* achievements: no count */
        else n += snprintf(buf + n, sizeof buf - n, "%s %d, %d point%s. ", name, val, pts, pts == 1 ? "" : "s");
    }
    snprintf(buf + n, sizeof buf - n, "Total %d.", s32(o + 0x2d8));
    speak_utf8(buf, interrupt);
    return 1;
}

static void est_poll(void) {
    uint32_t o = est_open() ? g32(EST_PTR) : 0;
    if (!o) { g_est_said = 0; return; }
    text_mute_pack(EST_PACK);
    if (!g_est_said && s32(o + 0x410) >= 32) g_est_said = est_say(1);
}

/* ---------- Level up: spending Z points ----------
 * Same result object, once "# NEW BALL #" runs: +0x29c points added to each
 * of the 7 stats (32-bit each), +0x404 points remaining, +0x408 cursor.
 * Right on the stick adds a point, A finishes. After a battle the screen
 * runs under "# RESULT #"; Z points from a story event open it with
 * "Task_Level_UP" and "# STATUS #" instead. */
static const char *k_stats[] = { "Health", "Ki", "Attack", "Guard", "Arts", "Ability", "Com" };
static int g_lv_cur = -1, g_lv_left = -1, g_lv_add[7];

static uint32_t lv_obj(void) {
    uint32_t o = g32(EST_PTR);
    if (!(task_running("# RESULT #") || task_running("Task_Level_UP")) || !task_running("# NEW BALL #") || est_open()
        || !o || !guest_ok(o, 0x500)) return 0;
    return s32(o + 0x408) >= 0 && s32(o + 0x408) < 7 ? o : 0;
}

/* "Is this okay?" after A: texture 26 of pack 8d82d441, cursor +0x40c (0 Yes). */
#define LV_PACK 0x8d82d441u
static int lv_confirm(void) { return text_visible(LV_PACK, 26); }

static int lv_say(int interrupt, int all) {
    uint32_t o = lv_obj();
    if (!o) return 0;
    if (lv_confirm()) {
        speakf(interrupt, "%s", s32(o + 0x40c) == 0 ? "Yes" : "No");
        return 1;
    }
    int c = s32(o + 0x408), left = s32(o + 0x404);
    char buf[400];
    int n = snprintf(buf, sizeof buf, "%s plus %d, %d of 7. %d remaining", k_stats[c], s32(o + 0x29c + 4 * c), c + 1, left);
    if (all) {
        n += snprintf(buf + n, sizeof buf - n, ". Added:");
        for (int i = 0; i < 7; i++) n += snprintf(buf + n, sizeof buf - n, " %s %d%s", k_stats[i], s32(o + 0x29c + 4 * i), i < 6 ? "," : ".");
        snprintf(buf + n, sizeof buf - n, " Right adds a point, A finishes.");
    }
    speak_utf8(buf, interrupt);
    return 1;
}

static void lv_poll(void) {
    uint32_t o = lv_obj();
    if (!o) { g_lv_cur = -1; return; }
    int c = s32(o + 0x408), left = s32(o + 0x404), changed = 0;
    static int conf = -1;
    int cf = lv_confirm() ? s32(o + 0x40c) : -1;
    if (cf != conf && cf >= 0) speakf(conf < 0 ? 0 : 1, "%s", cf == 0 ? "Yes" : "No");
    conf = cf;
    if (cf >= 0) return;
    for (int i = 0; i < 7; i++) { int v = s32(o + 0x29c + 4 * i); if (v != g_lv_add[i]) changed = 1; g_lv_add[i] = v; }
    if (g_lv_cur < 0) { speakf(0, "Spend Z points: %d remaining. Up and down choose a stat, right adds a point, A finishes.", left); lv_say(0, 0); }
    else if (c != g_lv_cur || changed || left != g_lv_left) lv_say(1, 0);
    g_lv_cur = c; g_lv_left = left;
}

/* ---------- Zenie found ----------
 * Task "Task_EvtMoney" while the money picture is up; the event object
 * (pointer at 82439cc8) holds the amount at +0x104. Your Zenie are at
 * 824a5898; the task adds the amount when it closes (up to 990000). */
#define MONEY_EVT   0x82439cc8u
#define MONEY_TOTAL 0x824a5898u
static int g_money_on;
static void money_poll(void) {
    int on = task_running("Task_EvtMoney");
    if (on && !g_money_on) {
        uint32_t o = g32(MONEY_EVT);
        int amt = o ? s32(o + 0x104) : 0, total = s32(MONEY_TOTAL) + amt;
        if (total > 990000) total = 990000;
        if (amt > 0) speakf(0, "Received %d Zenie. You now have %d.", amt, total);
    }
    g_money_on = on;
}

/* ---------- Yes / No questions in story events ----------
 * "Save game?", "Save progress?", "Continue game?": pictures 7, 6, 8 of
 * pack 96b4052f with Yes (3) and No (4). The highlighted answer is byte
 * +0x87 of the event object (pointer at 82439cc8, as for Zenie): 0 Yes. */
#define EVQ_PACK 0x96b4052fu
static int g_evq = -1;

/* A save question opened from here sits on top of this one: it has its own reader. */
static int evq_open(void) { return text_visible(EVQ_PACK, 3) && text_visible(EVQ_PACK, 4) && !sysq_open(); }

static int evq_say(int interrupt, int title) {
    uint32_t o = g32(MONEY_EVT);
    if (!evq_open() || !o || !guest_ok(o + 0x87, 1)) return 0;
    int c = g8(o + 0x87);
    const char *q = text_visible(EVQ_PACK, 7) ? "Save game?" : text_visible(EVQ_PACK, 6) ? "Save progress?"
                  : text_visible(EVQ_PACK, 8) ? "Continue game?" : "";
    speakf(interrupt, "%s%s%s", title ? q : "", title && *q ? " " : "", c == 0 ? "Yes" : "No");
    g_evq = c;
    return 1;
}

static void evq_poll(void) {
    if (text_visible(EVQ_PACK, 3) && text_visible(EVQ_PACK, 4))
        text_mute_pack(EVQ_PACK);      /* this reader says the question and the answer (also behind a save question) */
    static DWORD since;
    if (!evq_open()) { g_evq = -1; since = 0; return; }
    if (!since) since = GetTickCount();
    uint32_t o = g32(MONEY_EVT);
    if (!o || !guest_ok(o + 0x87, 1)) return;
    int c = g8(o + 0x87);
    if (g_evq < 0) { if (GetTickCount() - since >= 300) evq_say(1, 1); }   /* the question picture comes a moment after Yes/No */
    else if (c != g_evq) evq_say(1, 0);
}

/* ---------- Check Status (field menu) ----------
 * "# STATUS #" without "# RESULT #": the same object as the level up
 * screen: +0x298 level, +0x29c the 7 stats, +0x2d4 experience. Experience
 * needed for each level: 32-bit table at 82374700, indexed by level. */
#define EXP_TABLE 0x82374700u
static int g_status_on;

static int status_say(int interrupt) {
    uint32_t o = g32(EST_PTR);
    if (!task_running("# STATUS #") || task_running("# RESULT #") || task_running("Task_Level_UP") || !o || !guest_ok(o, 0x400)) return 0;
    int lv = s32(o + 0x298), exp = s32(o + 0x2d4);
    char buf[400];
    int n = snprintf(buf, sizeof buf, "Level %d, experience %d", lv, exp);
    if (lv >= 0 && lv < 99) {
        int need = s32(EXP_TABLE + 4 * lv) - exp;
        if (need > 0) n += snprintf(buf + n, sizeof buf - n, ", %d to next level", need);
    }
    n += snprintf(buf + n, sizeof buf - n, ".");
    for (int i = 0; i < 7; i++) n += snprintf(buf + n, sizeof buf - n, " %s %d%s", k_stats[i], s32(o + 0x29c + 4 * i), i < 6 ? "," : ".");
    speak_utf8(buf, interrupt);
    return 1;
}

static void status_poll(void) {
    int on = task_running("# STATUS #") && !task_running("# RESULT #") && !task_running("Task_Level_UP");
    if (on && !g_status_on) g_status_on = status_say(0); else if (!on) g_status_on = 0;
}

/* ---------- Custom Tray (field menu, Edit Skills) ----------
 * Task "SEDIT 0P MAIN"; object at *(8247971c):
 *   +0x50 mode: 5 = on the tray, 6 = in the capsule list
 *   +0x64 tray cursor (1 = first slot)   +0x68 list tab (Ability, Equipment, Item)
 *   +0x90 + 0x1c * tab: that tab's list: +4 cursor, +8 capsule IDs (16 bit,
 *          -1 = none), +0x1a number of capsules
 *   +0x100 capsules in the tray, +0x104 their IDs (32 bit)
 * Capsule info: table at *(8237560c), 0x28 bytes per ID; byte +8 low 3
 * bits = slots it takes (the tray has 7). */
#define SEDIT_PTR   0x8247971cu
#define CAPS_INFO   0x8237560cu
#define TRAY_NAMES_PACK 0x07992cd0u       /* short capsule names drawn in the tray, by capsule ID */
static const char *k_tabs[] = { "Ability", "Equipment", "Item" };
extern const char *capsule_name(int id);
static int g_se_mode = -1, g_se_cur = -1, g_se_tab = -1, g_se_count = -1;

static int caps_cost(int id) {
    uint32_t t = g32(CAPS_INFO);
    return t && id > 0 && id < 0x253 ? (g8(t + id * 0x28 + 8) & 7) : 0;
}

static uint32_t sedit_obj(void) {
    uint32_t o = g32(SEDIT_PTR);
    return task_running("SEDIT 0P MAIN") && o && guest_ok(o, 0x200) ? o : 0;
}

static int tray_used(uint32_t o, int *slots) {
    int n = s32(o + 0x100), used = 0;
    if (n < 0 || n > 7) n = 0;
    for (int i = 0; i < n; i++) used += caps_cost(s32(o + 0x104 + 4 * i));
    *slots = used;
    return n;
}

static int in_tray(uint32_t o, int id) {
    int slots, n = tray_used(o, &slots);
    for (int i = 0; i < n; i++) if (s32(o + 0x104 + 4 * i) == id) return 1;
    return 0;
}

static void caps_words(int id, char *out, int cap) {
    int c = caps_cost(id);
    snprintf(out, cap, "%s, %d slot%s", capsule_name(id), c, c == 1 ? "" : "s");
}

static int sedit_say(int interrupt, int full) {
    uint32_t o = sedit_obj();
    if (!o) return 0;
    int mode = s32(o + 0x50), slots, n = tray_used(o, &slots);
    char buf[300], w[160];
    if (mode == 5) {
        int row = s32(o + 0x64) - 1, at = 0, id = -1;
        for (int i = 0; i < n; i++) {
            int c = caps_cost(s32(o + 0x104 + 4 * i));
            if (row >= at && row < at + (c ? c : 1)) { id = s32(o + 0x104 + 4 * i); break; }
            at += c ? c : 1;
        }
        if (id > 0) caps_words(id, w, sizeof w); else snprintf(w, sizeof w, "empty");
        int k = snprintf(buf, sizeof buf, "Slot %d: %s", row + 1, w);
        if (full) snprintf(buf + k, sizeof buf - k, ". %d of 7 slots used. A picks a capsule, B removes it, X shows its skill card, Y goes back.", slots);
        speak_utf8(buf, interrupt);
        return 1;
    }
    if (mode == 6) {
        int tab = s32(o + 0x68);
        if (tab < 0 || tab > 2) return 0;
        uint32_t L = o + 0x90 + 0x1c * tab;
        int cnt = (int16_t)g16(L + 0x1a), cur = s32(L + 4);
        int k = 0;
        if (full || tab != g_se_tab) k = snprintf(buf, sizeof buf, "%s, %d capsule%s. ", k_tabs[tab], cnt, cnt == 1 ? "" : "s");
        if (cnt <= 0) snprintf(buf + k, sizeof buf - k, "Empty. Left and right change the tab.");
        else {
            int id = (int16_t)g16(L + 8 + 2 * cur);
            caps_words(id, w, sizeof w);
            k += snprintf(buf + k, sizeof buf - k, "%s%s, %d of %d", w, in_tray(o, id) ? ", in tray" : "", cur + 1, cnt);
            if (full) snprintf(buf + k, sizeof buf - k, ". %d of 7 slots used. A puts it in the tray, X shows its skill card, left and right change the tab.", slots);
        }
        speak_utf8(buf, interrupt);
        return 1;
    }
    return 0;
}

static void sedit_poll(void) {
    uint32_t o = sedit_obj();
    if (!o) { g_se_mode = g_se_cur = g_se_tab = g_se_count = -1; return; }
    text_mute_pack(TRAY_NAMES_PACK);
    int sl, cnt = tray_used(o, &sl);
    if (g_se_count >= 0 && cnt != g_se_count)
        speakf(1, "%s. %d of 7 slots used", cnt > g_se_count ? "Added" : "Removed", sl);
    g_se_count = cnt;
    int mode = s32(o + 0x50), tab = s32(o + 0x68), cur;
    if (mode == 5) cur = s32(o + 0x64);
    else if (mode == 6 && tab >= 0 && tab <= 2) cur = s32(o + 0x90 + 0x1c * tab + 4);
    else cur = -1;
    if (mode != g_se_mode && (mode == 5 || mode == 6)) {
        if (g_se_mode < 0) { int sl; tray_used(o, &sl); speakf(0, "Custom tray, %d of 7 slots used", sl); }
        g_se_tab = -1;
        sedit_say(g_se_mode < 0 ? 0 : 1, 0);
    } else if ((mode == 5 || mode == 6) && (cur != g_se_cur || (mode == 6 && tab != g_se_tab))) sedit_say(1, 0);
    g_se_mode = mode; g_se_cur = cur; g_se_tab = mode == 6 ? tab : -1;
}

/* F5 with the field menu, Battle Estimation or level up open. */
int du_menu_read(void) { return evq_say(1, 1) || sedit_say(1, 1) || status_say(1) || fmenu_say(1) || est_say(1) || lv_say(1, 1); }

void du_poll(void) {
    fmenu_poll();
    money_poll();
    status_poll();
    sedit_poll();
    est_poll();
    lv_poll();
    evq_poll();
    /* The field must stay up a moment before the guide speaks: it flickers
     * on while an event or battle is starting. */
    int field = du_in_field();
    DWORD now = GetTickCount();
    /* A Dragon Ball picked up: say the count after Goku's line. */
    static int last_balls = -1;
    int balls = field ? balls_held() : -1;
    if (balls >= 0) {
        if (last_balls >= 0 && balls > last_balls)
            speakf(0, balls == 7 ? "All 7 Dragon Balls!" : "Dragon Balls: %d of 7", balls);
        last_balls = balls;
    } else if (!task_running("# FLIGHT #")) last_balls = -1;
    if (!field) { g_was_field = 0; g_field_since = now; return; }
    if (now - g_field_since < 1500) return;
    if (!g_was_field) {
        if (!target_valid()) g_target = default_target();
        g_last_ks = 0;
        if (g_target) announce_guide();
    }
    g_was_field = 1;
    if (g_target && !target_valid()) {
        g_target = default_target();
        if (g_target) announce_guide();
    }
    /* Ki Search pick: the banner names it; add the distance and what to do. */
    uint32_t e = g32(FIELD + 0x1e4), ks = e ? g32(e + 0x20) : 0;
    if (!task_running("# FILTER #")) ks = 0;
    if (ks && ks != g_last_ks) {
        float d, a;
        bearing(ks, &d, &a);
        char name[96];
        marker_name(ks, name, sizeof name);
        /* the game's banner reads the place name; hidden spots show "???" */
        if (is_hidden(ks)) speakf(0, "%s, %d units, A to take it", name, (int)(d + 0.5f));
        else speakf(0, "%d units, A to go there", (int)(d + 0.5f));
    }
    g_last_ks = ks;
    if (g_target) {
        float d, a;
        bearing(g_target, &d, &a);
        if (is_hidden(g_target)) {
            /* the banner check below speaks when A will work */
            float rad = gf(g_target + 0xc);
            if (d < rad && !g_in_range) speak(L"Near the hidden spot. Face it, press R B for Ki Search, then A", 0);
            if (d < rad) g_in_range = 1; else if (d > rad + 400) g_in_range = 0;
        } else {
            int in = d < KI_RANGE - 200;
            if (in && !g_in_range) speak(L"Guide target in Ki Search range, press R B and face it", 0);
            if (d > KI_RANGE) g_in_range = 0; else if (in) g_in_range = 1;
        }
    }
    /* A hidden spot's "???" banner is up: A picks it up. */
    int place = place_here();
    if (place != g_last_place && place == HIDDEN_NAME) speak(L"Hidden spot here. R B for Ki Search, then A", 0);
    g_last_place = place;
    guide_beep();
}
