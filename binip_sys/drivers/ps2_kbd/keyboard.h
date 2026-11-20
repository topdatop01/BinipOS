#pragma once
#include <stdint.h>

typedef void (*kbd_callback_t)(char c);

void keyboard_init(kbd_callback_t callback);
