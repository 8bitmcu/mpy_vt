#!/usr/bin/env python3

# This utility will convert a monospaced font for usage with mpy_vt

from PIL import Image, ImageFont, ImageDraw
import argparse

BPP = 1
CHARACTERS = ''.join(chr(c) for c in range(32, 127))
UNICODE_SYM = ''.join(chr(c) for c in (0x2500, 0x2502, 0x250c, 0x2510, 0x2514, 0x2518))


def encode_chars(characters, font, char_width, char_height, y_offset, bpp, sample_p, remap, palette):
    wide = (char_width * bpp + 7) // 8  # bytes per row (matches decoder's wide)
    row_stride = wide * 8               # bits per row after padding
    bitstring = ''
    for char in characters:
        char_im = Image.new('L', (char_width, char_height), 0)
        ImageDraw.Draw(char_im).text((0, y_offset), char, font=font, fill=255)
        char_im = char_im.quantize(palette=sample_p)
        char_im = char_im.point(remap)
        char_im.putpalette(palette)
        for y in range(char_im.height):
            row_bits = ''
            for x in range(char_im.width):
                color = char_im.getpixel((x, y))
                row_bits += ''.join(
                    '1' if (color & (1 << bit - 1)) else '0'
                    for bit in range(bpp, 0, -1)
                )
            bitstring += row_bits + '0' * (row_stride - len(row_bits))
    return bitstring


def emit_block(varname, bitstring):
    print(f'_{varname} =\\')
    print("    b'", end='')
    for i in range(0, len(bitstring), 8):
        if i and i % (16 * 8) == 0:
            print("'\\\n    b'", end='')
        print(f'\\x{int(bitstring[i:i+8], 2):02x}', end='')
    print("'\\\n    b''")
    print(f'\n{varname} = memoryview(_{varname})')


def main():
    parser = argparse.ArgumentParser(prog='fontconvert')
    parser.add_argument('font_file', help='Path to the regular font file')
    parser.add_argument('font_size', type=int, help='Font size to render')
    parser.add_argument('-b', '--bold', help='Optional path to the bold font file', default=None)
    parser.add_argument('-i', '--italic', help='Optional path to the italic font file', default=None)
    parser.add_argument('-bi', '--bold_italic', help='Optional path to the bold+italic font file', default=None)
    parser.add_argument('-u', '--unicode', action='store_true', help='Include unicode box drawing characters')
    parser.add_argument('--force-height', type=int, help='Force a specific pixel height boundary', default=None)
    parser.add_argument('--force-width', type=int, help='Force a specific pixel width boundary', default=None)
    args = parser.parse_args()

    bpp = BPP
    
    # Always lock grid metrics to the regular font face
    reg_font = ImageFont.truetype(args.font_file, args.font_size)

    _, top, _, bottom = reg_font.getbbox(CHARACTERS)

    char_height = args.force_height if args.force_height is not None else (bottom - top)
    y_offset = -top
    
    # Use forced width if provided, otherwise fall back to automated calculation
    char_width = args.force_width if args.force_width is not None else max(reg_font.getbbox(c)[2] for c in 'MW@#')

    try:
        _adaptive = Image.Palette.ADAPTIVE
    except AttributeError:
        _adaptive = Image.ADAPTIVE

    sample = Image.new('L', (char_width * len(CHARACTERS), char_height), 0)
    ImageDraw.Draw(sample).text((0, y_offset), CHARACTERS, font=reg_font, fill=255)
    sample_p = sample.convert(mode='P', palette=_adaptive, colors=1 << bpp)
    palette = sample_p.getpalette()

    # Ensure darkest entry = index 0
    n = 1 << bpp
    lum = [sum(palette[i*3:(i+1)*3]) for i in range(n)]
    order = sorted(range(n), key=lambda i: lum[i])
    remap = list(range(256))
    if order != list(range(n)):
        remap = [0] * 256
        for new, old in enumerate(order):
            remap[old] = new
        new_pal = palette[:]
        for new, old in enumerate(order):
            new_pal[new*3:(new+1)*3] = palette[old*3:(old+1)*3]
        palette = new_pal

    # Metadata Global Headers
    print(f'FIRST = {hex(ord(CHARACTERS[0]))}')
    print(f'LAST = {hex(ord(CHARACTERS[-1]))}')
    print(f'HEIGHT = {char_height}')
    print(f'WIDTH = {char_width}')
    print()

    # Emit REGULAR Font Block
    emit_block('REGULAR', encode_chars(CHARACTERS, reg_font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))

    # Emit BOLD Font Block
    if args.bold:
        bold_font = ImageFont.truetype(args.bold, args.font_size)
        print()
        emit_block('BOLD', encode_chars(CHARACTERS, bold_font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))
    else:
        print('\nBOLD = REGULAR')

    # Emit ITALICS Font Block
    if args.italic:
        italic_font = ImageFont.truetype(args.italic, args.font_size)
        print()
        emit_block('ITALICS', encode_chars(CHARACTERS, italic_font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))
    else:
        print('\nITALIC = REGULAR')

    # Emit BOLD_ITALICS Font Block
    if args.bold_italic:
        bi_font = ImageFont.truetype(args.bold_italic, args.font_size)
        print()
        emit_block('BOLD_ITALIC', encode_chars(CHARACTERS, bi_font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))
    else:
        # Cascades back elegantly to Bold (which itself cascades back to Regular if missing)
        print('\nBOLD_ITALIC = BOLD')

    # Emit UNICODE Block (Using Regular Font Metrics)
    if args.unicode:
        codepoints = ', '.join(hex(ord(c)) for c in UNICODE_SYM)
        print()
        print(f'UNICODE_CHARS = ({codepoints},)')
        print()
        emit_block('UNICODE_FONT', encode_chars(UNICODE_SYM, reg_font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))


if __name__ == '__main__':
    main()
