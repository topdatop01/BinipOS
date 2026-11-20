#include "keyboard.h"
#include "../../kernel/cpu/io.h"
#include "../../kernel/cpu/idt.h"
#include <stdbool.h>
#include <stddef.h>

extern void biots_switch_tty(uint8_t index);

static kbd_callback_t char_callback = NULL;
static bool shift_pressed = false;
static bool caps_lock = false;

static const char scancode_ascii_lower = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0
};

static const char scancode_ascii_upper = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0
};

static void keyboard_irq_handler(void) {
    uint8_t scancode = inb(0x60);

    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_pressed = false;
        }
    } else {
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
        } else if (scancode == 0x3A) {
            caps_lock = !caps_lock;
        } else if (scancode >= 0x3B && scancode <= 0x3E) {
            // F1..F4 -> switch virtual TTY 0..3
            biots_switch_tty(scancode - 0x3B);
        } else if (scancode < 128) {
            char c = shift_pressed ? scancode_ascii_upper[scancode]
                                   : scancode_ascii_lower[scancode];

            if (caps_lock && c >= 'a' && c <= 'z') {
                c -= 32;
            } else if (caps_lock && c >= 'A' && c <= 'Z' && shift_pressed) {
                c += 32;
            }

            if (c != 0 && char_callback != NULL) {
                char_callback(c);
            }
        }
    }
}

void keyboard_init(kbd_callback_t callback) {
    char_callback = callback;

    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    irq_install_handler(1, keyboard_irq_handler);
}
