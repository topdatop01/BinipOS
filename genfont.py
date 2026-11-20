from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf"
FONT_SIZE = 16
CELL_W, CELL_H = 10, 20

# Basic ASCII (0x20..0x7E) + Cyrillic (U+0410..U+044F, Yo/yo)
chars = set(chr(c) for c in range(32, 127))
chars.update(chr(c) for c in range(0x0410, 0x0450))
chars.update(['Ё', 'ё'])

# Box-drawing characters and block elements for TUI/frames
chars.update(chr(c) for c in range(0x2500, 0x2580))
chars.update(['█', '▓', '▒', '░', '▀', '▄', '▌', '▐'])

# Common symbols, currencies, arrows, and math operators
extra = (
    "«»—–…№₽€£¥¢©®™°±×÷≠≤≥≈∞√•§¶†‡"
    "←↑→↓↔↕↖↗↘↙⇐⇒⇔"
    "▶◀▲▼►◄■□▪▫◆◇○●✓✗✔✘★☆⚡⚙"
)
chars.update(extra)

# Sort by Unicode codepoint for binary search in kernel
sorted_chars = sorted(chars, key=ord)

font = ImageFont.truetype(FONT_PATH, FONT_SIZE)

with open("binip_sys/kernel/jbmono_font.h", "w", encoding="utf-8") as f:
    f.write("#pragma once\n#include <stdint.h>\n\n")
    f.write(f"#define JBMONO_W {CELL_W}\n#define JBMONO_H {CELL_H}\n")
    f.write(f"#define JBMONO_COUNT {len(sorted_chars)}\n\n")

    f.write("static const uint32_t jbmono_codepoints[JBMONO_COUNT] = {\n    ")
    f.write(", ".join(f"0x{ord(c):04X}" for c in sorted_chars))
    f.write("\n};\n\n")

    f.write(f"static const uint8_t jbmono_glyphs[JBMONO_COUNT][{CELL_H}][{CELL_W}] = {{\n")
    for c in sorted_chars:
        img = Image.new("L", (CELL_W, CELL_H), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 1), c, font=font, fill=255)
        f.write(f"    {{ // U+{ord(c):04X}\n")
        for y in range(CELL_H):
            row = ", ".join(f"{img.getpixel((x, y)):3d}" for x in range(CELL_W))
            f.write(f"        {{{row}}},\n")
        f.write("    },\n")
    f.write("};\n")

print(f"Generated {len(sorted_chars)} glyphs successfully!")
