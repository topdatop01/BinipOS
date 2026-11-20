#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "jbmono_font.h"
#include "cpu/idt.h"
#include "../drivers/ps2_kbd/keyboard.h"
#include "fses.h"


static struct limine_framebuffer *global_fb = NULL;
static uint64_t term_x = 40;
static uint64_t term_y = 40;
static uint32_t term_fg = 0xFFFFFF;
static uint32_t term_bg = 0x101418;


__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    __asm__ __volatile__ ("cli");
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}

// Decode a single UTF-8 sequence into a Unicode codepoint
static uint32_t utf8_next(const char **str) {
    const uint8_t *s = (const uint8_t *)(*str);
    uint32_t cp = 0;

    if (s[0] < 0x80) {
        cp = s[0];
        *str += 1;
    } else if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *str += 2;
    } else if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *str += 3;
    } else if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *str += 4;
    } else {
        cp = '?';
        *str += 1;
    }
    return cp;
}

// Binary search for glyph index in jbmono_codepoints
static int find_glyph(uint32_t cp) {
    int left = 0, right = JBMONO_COUNT - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (jbmono_codepoints[mid] == cp) return mid;
        if (jbmono_codepoints[mid] < cp) left = mid + 1;
        else right = mid - 1;
    }
    return -1;
}

// Alpha blend foreground and background colors
static uint32_t blend_color(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;
    uint32_t r = (((fg >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) >> 8;
    uint32_t g = (((fg >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) >> 8;
    uint32_t b = ((fg & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) >> 8;
    return (r << 16) | (g << 8) | b;
}

void draw_char_utf8(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t cp, uint32_t fg, uint32_t bg) {
    int idx = find_glyph(cp);
    if (idx < 0) idx = find_glyph('?');

    uint32_t *fb_ptr = (uint32_t *)fb->address;
    uint64_t stride = fb->pitch / 4;

    for (int row = 0; row < JBMONO_H; row++) {
        if (y + row >= fb->height) break;
        for (int col = 0; col < JBMONO_W; col++) {
            if (x + col >= fb->width) break;
            uint8_t alpha = jbmono_glyphs[idx][row][col];
            fb_ptr[(y + row) * stride + (x + col)] = blend_color(fg, bg, alpha);
        }
    }
}

void print_utf8(struct limine_framebuffer *fb, uint64_t x, uint64_t y, const char *str, uint32_t fg, uint32_t bg) {
    uint64_t cur_x = x;
    uint64_t cur_y = y;
    while (*str) {
        uint32_t cp = utf8_next(&str);
        if (cp == '\n') {
            cur_x = x;
            cur_y += JBMONO_H;
        } else {
            draw_char_utf8(fb, cur_x, cur_y, cp, fg, bg);
            cur_x += JBMONO_W;
        }
    }
}

void biots_input_char(char c) {
    if (global_fb == NULL) return;

    if (c == '\n') {
        term_x = 40;
        term_y += JBMONO_H;
    } else if (c == '\b') {
        if (term_x > 40) {
            term_x -= JBMONO_W;
            draw_char_utf8(global_fb, term_x, term_y, ' ', term_fg, term_bg);
        }
    } else {
        draw_char_utf8(global_fb, term_x, term_y, (uint32_t)c, term_fg, term_bg);
        term_x += JBMONO_W;
    }
}


void clear_screen(struct limine_framebuffer *fb, uint32_t color) {
    uint32_t *fb_ptr = (uint32_t *)fb->address;
    uint64_t stride = fb->pitch / 4;

    for (uint64_t y = 0; y < fb->height; y++) {
        for (uint64_t x = 0; x < fb->width; x++) {
            fb_ptr[y * stride + x] = color;
        }
    }
}

void _start(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        hcf();
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    global_fb = framebuffer_request.response->framebuffers[0];
    clear_screen(global_fb, term_bg);

    fses_init(global_fb);
    idt_init();
    keyboard_init(biots_input_char);

    print_utf8(global_fb, 40, 20, "BinipOS v0.1 • Type something:", term_fg, term_bg);



    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}
