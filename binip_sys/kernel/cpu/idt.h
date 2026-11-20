#pragma once
#include <stdint.h>

typedef void (*irq_handler_t)(void);

void idt_init(void);
void irq_install_handler(uint8_t irq, irq_handler_t handler);
