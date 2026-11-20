#include "biots.h"
#include "kernel/jbmono_font.h"
#include <stdarg.h>

struct biots_cell {
    uint32_t cp;
    uint32_t fg;
    uint32_t bg;
};

struct biots_tty {
    struct biots_cell cells[64][160];
    uint64_t cur_col;
    uint64_t cur_row;
    uint32_t fg;
    uint32_t bg;
    char line_buf[256];
    volatile size_t line_len;
    volatile bool line_ready;
};

static struct limine_framebuffer *tty_fb = NULL;
static uint64_t cols = 0;
static uint64_t rows = 0;

static struct biots_tty ttys[4];
static uint8_t active_idx = 0;

static uint32_t utf8_next(const char **str) {
    const uint8_t *s = (const uint8_t *)(*str);
    uint8_t b0 = *(s + 0);
    uint8_t b1 = *(s + 1);
    uint8_t b2 = *(s + 2);
    uint8_t b3 = *(s + 3);
    uint32_t cp = 0;

    if (b0 < 0x80) {
        cp = b0;
        *str += 1;
    } else if ((b0 & 0xE0) == 0xC0 && (b1 & 0xC0) == 0x80) {
        cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        *str += 2;
    } else if ((b0 & 0xF0) == 0xE0 && (b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
        cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        *str += 3;
    } else if ((b0 & 0xF8) == 0xF0 && (b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
        cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        *str += 4;
    } else {
        cp = '?';
        *str += 1;
    }
    return cp;
}

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

static uint32_t blend_color(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;
    uint32_t r = (((fg >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) >> 8;
    uint32_t g = (((fg >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) >> 8;
    uint32_t b = ((fg & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) >> 8;
    return (r << 16) | (g << 8) | b;
}

static void render_cell(uint64_t col, uint64_t row, struct biots_cell cell) {
    if (tty_fb == NULL) return;

    int idx = find_glyph(cell.cp);
    if (idx < 0) idx = find_glyph('?');

    uint64_t px_x = col * JBMONO_W;
    uint64_t px_y = row * JBMONO_H;

    uint32_t *fb_ptr = (uint32_t *)tty_fb->address;
    uint64_t stride = tty_fb->pitch / 4;

    for (int y = 0; y < JBMONO_H; y++) {
        if (px_y + y >= tty_fb->height) break;
        for (int x = 0; x < JBMONO_W; x++) {
            if (px_x + x >= tty_fb->width) break;
            uint8_t alpha = jbmono_glyphs[idx][y][x];
            fb_ptr[(px_y + y) * stride + (px_x + x)] = blend_color(cell.fg, cell.bg, alpha);
        }
    }
}

static void redraw_active_tty(void) {
    if (tty_fb == NULL) return;
    struct biots_tty *t = &ttys[active_idx];

    for (uint64_t r = 0; r < rows; r++) {
        for (uint64_t c = 0; c < cols; c++) {
            render_cell(c, r, t->cells[r][c]);
        }
    }
}

static void scroll_up(struct biots_tty *t, bool is_active) {
    for (uint64_t r = 0; r < rows - 1; r++) {
        for (uint64_t c = 0; c < cols; c++) {
            t->cells[r][c] = t->cells[r + 1][c];
        }
    }
    for (uint64_t c = 0; c < cols; c++) {
        t->cells[rows - 1][c] = (struct biots_cell){ ' ', t->fg, t->bg };
    }
    if (is_active) {
        redraw_active_tty();
    }
}

static void newline(struct biots_tty *t, bool is_active) {
    t->cur_col = 0;
    t->cur_row++;
    if (t->cur_row >= rows) {
        scroll_up(t, is_active);
        t->cur_row = rows - 1;
    }
}

void biots_clear(void) {
    struct biots_tty *t = &ttys[active_idx];
    for (uint64_t r = 0; r < rows; r++) {
        for (uint64_t c = 0; c < cols; c++) {
            t->cells[r][c] = (struct biots_cell){ ' ', t->fg, t->bg };
        }
    }
    t->cur_col = 0;
    t->cur_row = 0;
    redraw_active_tty();
}

void biots_init(struct limine_framebuffer *fb, uint32_t fg, uint32_t bg) {
    tty_fb = fb;
    cols = fb->width / JBMONO_W;
    rows = fb->height / JBMONO_H;
    if (cols > 160) cols = 160;
    if (rows > 64) rows = 64;

    for (int i = 0; i < 4; i++) {
        ttys[i].fg = fg;
        ttys[i].bg = bg;
        ttys[i].cur_col = 0;
        ttys[i].cur_row = 0;
        ttys[i].line_len = 0;
        ttys[i].line_ready = false;
        for (uint64_t r = 0; r < rows; r++) {
            for (uint64_t c = 0; c < cols; c++) {
                ttys[i].cells[r][c] = (struct biots_cell){ ' ', fg, bg };
            }
        }
    }

    active_idx = 0;
    redraw_active_tty();
}

void biots_switch_tty(uint8_t index) {
    if (index >= 4 || index == active_idx) return;
    active_idx = index;
    redraw_active_tty();
}

uint8_t biots_active_tty(void) {
    return active_idx;
}

void biots_set_color(uint32_t fg, uint32_t bg) {
    ttys[active_idx].fg = fg;
    ttys[active_idx].bg = bg;
}

void biots_putchar(uint32_t cp) {
    if (tty_fb == NULL) return;
    struct biots_tty *t = &ttys[active_idx];

    if (cp == '\n') {
        newline(t, true);
    } else if (cp == '\r') {
        t->cur_col = 0;
    } else if (cp == '\t') {
        t->cur_col = (t->cur_col + 4) & ~3ULL;
        if (t->cur_col >= cols) newline(t, true);
    } else if (cp == '\b') {
        if (t->cur_col > 0) {
            t->cur_col--;
            t->cells[t->cur_row][t->cur_col] = (struct biots_cell){ ' ', t->fg, t->bg };
            render_cell(t->cur_col, t->cur_row, t->cells[t->cur_row][t->cur_col]);
        }
    } else {
        t->cells[t->cur_row][t->cur_col] = (struct biots_cell){ cp, t->fg, t->bg };
        render_cell(t->cur_col, t->cur_row, t->cells[t->cur_row][t->cur_col]);
        t->cur_col++;
        if (t->cur_col >= cols) {
            newline(t, true);
        }
    }
}

void biots_print(const char *str) {
    while (*str) {
        uint32_t cp = utf8_next(&str);
        biots_putchar(cp);
    }
}

static void print_uint(uint64_t val, int base, bool uppercase) {
    char buf[65];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (val == 0) {
        biots_putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = digits[val % base];
        val /= base;
    }
    while (i > 0) {
        biots_putchar(buf[--i]);
    }
}

static void print_int(int64_t val) {
    if (val < 0) {
        biots_putchar('-');
        val = -val;
    }
    print_uint((uint64_t)val, 10, false);
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    biots_print(s ? s : "(null)");
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    biots_putchar((uint32_t)c);
                    break;
                }
                case 'd':
                case 'i':
                    print_int(va_arg(args, int64_t));
                    break;
                case 'u':
                    print_uint(va_arg(args, uint64_t), 10, false);
                    break;
                case 'x':
                    print_uint(va_arg(args, uint64_t), 16, false);
                    break;
                case 'X':
                    print_uint(va_arg(args, uint64_t), 16, true);
                    break;
                case 'p':
                    biots_print("0x");
                    print_uint((uint64_t)va_arg(args, void *), 16, true);
                    break;
                case '%':
                    biots_putchar('%');
                    break;
                default:
                    biots_putchar('%');
                    biots_putchar(*fmt);
                    break;
            }
        } else {
            uint32_t cp = utf8_next(&fmt);
            biots_putchar(cp);
            continue;
        }
        fmt++;
    }

    va_end(args);
}

void biots_input_char(char c) {
    struct biots_tty *t = &ttys[active_idx];
    if (t->line_ready) return;

    if (c == '\n') {
        biots_putchar('\n');
        t->line_buf[t->line_len] = '\0';
        t->line_ready = true;
    } else if (c == '\b') {
        if (t->line_len > 0) {
            t->line_len--;
            biots_putchar('\b');
        }
    } else if (c >= 32 && t->line_len < 255) {
        t->line_buf[t->line_len++] = c;
        biots_putchar(c);
    }
}

size_t biots_readline(char *buf, size_t max_len) {
    while (!ttys[active_idx].line_ready) {
        __asm__ __volatile__ ("hlt");
    }

    struct biots_tty *t = &ttys[active_idx];
    size_t copy_len = (t->line_len < max_len - 1) ? t->line_len : (max_len - 1);
    for (size_t i = 0; i < copy_len; i++) {
        buf[i] = t->line_buf[i];
    }
    buf[copy_len] = '\0';

    t->line_len = 0;
    t->line_ready = false;
    return copy_len;
}
