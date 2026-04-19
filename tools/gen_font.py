#!/usr/bin/env python3
"""Generate FONT.TGA for Flicky's Flock custom sprite font.

Creates a 512x8 pixel, 32-bit BGRA uncompressed TGA containing
64 characters (ASCII 32-95), each 8x8 pixels.

Each character is a 5x7 pixel body with 1px dark outline, in yellow
body color matching the game's existing title screen sprite style.

Usage: python gen_font.py [output_path]
Default output: ../Flickys-Flock-Netlink/cd/TEX/FONT.TGA
"""

import struct
import sys
import os

# Colors in BGRA format (TGA native byte order)
TRANSPARENT   = (0, 0, 0, 0)
BODY_COLOR    = (9, 239, 239, 255)      # bright yellow (RGB 239,239,9)
OUTLINE_COLOR = (12, 12, 12, 255)       # near-black outline

IMG_W = 512  # 64 chars * 8 pixels each
IMG_H = 8

# Character body definitions: ASCII 32-95 (64 chars)
# Each character is 7 rows of 5-char strings ('#' = body, '.' = empty)
# Characters are placed at (col+1, row) in the 8x8 cell
CHARS = [
    # 32: SPACE
    [".....",
     ".....",
     ".....",
     ".....",
     ".....",
     ".....",
     "....."],
    # 33: !
    ["..#..",
     "..#..",
     "..#..",
     "..#..",
     ".....",
     "..#..",
     "....."],
    # 34: "
    [".#.#.",
     ".#.#.",
     ".....",
     ".....",
     ".....",
     ".....",
     "....."],
    # 35: #
    [".#.#.",
     "#####",
     ".#.#.",
     "#####",
     ".#.#.",
     ".....",
     "....."],
    # 36: $
    ["..#..",
     ".####",
     "#.#..",
     ".###.",
     "..#.#",
     "####.",
     "..#.."],
    # 37: %
    ["##...",
     "##.#.",
     "..#..",
     ".#...",
     "#.##.",
     "..##.",
     "....."],
    # 38: &
    [".##..",
     "#..#.",
     ".##..",
     ".#...",
     "#.#.#",
     "#..#.",
     ".##.#"],
    # 39: '
    ["..#..",
     "..#..",
     ".....",
     ".....",
     ".....",
     ".....",
     "....."],
    # 40: (
    ["..#..",
     ".#...",
     "#....",
     "#....",
     "#....",
     ".#...",
     "..#.."],
    # 41: )
    ["..#..",
     "...#.",
     "....#",
     "....#",
     "....#",
     "...#.",
     "..#.."],
    # 42: *
    [".....",
     "..#..",
     "#.#.#",
     ".###.",
     "#.#.#",
     "..#..",
     "....."],
    # 43: +
    [".....",
     "..#..",
     "..#..",
     "#####",
     "..#..",
     "..#..",
     "....."],
    # 44: ,
    [".....",
     ".....",
     ".....",
     ".....",
     ".##..",
     "..#..",
     ".#..."],
    # 45: -
    [".....",
     ".....",
     ".....",
     "#####",
     ".....",
     ".....",
     "....."],
    # 46: .
    [".....",
     ".....",
     ".....",
     ".....",
     ".....",
     ".##..",
     ".##.."],
    # 47: /
    ["....#",
     "...#.",
     "..#..",
     ".#...",
     "#....",
     ".....",
     "....."],
    # 48: 0
    [".###.",
     "#...#",
     "#..##",
     "#.#.#",
     "##..#",
     "#...#",
     ".###."],
    # 49: 1
    ["..#..",
     ".##..",
     "..#..",
     "..#..",
     "..#..",
     "..#..",
     ".###."],
    # 50: 2
    [".###.",
     "#...#",
     "....#",
     "..##.",
     ".#...",
     "#....",
     "#####"],
    # 51: 3
    [".###.",
     "#...#",
     "....#",
     "..##.",
     "....#",
     "#...#",
     ".###."],
    # 52: 4
    ["...#.",
     "..##.",
     ".#.#.",
     "#..#.",
     "#####",
     "...#.",
     "...#."],
    # 53: 5
    ["#####",
     "#....",
     "####.",
     "....#",
     "....#",
     "#...#",
     ".###."],
    # 54: 6
    ["..##.",
     ".#...",
     "#....",
     "####.",
     "#...#",
     "#...#",
     ".###."],
    # 55: 7
    ["#####",
     "....#",
     "...#.",
     "..#..",
     ".#...",
     ".#...",
     ".#..."],
    # 56: 8
    [".###.",
     "#...#",
     "#...#",
     ".###.",
     "#...#",
     "#...#",
     ".###."],
    # 57: 9
    [".###.",
     "#...#",
     "#...#",
     ".####",
     "....#",
     "...#.",
     ".##.."],
    # 58: :
    [".....",
     ".##..",
     ".##..",
     ".....",
     ".##..",
     ".##..",
     "....."],
    # 59: ;
    [".....",
     ".##..",
     ".##..",
     ".....",
     ".##..",
     "..#..",
     ".#..."],
    # 60: <
    ["...#.",
     "..#..",
     ".#...",
     "#....",
     ".#...",
     "..#..",
     "...#."],
    # 61: =
    [".....",
     ".....",
     "#####",
     ".....",
     "#####",
     ".....",
     "....."],
    # 62: >
    [".#...",
     "..#..",
     "...#.",
     "....#",
     "...#.",
     "..#..",
     ".#..."],
    # 63: ?
    [".###.",
     "#...#",
     "....#",
     "..##.",
     "..#..",
     ".....",
     "..#.."],
    # 64: @
    [".###.",
     "#...#",
     "#.###",
     "#.#.#",
     "#.##.",
     "#....",
     ".###."],
    # 65: A
    [".###.",
     "#...#",
     "#...#",
     "#####",
     "#...#",
     "#...#",
     "#...#"],
    # 66: B
    ["####.",
     "#...#",
     "#...#",
     "####.",
     "#...#",
     "#...#",
     "####."],
    # 67: C
    [".###.",
     "#...#",
     "#....",
     "#....",
     "#....",
     "#...#",
     ".###."],
    # 68: D
    ["####.",
     "#...#",
     "#...#",
     "#...#",
     "#...#",
     "#...#",
     "####."],
    # 69: E
    ["#####",
     "#....",
     "#....",
     "###..",
     "#....",
     "#....",
     "#####"],
    # 70: F
    ["#####",
     "#....",
     "#....",
     "###..",
     "#....",
     "#....",
     "#...."],
    # 71: G
    [".###.",
     "#...#",
     "#....",
     "#.###",
     "#...#",
     "#...#",
     ".####"],
    # 72: H
    ["#...#",
     "#...#",
     "#...#",
     "#####",
     "#...#",
     "#...#",
     "#...#"],
    # 73: I
    [".###.",
     "..#..",
     "..#..",
     "..#..",
     "..#..",
     "..#..",
     ".###."],
    # 74: J
    ["..###",
     "...#.",
     "...#.",
     "...#.",
     "...#.",
     "#..#.",
     ".##.."],
    # 75: K
    ["#...#",
     "#..#.",
     "#.#..",
     "##...",
     "#.#..",
     "#..#.",
     "#...#"],
    # 76: L
    ["#....",
     "#....",
     "#....",
     "#....",
     "#....",
     "#....",
     "#####"],
    # 77: M
    ["#...#",
     "##.##",
     "#.#.#",
     "#.#.#",
     "#...#",
     "#...#",
     "#...#"],
    # 78: N
    ["#...#",
     "##..#",
     "#.#.#",
     "#..##",
     "#...#",
     "#...#",
     "#...#"],
    # 79: O
    [".###.",
     "#...#",
     "#...#",
     "#...#",
     "#...#",
     "#...#",
     ".###."],
    # 80: P
    ["####.",
     "#...#",
     "#...#",
     "####.",
     "#....",
     "#....",
     "#...."],
    # 81: Q
    [".###.",
     "#...#",
     "#...#",
     "#...#",
     "#.#.#",
     "#..#.",
     ".##.#"],
    # 82: R
    ["####.",
     "#...#",
     "#...#",
     "####.",
     "#.#..",
     "#..#.",
     "#...#"],
    # 83: S
    [".###.",
     "#...#",
     "#....",
     ".###.",
     "....#",
     "#...#",
     ".###."],
    # 84: T
    ["#####",
     "..#..",
     "..#..",
     "..#..",
     "..#..",
     "..#..",
     "..#.."],
    # 85: U
    ["#...#",
     "#...#",
     "#...#",
     "#...#",
     "#...#",
     "#...#",
     ".###."],
    # 86: V
    ["#...#",
     "#...#",
     "#...#",
     "#...#",
     ".#.#.",
     ".#.#.",
     "..#.."],
    # 87: W
    ["#...#",
     "#...#",
     "#...#",
     "#.#.#",
     "#.#.#",
     "##.##",
     "#...#"],
    # 88: X
    ["#...#",
     "#...#",
     ".#.#.",
     "..#..",
     ".#.#.",
     "#...#",
     "#...#"],
    # 89: Y
    ["#...#",
     "#...#",
     ".#.#.",
     "..#..",
     "..#..",
     "..#..",
     "..#.."],
    # 90: Z
    ["#####",
     "....#",
     "...#.",
     "..#..",
     ".#...",
     "#....",
     "#####"],
    # 91: [
    [".###.",
     ".#...",
     ".#...",
     ".#...",
     ".#...",
     ".#...",
     ".###."],
    # 92: backslash
    ["#....",
     ".#...",
     "..#..",
     "...#.",
     "....#",
     ".....",
     "....."],
    # 93: ]
    [".###.",
     "...#.",
     "...#.",
     "...#.",
     "...#.",
     "...#.",
     ".###."],
    # 94: ^
    ["..#..",
     ".#.#.",
     "#...#",
     ".....",
     ".....",
     ".....",
     "....."],
    # 95: _
    [".....",
     ".....",
     ".....",
     ".....",
     ".....",
     ".....",
     "#####"],
]

