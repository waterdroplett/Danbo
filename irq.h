#ifndef IRQ_H
#define IRQ_H

#include "idt.h"

typedef void (*irq_handler_t)(struct registers *);

void irq_install_handler(int irq, irq_handler_t handler);
void irq_dispatch(struct registers *r);

#endif