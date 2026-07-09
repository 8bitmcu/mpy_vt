#!/usr/bin/env python3

# This utility will convert a monospaced font for usage with mpy_vt

from PIL import Image, ImageFont, ImageDraw
import argparse

BPP = 1
CHARACTERS = ''.join(chr(c) for c in range(32, 127))
BOX_DRAWING = ''.join(chr(c) for c in range(0x2500, 0x2519))  # ─ to └┘


def encode_chars(characters, font, char_width, char_height, y_offset, bpp, sample_p, remap, palette):
    bitstring = ''
    for char in characters:
        char_im = Image.new('L', (char_width, char_height), 0)
        ImageDraw.Draw(char_im).text((0, y_offset), char, font=font, fill=255)
        char_im = char_im.quantize(palette=sample_p)
        char_im = char_im.point(remap)
        char_im.putpalette(palette)
        for y in range(char_im.height):
            for x in range(char_im.width):
                color = char_im.getpixel((x, y))
                bitstring += ''.join(
                    '1' if (color & (1 << bit - 1)) else '0'
                    for bit in range(bpp, 0, -1)
                )
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
    parser = argparse.ArgumentParser(prog='font2bitmap')
    parser.add_argument('font_file')
    parser.add_argument('font_size', type=int)
    parser.add_argument('-b', '--box-drawing', action='store_true')
    args = parser.parse_args()

    bpp = BPP
    font = ImageFont.truetype(args.font_file, args.font_size)

    _, top, _, bottom = font.getbbox(CHARACTERS)
    char_height = bottom - top
    y_offset = -top
    char_width = max(font.getbbox(c)[2] for c in 'MW@#')

    try:
        _adaptive = Image.Palette.ADAPTIVE
    except AttributeError:
        _adaptive = Image.ADAPTIVE

    sample = Image.new('L', (char_width * len(CHARACTERS), char_height), 0)
    ImageDraw.Draw(sample).text((0, y_offset), CHARACTERS, font=font, fill=255)
    sample_p = sample.convert(mode='P', palette=_adaptive, colors=1 << bpp)
    palette = sample_p.getpalette()

    # ensure darkest entry = index 0
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

    print(f'FIRST = {hex(ord(CHARACTERS[0]))}')
    print(f'LAST = {hex(ord(CHARACTERS[-1]))}')
    print(f'HEIGHT = {char_height}')
    print(f'WIDTH = {char_width}')
    print()
    emit_block('FONT', encode_chars(CHARACTERS, font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))

    if args.box_drawing:
        print()
        print(f'UNICODEBOX_FIRST = {hex(ord(BOX_DRAWING[0]))}')
        print(f'UNICODEBOX_LAST = {hex(ord(BOX_DRAWING[-1]))}')
        print()
        emit_block('UNICODEBOX_FONT', encode_chars(BOX_DRAWING, font, char_width, char_height, y_offset, bpp, sample_p, remap, palette))


main()
