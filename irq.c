#include "irq.h"
#include "pic.h"

static irq_handler_t handlers[16];

void irq_install_handler(int irq, irq_handler_t handler) {
    handlers[irq] = handler;
    pic_unmask(irq);
}

void irq_dispatch(struct registers *r) {
    int irq = r->int_no - 32;
    pic_send_eoi(irq);              // acknowledge first: the handler may switch tasks
    if (handlers[irq])
        handlers[irq](r);
}