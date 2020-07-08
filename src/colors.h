#pragma once
// ANSI color escapes
#include <stdint.h>
#include <stdio.h>

typedef enum {
	TERM_FG_BLACK    = 0x100,
	TERM_FG_RED      = 0x101,
	TERM_FG_YELLOW   = 0x103,
	TERM_FG_GREEN    = 0x102,
	TERM_FG_BLUE     = 0x104,
	TERM_FG_CYAN     = 0x106,
	TERM_FG_MAGENTA  = 0x105,
	TERM_FG_SILVER   = 0x107,

	TERM_FG_GRAY     = 0x108,
	TERM_FG_LRED     = 0x109,
	TERM_FG_LYELLOW  = 0x10B,
	TERM_FG_LGREEN   = 0x10A,
	TERM_FG_LBLUE    = 0x10C,
	TERM_FG_LCYAN    = 0x10E,
	TERM_FG_LMAGENTA = 0x10D,
	TERM_FG_WHITE    = 0x10F,

	TERM_BG_BLACK    = 0x200,
	TERM_BG_RED      = 0x210,
	TERM_BG_YELLOW   = 0x230,
	TERM_BG_GREEN    = 0x220,
	TERM_BG_BLUE     = 0x240,
	TERM_BG_CYAN     = 0x260,
	TERM_BG_MAGENTA  = 0x250,
	TERM_BG_SILVER   = 0x270,

	TERM_BG_GRAY     = 0x280,
	TERM_BG_LRED     = 0x290,
	TERM_BG_LYELLOW  = 0x2B0,
	TERM_BG_LGREEN   = 0x2A0,
	TERM_BG_LBLUE    = 0x2C0,
	TERM_BG_LCYAN    = 0x2E0,
	TERM_BG_LMAGENTA = 0x2D0,
	TERM_BG_WHITE    = 0x2F0,
} TermColor;

void color_fprintf(FILE* stream, TermColor color, const char* format, ...);
#define color_printf(color, fmt, ...) color_fprintf(stdout, color, fmt, ##__VA_ARGS__)

size_t color_snprintf(char* buffer, size_t len, TermColor color, const char* format, ...);
void color_start(FILE* stream, TermColor color);
void color_end(FILE* stream);

void color_enable();
void color_disable();
