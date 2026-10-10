/*
 * The game's task list. Budokai 3 runs each screen and system as a named
 * task ("CHARASEL" is character select, "# TOUR MODE #" the World
 * Tournament, ...). Task records are linked from the global at 8238cd88:
 *   +0x00 name (16 chars)   +0x1c task function   +0x28 next task
 *   +0x30 the task's work data
 * The mod walks the list every frame: tasks that appear or go away are logged
 * (that is how screens get identified), known screens are announced, and
 * screen readers look up their task's work data here.
 */
#include "access.h"
#include <string.h>

#define TASK_HEAD 0x8238cd88u
#define MAXT 128

typedef struct { char name[17]; uint32_t rec, work; } Task;
static Task g_cur[MAXT], g_prev[MAXT];
static int g_ncur, g_nprev;

static int find(const Task *l, int n, const char *name) {
    for (int i = 0; i < n; i++) if (!strcmp(l[i].name, name)) return i;
    return -1;
}

/* Work data of a running task, or 0. */
uint32_t task_work(const char *name) {
    int i = find(g_cur, g_ncur, name);
    return i >= 0 ? g_cur[i].work : 0;
}
int task_running(const char *name) { return find(g_cur, g_ncur, name) >= 0; }
/* Work data of every running task with this name (some screens run one per
 * player); returns how many. */
int task_works(const char *name, uint32_t *out, int max) {
    int n = 0;
    for (int i = 0; i < g_ncur && n < max; i++) if (!strcmp(g_cur[i].name, name)) out[n++] = g_cur[i].work;
    return n;
}

/* Address of a running task's record (some tasks keep their data inline
 * from +0x30 instead of behind a pointer), or 0. */
uint32_t task_rec(const char *name) {
    int i = find(g_cur, g_ncur, name);
    return i >= 0 ? g_cur[i].rec : 0;
}

/* Screens worth announcing when their task starts. */
static const struct { const char *task, *say; } k_screens[] = {
    { "CHARASEL", "Character select" },
};

void tasks_poll(void) {
    memcpy(g_prev, g_cur, sizeof g_cur); g_nprev = g_ncur;
    g_ncur = 0;
    uint32_t t = g32(TASK_HEAD);
    while (t && g_ncur < MAXT && guest_ok(t, 0x34)) {
        Task *k = &g_cur[g_ncur];
        memcpy(k->name, gptr(t), 16); k->name[16] = 0;
        for (int i = 0; i < 16; i++) if (k->name[i] && (k->name[i] < 32 || k->name[i] > 126)) { k->name[i] = 0; break; }
        k->rec = t; k->work = g32(t + 0x30);
        g_ncur++;
        if (!strcmp(k->name, "TCB Tail")) break;
        uint32_t nx = g32(t + 0x28);
        if (nx == t) break;
        t = nx;
    }
    if (!g_nprev) return;
    for (int i = 0; i < g_ncur; i++)
        if (find(g_prev, g_nprev, g_cur[i].name) < 0 && find(g_cur, i, g_cur[i].name) < 0) {
            log_line("task start: %s (work %08x)", g_cur[i].name, g_cur[i].work);
            for (unsigned s = 0; s < sizeof k_screens / sizeof k_screens[0]; s++)
                if (!strcmp(k_screens[s].task, g_cur[i].name)) speak_utf8(k_screens[s].say, 1);
        }
    for (int i = 0; i < g_nprev; i++)
        if (find(g_cur, g_ncur, g_prev[i].name) < 0) log_line("task end: %s", g_prev[i].name);
}

/* F11: list running tasks in the log (development). */
void tasks_dump(void) {
    for (int i = 0; i < g_ncur; i++) log_line("task %d: %s work %08x", i, g_cur[i].name, g_cur[i].work);
}