def generate_font():
    """Generate the font TGA image."""
    # Create pixel grid (row 0 = top of image)
    pixels = [[TRANSPARENT] * IMG_W for _ in range(IMG_H)]

    # Place body pixels for each character
    for ci, char_data in enumerate(CHARS):
        cell_x = ci * 8  # left edge of this char's 8x8 cell

        for row in range(7):
            row_str = char_data[row]
            for col in range(5):
                if col < len(row_str) and row_str[col] == '#':
                    # Body pixel at (cell_x + col + 1, row)
                    px = cell_x + col + 1
                    py = row
                    if 0 <= px < IMG_W and 0 <= py < IMG_H:
                        pixels[py][px] = BODY_COLOR

    # Add outline pixels (8-connected neighbors of body pixels)
    outline_pixels = []
    for y in range(IMG_H):
        for x in range(IMG_W):
            if pixels[y][x] == TRANSPARENT:
                # Check all 8 neighbors
                has_body_neighbor = False
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dy == 0 and dx == 0:
                            continue
                        ny, nx = y + dy, x + dx
                        if 0 <= ny < IMG_H and 0 <= nx < IMG_W:
                            if pixels[ny][nx] == BODY_COLOR:
                                has_body_neighbor = True
                                break
                    if has_body_neighbor:
                        break
                if has_body_neighbor:
                    # Don't outline into column 7 of any cell (spacing column)
                    col_in_cell = x % 8
                    if col_in_cell < 7:
                        outline_pixels.append((y, x))

    for y, x in outline_pixels:
        pixels[y][x] = OUTLINE_COLOR

    # Write TGA file (bottom-to-top row order)
    header = struct.pack('<BBBHHBHHHHBB',
        0,      # id_length
        0,      # color_map_type
        2,      # image_type (uncompressed true-color)
        0, 0,   # color_map_spec (offset, length)
        0,      # color_map_entry_size
        0, 0,   # x_origin, y_origin
        IMG_W,  # width
        IMG_H,  # height
        32,     # pixel_depth (32 bits = BGRA)
        8,      # image_descriptor (8 alpha bits, bottom-to-top)
    )

    pixel_data = bytearray()
    # TGA bottom-to-top: row IMG_H-1 first, row 0 last
    for y in range(IMG_H - 1, -1, -1):
        for x in range(IMG_W):
            b, g, r, a = pixels[y][x]
            pixel_data.extend([b, g, r, a])

    return header + bytes(pixel_data)


def main():
    if len(sys.argv) > 1:
        out_path = sys.argv[1]
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        out_path = os.path.join(script_dir, "..", "Flickys-Flock-Netlink",
                                "cd", "TEX", "FONT.TGA")

    tga_data = generate_font()

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(tga_data)

    print(f"Generated {out_path} ({len(tga_data)} bytes)")
    print(f"Image: {IMG_W}x{IMG_H}, 32bpp BGRA, {len(CHARS)} characters")


if __name__ == '__main__':
    main()
