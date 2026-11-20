#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../kernel/limine.h"

void biots_init(struct limine_framebuffer *fb, uint32_t fg, uint32_t bg);
void biots_switch_tty(uint8_t index);
uint8_t biots_active_tty(void);

void biots_clear(void);
void biots_set_color(uint32_t fg, uint32_t bg);

void biots_putchar(uint32_t cp);
void biots_print(const char *str);
void kprintf(const char *fmt, ...);

void biots_input_char(char c);
size_t biots_readline(char *buf, size_t max_len);
