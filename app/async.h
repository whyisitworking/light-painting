#ifndef ASYNC_H
#define ASYNC_H

#include <pico/critical_section.h>
#include <pico/types.h>

#define synchronized(stmt)                                                     \
    do {                                                                       \
        uint32_t saved_irq = save_and_disable_interrupts();                    \
        stmt;                                                                  \
        restore_interrupts(saved_irq);                                         \
    } while (0)

#endif