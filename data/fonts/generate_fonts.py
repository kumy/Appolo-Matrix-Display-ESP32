#!/usr/bin/env python3
"""Generates .font binary files for the matrix-display-2026 firmware.

File format (see include/display/Font.h for the loader):
  byte 0: magic 'F' (0x46)
  byte 1: version (1)
  byte 2: glyph width px
  byte 3: glyph height px
  byte 4: char pitch (width + spacing)
  byte 5: line pitch (height + spacing)
  byte 6: first char code
  byte 7: char count
  byte 8+: charCount * height bytes; each byte's low `width` bits used,
           MSB = leftmost column (bit (width-1-col)).

Fonts:
- classic3x5: the exact glyph set already hand-authored in
  src/display/Renderer.cpp, extracted verbatim (not retyped) as the
  built-in fallback's on-disk twin.
- wide6x10: 2x nearest-neighbor upscale of classic3x5.
- led7x5, dotmatrix7x4, dot8x8: original dot-matrix-style digit/
  punctuation sets, hand-authored at each target size (NOT traced from
  any specific commercial font file — no access to fetch/parse those).
  Letters (A-Z) for these three are derived from classic3x5 via
  nearest-neighbor scaling to fill out the character set, since
  hand-authoring 3 complete 26-letter sets was out of scope for the time
  available; digits/punctuation (what Clock/Countdown pages actually
  need) are hand-designed for each size.
- ledfull16: full-height (16px) bold font, upscaled from led7x5's
  hand-authored digits (not independently hand-authored) for the same
  time-budget reason.
"""

FIRST_CHAR = 0x20  # space
LAST_CHAR = 0x5A   # 'Z'

GLYPHS_3X5 = {
    ' ': [0, 0, 0, 0, 0],
    ':': [0, 2, 0, 2, 0],
    '-': [0, 0, 7, 0, 0],
    '/': [1, 1, 2, 4, 4],
    '.': [0, 0, 0, 0, 2],
    '0': [7, 5, 5, 5, 7],
    '1': [2, 6, 2, 2, 7],
    '2': [7, 1, 7, 4, 7],
    '3': [7, 1, 7, 1, 7],
    '4': [5, 5, 7, 1, 1],
    '5': [7, 4, 7, 1, 7],
    '6': [7, 4, 7, 5, 7],
    '7': [7, 1, 1, 1, 1],
    '8': [7, 5, 7, 5, 7],
    '9': [7, 5, 7, 1, 7],
    'A': [7, 5, 7, 5, 5],
    'B': [6, 5, 6, 5, 6],
    'C': [7, 4, 4, 4, 7],
    'D': [6, 5, 5, 5, 6],
    'E': [7, 4, 7, 4, 7],
    'F': [7, 4, 7, 4, 4],
    'G': [7, 4, 5, 5, 7],
    'H': [5, 5, 7, 5, 5],
    'I': [7, 2, 2, 2, 7],
    'J': [1, 1, 1, 5, 7],
    'K': [5, 5, 6, 5, 5],
    'L': [4, 4, 4, 4, 7],
    'M': [5, 7, 7, 5, 5],
    'N': [5, 7, 7, 7, 5],
    'O': [7, 5, 5, 5, 7],
    'P': [7, 5, 7, 4, 4],
    'Q': [7, 5, 5, 7, 1],
    'R': [7, 5, 7, 6, 5],
    'S': [7, 4, 7, 1, 7],
    'T': [7, 2, 2, 2, 2],
    'U': [5, 5, 5, 5, 7],
    'V': [5, 5, 5, 5, 2],
    'W': [5, 5, 7, 7, 5],
    'X': [5, 5, 2, 5, 5],
    'Y': [5, 5, 2, 2, 2],
    'Z': [7, 1, 2, 4, 7],
}

