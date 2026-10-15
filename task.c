#include <stdint.h>
#include "task.h"
#include "heap.h"
#include "string.h"
#include "timer.h"
#include "term.h"
#include "cpu.h"

#define STACK_SIZE 8192
#define IDLE_SLOT  1

enum { T_UNUSED = 0, T_READY, T_SLEEPING, T_ZOMBIE };

struct task {
    uint32_t esp;           // saved stack pointer while not running
    int      state;
    uint32_t wake_tick;
    uint32_t ticks;         // timer ticks this task was running for
    uint8_t *stack;
    task_fn  entry;
    void    *arg;
    char     name[16];
};

static struct task tasks[MAX_TASKS];
static struct task *current = 0;
static int enabled = 0;

extern void switch_task(uint32_t *old_esp, uint32_t new_esp);

// Pick the next task and switch to it. Must be called with interrupts disabled.
static void schedule(void) {
    struct task *prev = current;
    struct task *next = 0;
    uint32_t now = timer_ticks();
    int start = (int)(prev - tasks);

    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (idx == IDLE_SLOT) continue;               // idle only runs when nothing else can
        struct task *t = &tasks[idx];
        if (t->state == T_SLEEPING && (int32_t)(now - t->wake_tick) >= 0)
            t->state = T_READY;
        if (t->state == T_READY) { next = t; break; }
    }
    if (!next) next = &tasks[IDLE_SLOT];
    if (next == prev) return;

    current = next;
    switch_task(&prev->esp, next->esp);
}

// Every new task starts here (switch_task's `ret` lands on this function).
static void task_trampoline(void) {
    __asm__ volatile ("sti");                         // we arrive with interrupts off
    current->entry(current->arg);
    task_exit();
}

static void idle_task(void *arg) {
    (void)arg;
    for (;;) __asm__ volatile ("hlt");
}

int task_create(const char *name, task_fn fn, void *arg) {
    task_reap();
    uint8_t *stack = kmalloc(STACK_SIZE);
    if (!stack) return -1;

    uint32_t flags = irq_save();
    int id = -1;
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state == T_UNUSED) { id = i; break; }
    if (id < 0) {
        irq_restore(flags);
        kfree(stack);
        return -1;
    }

    struct task *t = &tasks[id];
    memset(t, 0, sizeof(*t));
    for (int i = 0; i < 15 && name[i]; i++) t->name[i] = name[i];
    t->stack = stack;
    t->entry = fn;
    t->arg   = arg;

    // Build the stack so switch_task's pops and `ret` start the trampoline.
    uint32_t *sp = (uint32_t *)(((uint32_t)stack + STACK_SIZE) & ~0xFu);
    *--sp = 0;                              // fake return address for the trampoline
    *--sp = (uint32_t)task_trampoline;      // popped by `ret`
    *--sp = 0;                              // ebp
    *--sp = 0;                              // ebx
    *--sp = 0;                              // esi
    *--sp = 0;                              // edi
    t->esp = (uint32_t)sp;
    t->state = T_READY;

    irq_restore(flags);
    return id;
}

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));

    // Slot 0 is the code that is already running (kernel_main and the shell),
    // on the original boot stack.
    strcpy(tasks[0].name, "shell");
    tasks[0].state = T_READY;
    current = &tasks[0];

    task_create("idle", idle_task, 0);      // lands in slot 1 = IDLE_SLOT
    enabled = 1;
}

void task_tick(void) {
    if (!enabled) return;
    current->ticks++;
    schedule();                             // interrupts are already off inside an IRQ
}

void task_yield(void) {
    uint32_t flags = irq_save();
    schedule();
    irq_restore(flags);
}

void task_sleep(uint32_t ticks) {
    uint32_t flags = irq_save();
    current->wake_tick = timer_ticks() + ticks;
    current->state = T_SLEEPING;
    schedule();
    irq_restore(flags);
}

void task_exit(void) {
    irq_save();
    current->state = T_ZOMBIE;
    schedule();                             // never returns to a zombie
    for (;;) __asm__ volatile ("hlt");
}

int task_kill(int id) {
    if (id <= IDLE_SLOT || id >= MAX_TASKS) return -1;   // can't kill the shell or idle
    uint32_t flags = irq_save();
    struct task *t = &tasks[id];
    if (t->state == T_UNUSED || t == current) {
        irq_restore(flags);
        return -1;
    }
    t->state = T_ZOMBIE;
    irq_restore(flags);
    return 0;
}

void task_reap(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        uint8_t *stack = 0;
        uint32_t flags = irq_save();
        struct task *t = &tasks[i];
        if (t->state == T_ZOMBIE && t != current) {
            stack = t->stack;
            memset(t, 0, sizeof(*t));       // slot is free again
        }
        irq_restore(flags);
        if (stack) kfree(stack);
    }
}

int task_current_id(void) {
    return (int)(current - tasks);
}

static void print_padded(const char *s, int width) {
    term_print(s);
    for (int l = (int)strlen(s); l < width; l++) term_putc(' ');
}

static void print_dec_padded(uint32_t n, int width) {
    int digits = 1;
    for (uint32_t m = n; m >= 10; m /= 10) digits++;
    term_print_dec(n);
    while (digits++ < width) term_putc(' ');
}

void task_list(void) {
    term_print("ID  STATE     TICKS     NAME\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        struct task *t = &tasks[i];
        if (t->state == T_UNUSED) continue;
        const char *st = (t == current)         ? "running"  :
                         (t->state == T_READY)  ? "ready"    :
                         (t->state == T_SLEEPING) ? "sleeping" : "zombie";
        print_dec_padded((uint32_t)i, 4);
        print_padded(st, 10);
        print_dec_padded(t->ticks, 10);
        term_print(t->name);
        term_putc('\n');
    }
}