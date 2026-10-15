#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 16

typedef void (*task_fn)(void *arg);

void task_init(void);
int  task_create(const char *name, task_fn fn, void *arg);   // returns the task id, or -1
void task_yield(void);
void task_sleep(uint32_t ticks);                             // 100 ticks = 1 second
void task_exit(void);
int  task_kill(int id);                                      // 0 = ok, -1 = failed
int  task_current_id(void);
void task_reap(void);                                        // free the stacks of finished tasks
void task_list(void);                                        // print a table (for `ps`)
void task_tick(void);                                        // called by the timer interrupt

#endif