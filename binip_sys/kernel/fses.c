#include "fses.h"
#include <stddef.h>

extern void clear_screen(struct limine_framebuffer *fb, uint32_t color);
extern void print_utf8(struct limine_framebuffer *fb, uint64_t x, uint64_t y, const char *str, uint32_t fg, uint32_t bg);

static struct limine_framebuffer *fses_fb = NULL;

static const char *exception_names[] = {
    "Division By Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt (NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)"
};

static void uint64_to_hex(uint64_t val, char *buf) {
    const char *hex = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[17 - i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[18] = '\0';
}

void fses_init(struct limine_framebuffer *fb) {
    fses_fb = fb;
}

void trigger_fses(const char *reason, struct cpu_registers *regs) {
    __asm__ __volatile__ ("cli");

    if (fses_fb != NULL) {
        uint32_t bg = 0x00008B; // Deep red background
        uint32_t fg = 0xFFFFFF; // White text
        uint32_t hl = 0xFFD700; // Gold highlight for labels

        clear_screen(fses_fb, bg);

        print_utf8(fses_fb, 40, 40,
            "\n"
            "                 \\ /     #######                      \n"
            "                  X      #      #                     \n"
            "                 / \\     #      #                     \n"
            "                         #######                      \n"
            "                         #                            \n"
            "                 \\ /     #                            \n"
            "                  X      #                            \n"
            "                 / \\     #                            \n"
            "                                                      \n"
            "                FATAL SYSTEM ERROR SCREEN\n"
            "        BinipOS encountered an unrecoverable error\n"
            "\n\n\n\n",
            fg, bg);

        print_utf8(fses_fb, 40, 340, "Reason: ", hl, bg);
        print_utf8(fses_fb, 120, 340, reason, fg, bg);

        if (regs != NULL) {
            char hex_buf[19];

            uint64_to_hex(regs->rip, hex_buf);
            print_utf8(fses_fb, 40, 380, "RIP: ", hl, bg);
            print_utf8(fses_fb, 90, 380, hex_buf, fg, bg);

            uint64_to_hex(regs->rsp, hex_buf);
            print_utf8(fses_fb, 40, 405, "RSP: ", hl, bg);
            print_utf8(fses_fb, 90, 405, hex_buf, fg, bg);

            uint64_to_hex(regs->err_code, hex_buf);
            print_utf8(fses_fb, 40, 430, "ERR: ", hl, bg);
            print_utf8(fses_fb, 90, 430, hex_buf, fg, bg);
        }

        print_utf8(fses_fb, 40, 580, "System halted. Please reboot your machine.", 0xCCCCCC, bg);
    }

    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}

void exception_handler(struct cpu_registers *regs) {
    const char *reason = "Unknown CPU Exception";
    if (regs->vector < 22) {
        reason = exception_names[regs->vector];
    }
    trigger_fses(reason, regs);
}
