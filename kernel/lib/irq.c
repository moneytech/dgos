#include "irq.h"

void *(*irq_handlers[73])(int, void*);
void (*irq_setmask)(int irq, int unmask);

void irq_hook(int irq, void *(*handler)(int, void*))
{
    irq_handlers[irq] = handler;
}

void irq_unhook(int irq, void *(*handler)(int, void*))
{
    if (irq_handlers[irq] == handler)
        irq_handlers[irq] = 0;
}

void *irq_invoke(int irq, void *ctx)
{
    if (irq_handlers[irq])
        return irq_handlers[irq](irq, ctx);
    return ctx;
}