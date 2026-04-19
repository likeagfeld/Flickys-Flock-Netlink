#pragma once
/*
 * font.h - Custom sprite font for online screens
 *
 * Provides styled text rendering using VDP1 sprites instead of
 * Jo Engine's VDP2 jo_printf. Each character is an 8x8 sprite
 * tile loaded from FONT.TGA, matching the title screen text style.
 *
 * Covers ASCII 32-95: space, punctuation, digits, uppercase A-Z.
 * Lowercase letters are automatically converted to uppercase.
 */

#include <jo/jo.h>

#define FONT_CHAR_W  8
#define FONT_CHAR_H  8
#define FONT_FIRST   32   /* space */
#define FONT_LAST    95   /* underscore */
#define FONT_COUNT   64

/* Convert jo_printf grid coords to VDP1 pixel coords (top-left) */
#define FONT_X(col)  (-160 + (col) * 8)
#define FONT_Y(row)  (-112 + (row) * 8)

/* Call once during loadSpriteAssets (while in TEX directory) */
void font_load(void);

/* Draw string at VDP1 coords (x,y = top-left of first char) */
void font_draw(const char* str, int x, int y, int z);

/* Draw string centered horizontally at given y */
void font_draw_centered(const char* str, int y, int z);

/* Draw formatted string (printf-style) at VDP1 coords */
void font_printf(int x, int y, int z, const char* fmt, ...);

/* Draw formatted string centered horizontally */
void font_printf_centered(int y, int z, const char* fmt, ...);