# --- led7x5: 5 wide x 7 tall, classic LED dot-matrix digit proportions ---
GLYPHS_LED7X5 = {
    ' ': [0b00000] * 7,
    ':': [0b00000, 0b00100, 0b00000, 0b00000, 0b00100, 0b00000, 0b00000],
    '-': [0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000],
    '/': [0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000],
    '.': [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00100],
    '0': [0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110],
    '1': [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    '2': [0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111],
    '3': [0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110],
    '4': [0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010],
    '5': [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],
    '6': [0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
    '7': [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000],
    '8': [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],
    '9': [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100],
}

# --- dotmatrix7x4: 4 wide x 7 tall, compact ---
GLYPHS_DOT7X4 = {
    ' ': [0b0000] * 7,
    ':': [0b0000, 0b0100, 0b0000, 0b0000, 0b0100, 0b0000, 0b0000],
    '-': [0b0000, 0b0000, 0b0000, 0b1111, 0b0000, 0b0000, 0b0000],
    '/': [0b0001, 0b0001, 0b0010, 0b0100, 0b1000, 0b1000, 0b1000],
    '.': [0b0000, 0b0000, 0b0000, 0b0000, 0b0000, 0b0000, 0b0100],
    '0': [0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110],
    '1': [0b0010, 0b0110, 0b0010, 0b0010, 0b0010, 0b0010, 0b0111],
    '2': [0b0110, 0b1001, 0b0001, 0b0010, 0b0100, 0b1000, 0b1111],
    '3': [0b1111, 0b0001, 0b0010, 0b0001, 0b0001, 0b1001, 0b0110],
    '4': [0b0010, 0b0110, 0b1010, 0b1010, 0b1111, 0b0010, 0b0010],
    '5': [0b1111, 0b1000, 0b1110, 0b0001, 0b0001, 0b1001, 0b0110],
    '6': [0b0011, 0b0100, 0b1000, 0b1110, 0b1001, 0b1001, 0b0110],
    '7': [0b1111, 0b0001, 0b0010, 0b0010, 0b0100, 0b0100, 0b0100],
    '8': [0b0110, 0b1001, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110],
    '9': [0b0110, 0b1001, 0b1001, 0b0111, 0b0001, 0b0010, 0b1100],
}

# --- dot8x8: 8 wide x 8 tall, square dot font ---
GLYPHS_DOT8X8 = {
    ' ': [0b00000000] * 8,
    ':': [0b00000000, 0b00011000, 0b00011000, 0b00000000, 0b00011000, 0b00011000, 0b00000000, 0b00000000],
    '-': [0b00000000, 0b00000000, 0b00000000, 0b01111110, 0b00000000, 0b00000000, 0b00000000, 0b00000000],
    '/': [0b00000010, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b01000000],
    '.': [0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00011000, 0b00011000, 0b00000000],
    '0': [0b00111100, 0b01000010, 0b01000110, 0b01001010, 0b01010010, 0b01100010, 0b01000010, 0b00111100],
    '1': [0b00001000, 0b00011000, 0b00101000, 0b00001000, 0b00001000, 0b00001000, 0b00001000, 0b00111110],
    '2': [0b00111100, 0b01000010, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01111110],
    '3': [0b01111110, 0b00000100, 0b00001000, 0b00000100, 0b00000010, 0b01000010, 0b00111100, 0b00000000],
    '4': [0b00000100, 0b00001100, 0b00010100, 0b00100100, 0b01111110, 0b00000100, 0b00000100, 0b00000000],
    '5': [0b01111110, 0b01000000, 0b01111100, 0b00000010, 0b00000010, 0b01000010, 0b00111100, 0b00000000],
    '6': [0b00011100, 0b00100000, 0b01000000, 0b01111100, 0b01000010, 0b01000010, 0b00111100, 0b00000000],
    '7': [0b01111110, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00010000, 0b00010000, 0b00000000],
    '8': [0b00111100, 0b01000010, 0b01000010, 0b00111100, 0b01000010, 0b01000010, 0b00111100, 0b00000000],
    '9': [0b00111100, 0b01000010, 0b01000010, 0b00111110, 0b00000010, 0b00000100, 0b00111000, 0b00000000],
}

FONT_SPECS = {
    'led7x5': {'width': 5, 'height': 7, 'char_pitch': 6, 'line_pitch': 8, 'glyphs': GLYPHS_LED7X5},
    'dotmatrix7x4': {'width': 4, 'height': 7, 'char_pitch': 5, 'line_pitch': 8, 'glyphs': GLYPHS_DOT7X4},
    'dot8x8': {'width': 8, 'height': 8, 'char_pitch': 9, 'line_pitch': 9, 'glyphs': GLYPHS_DOT8X8},
}


def unpack_row(value, width):
    return [(value >> (width - 1 - c)) & 1 for c in range(width)]


def pack_row(bits):
    value = 0
    for i, b in enumerate(bits):
        if b:
            value |= 1 << (len(bits) - 1 - i)
    return value


def scale_glyph(rows, src_w, src_h, dst_w, dst_h):
    """Nearest-neighbor scale a glyph from src_w x src_h to dst_w x dst_h."""
    out = []
    for dy in range(dst_h):
        sy = min(src_h - 1, (dy * src_h) // dst_h)
        src_bits = unpack_row(rows[sy], src_w)
        dst_bits = []
        for dx in range(dst_w):
            sx = min(src_w - 1, (dx * src_w) // dst_w)
            dst_bits.append(src_bits[sx])
        out.append(pack_row(dst_bits))
    return out


def build_font_bytes(glyph_width, glyph_height, char_pitch, line_pitch, glyph_lookup):
    char_count = LAST_CHAR - FIRST_CHAR + 1
    header = bytes([0x46, 1, glyph_width, glyph_height, char_pitch, line_pitch, FIRST_CHAR, char_count])
    body = bytearray()
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        ch = chr(code)
        rows = glyph_lookup.get(ch, [0] * glyph_height)
        body.extend(rows)
    return header + bytes(body)


def fill_letters_from_base(glyphs, target_w, target_h):
    """Derives A-Z (and any other missing entries) via nearest-neighbor
    scale from the 3x5 base font, leaving hand-authored entries alone."""
    filled = dict(glyphs)
    for ch, base_rows in GLYPHS_3X5.items():
        if ch in filled:
            continue
        filled[ch] = scale_glyph(base_rows, 3, 5, target_w, target_h)
    return filled


def main():
    outputs = {}

    outputs['classic3x5.font'] = build_font_bytes(3, 5, 4, 6, GLYPHS_3X5)

    wide_glyphs = {ch: scale_glyph(rows, 3, 5, 6, 10) for ch, rows in GLYPHS_3X5.items()}
    outputs['wide6x10.font'] = build_font_bytes(6, 10, 7, 11, wide_glyphs)

    for name, spec in FONT_SPECS.items():
        full_glyphs = fill_letters_from_base(spec['glyphs'], spec['width'], spec['height'])
        outputs[f'{name}.font'] = build_font_bytes(
            spec['width'], spec['height'], spec['char_pitch'], spec['line_pitch'], full_glyphs)

    # ledfull16: full 16px-tall bold font, upscaled from led7x5's
    # hand-authored digits/punctuation (5x7 -> 8x16), letters filled from
    # the 3x5 base directly at the same target size. Width capped at 8 so
    # each row still packs into a single byte (the .font row format is
    # one byte per row, low `width` bits used).
    led16_glyphs = {}
    for ch, rows in GLYPHS_LED7X5.items():
        led16_glyphs[ch] = scale_glyph(rows, 5, 7, 8, 16)
    led16_glyphs = fill_letters_from_base(led16_glyphs, 8, 16)
    outputs['ledfull16.font'] = build_font_bytes(8, 16, 9, 17, led16_glyphs)

    for filename, data in outputs.items():
        with open(filename, 'wb') as f:
            f.write(data)
        print(f"{filename}: {len(data)} bytes")


if __name__ == '__main__':
    main()
