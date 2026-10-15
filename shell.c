#include <stdint.h>
#include "shell.h"
#include "term.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"
#include "string.h"
#include "io.h"
#include "desktop.h"
#include "task.h"

#define LINE_MAX 128
#define MAX_ARGS 16

struct command {
    const char *name;
    const char *help;
    void (*fn)(int argc, char **argv);
};

static const struct command commands[];   // defined below, used by cmd_help

static int parse_uint(const char *s, uint32_t *out) {
    if (!*s) return 0;
    uint32_t v = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint32_t)(*s - '0');
        s++;
    }
    *out = v;
    return 1;
}

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; commands[i].name; i++) {
        term_print("  ");
        term_print(commands[i].name);
        int pad = 8 - (int)strlen(commands[i].name);
        while (pad-- > 0) term_putc(' ');
        term_print(commands[i].help);
        term_putc('\n');
    }
}

static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    term_clear();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) term_putc(' ');
        term_print(argv[i]);
    }
    term_putc('\n');
}

static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t total = pmm_total_pages();
    uint32_t free  = pmm_free_pages();
    term_print("Total: "); term_print_dec(total * 4); term_print(" KB (");
    term_print_dec(total / 256); term_print(" MB)\n");
    term_print("Free:  "); term_print_dec(free * 4);  term_print(" KB (");
    term_print_dec(free / 256);  term_print(" MB)\n");
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t secs = timer_ticks() / 100;    // timer runs at 100 Hz
    term_print("Up for "); term_print_dec(secs); term_print(" seconds\n");
}

static void cmd_heap(int argc, char **argv) {
    (void)argc; (void)argv;
    heap_dump();
}

static void cmd_color(int argc, char **argv) {
    uint32_t fg, bg = 0;
    if (argc < 2 || !parse_uint(argv[1], &fg) || fg > 15 ||
        (argc > 2 && (!parse_uint(argv[2], &bg) || bg > 15))) {
        term_print("usage: color <fg 0-15> [bg 0-15]\n");
        return;
    }
    term_set_color((uint8_t)fg, (uint8_t)bg);
}

static void cmd_ver(int argc, char **argv) {
    (void)argc; (void)argv;
    term_print("Danbo Desktop kernel v0.1 (x86, 32-bit)\n");
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    outb(0x64, 0xFE);                       // pulse the CPU reset line via the keyboard controller
    for (;;) __asm__ volatile ("hlt");
}

static void cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    term_print("System halted.\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

static void cmd_crash(int argc, char **argv) {
    (void)argc; (void)argv;
    volatile int zero = 0;
    volatile int x = 1 / zero;              // triggers a divide error
    (void)x;
}

static void cmd_gui(int argc, char **argv) {
    (void)argc; (void)argv;
    desktop_run();
}

#define VGA_TEXT ((volatile uint16_t *)0xB8000)

// Each spinner owns one cell in the top-right corner of the screen
static void spinner_task(void *arg) {
    uint32_t delay = (uint32_t)arg;
    int col = 79 - task_current_id();
    static const char frames[4] = { '|', '/', '-', '\\' };
    int f = 0;
    for (;;) {
        VGA_TEXT[col] = (uint16_t)(0x0E00 | (uint8_t)frames[f]);   // yellow on black
        f = (f + 1) & 3;
        task_sleep(delay);
    }
}

// Never sleeps: only preemption keeps the rest of the system alive
static void burn_task(void *arg) {
    (void)arg;
    volatile uint32_t n = 0;
    for (;;) n++;
}

static void cmd_spin(int argc, char **argv) {
    uint32_t delay = 10;
    if (argc > 1 && (!parse_uint(argv[1], &delay) || delay == 0)) {
        term_print("usage: spin [delay in ticks, 100 = 1s]\n");
        return;
    }
    int id = task_create("spin", spinner_task, (void *)delay);
    if (id < 0) { term_print("could not create task\n"); return; }
    term_print("started spinner, task "); term_print_dec((uint32_t)id); term_putc('\n');
}

static void cmd_burn(int argc, char **argv) {
    (void)argc; (void)argv;
    int id = task_create("burn", burn_task, 0);
    if (id < 0) { term_print("could not create task\n"); return; }
    term_print("started CPU hog, task "); term_print_dec((uint32_t)id); term_putc('\n');
}

static void cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    task_list();
}

static void cmd_kill(int argc, char **argv) {
    uint32_t id;
    if (argc < 2 || !parse_uint(argv[1], &id)) {
        term_print("usage: kill <task id>\n");
        return;
    }
    if (task_kill((int)id) != 0) {
        term_print("can't kill that task\n");
        return;
    }
    VGA_TEXT[79 - id] = 0x0F20;             // clear its spinner cell
    term_print("killed\n");
}

static void cmd_sleep(int argc, char **argv) {
    uint32_t secs;
    if (argc < 2 || !parse_uint(argv[1], &secs)) {
        term_print("usage: sleep <seconds>\n");
        return;
    }
    task_sleep(secs * 100);
}

static const struct command commands[] = {
    { "help",   "list commands",                cmd_help   },
    { "clear",  "clear the screen",             cmd_clear  },
    { "echo",   "print arguments",              cmd_echo   },
    { "mem",    "show physical memory usage",   cmd_mem    },
    { "uptime", "time since boot",              cmd_uptime },
    { "heap",   "dump kernel heap blocks",      cmd_heap   },
    { "color",  "set text colors (0-15)",       cmd_color  },
    { "ver",    "show kernel version",          cmd_ver    },
    { "crash",  "trigger a divide-by-zero",     cmd_crash  },
    { "reboot", "restart the machine",          cmd_reboot },
    { "halt",   "stop the CPU",                 cmd_halt   },
    { "gui",    "start the graphical desktop",  cmd_gui    },
    { "spin",   "start a background spinner",   cmd_spin   },
    { "burn",   "start a CPU-hogging task",     cmd_burn   },
    { "ps",     "list tasks",                   cmd_ps     },
    { "kill",   "kill a task by id",            cmd_kill   },
    { "sleep",  "sleep for N seconds",          cmd_sleep  },
    { 0, 0, 0 }
};

static void execute(char *line) {
    char *argv[MAX_ARGS];
    int argc = 0;

    char *p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') *p++ = 0;         // turn spaces into terminators
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
    }
    if (argc == 0) return;

    for (int i = 0; commands[i].name; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }
    term_print("Unknown command: ");
    term_print(argv[0]);
    term_print("  (try 'help')\n");
}

void shell_run(void) {
    char line[LINE_MAX];

    for (;;) {
        task_reap();
        term_set_color(11, 0);              // light cyan prompt
        term_print("danbo> ");
        term_set_color(15, 0);

        uint32_t len = 0;
        for (;;) {
            char c = keyboard_getchar();
            if (c == '\n') {
                term_putc('\n');
                break;
            }
            if (c == '\b') {
                if (len > 0) { len--; term_putc('\b'); }
                continue;
            }
            if (c >= 32 && c < 127 && len < LINE_MAX - 1) {
                line[len++] = c;
                term_putc(c);
            }
        }
        line[len] = 0;
        execute(line);
    }
}