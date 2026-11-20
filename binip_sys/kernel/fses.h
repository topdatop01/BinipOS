#pragma once
#include <stdint.h>
#include "limine.h"

struct cpu_registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

void fses_init(struct limine_framebuffer *fb);
void trigger_fses(const char *reason, struct cpu_registers *regs);
void exception_handler(struct cpu_registers *regs);
