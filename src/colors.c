
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "colors.h"

bool colors_enabled = true;

const char* const clear = "\x1b[0m";

const char* const fg_table[] = {
	"\x1b[30m",
	"\x1b[31m",
	"\x1b[32m",
	"\x1b[33m",
	"\x1b[34m",
	"\x1b[35m",
	"\x1b[36m",
	"\x1b[37m",

	"\x1b[90m",
	"\x1b[91m",
	"\x1b[92m",
	"\x1b[93m",
	"\x1b[94m",
	"\x1b[95m",
	"\x1b[96m",
	"\x1b[97m"
};

const char* const bg_table[] = {
	"\x1b[40m",
	"\x1b[41m",
	"\x1b[43m",
	"\x1b[42m",
	"\x1b[44m",
	"\x1b[46m",
	"\x1b[45m",
	"\x1b[47m",

	"\x1b[100m",
	"\x1b[101m",
	"\x1b[103m",
	"\x1b[102m",
	"\x1b[104m",
	"\x1b[106m",
	"\x1b[105m",
	"\x1b[107m"
};

#define FG_BIT 0x100
#define FG_MASK 0x0F
#define FG_SHIFT 0

#define BG_BIT 0x200
#define BG_MASK 0xF0
#define BG_SHIFT 0

void color_fprintf(FILE* stream, TermColor color, const char* format, ...) {
	color_start(stream, color);
	va_list args;
	va_start(args, format);
	int count = vfprintf(stream, format, args);
	va_end(args);
	if(color & (FG_BIT | BG_BIT)) color_end(stream);
}

void color_start(FILE* stream, TermColor color) {
	if (colors_enabled) {
		if (color & FG_BIT) fprintf(stream, "%s", fg_table[(color & FG_MASK) >> FG_SHIFT]);
		if (color & BG_BIT) fprintf(stream, "%s", bg_table[(color & BG_MASK) >> BG_SHIFT]);
	}
}

void color_end(FILE* stream) {
	if (colors_enabled) fprintf(stream, "%s", clear);
}

size_t color_snprintf(char* buffer, size_t len, TermColor color, const char* format, ...) {
	char* next = buffer;
	size_t total = 0;
	if (colors_enabled) {
		if (color & FG_BIT) {
			int count = snprintf(next, len, "%s", fg_table[(color & FG_MASK) >> FG_SHIFT]);
			if (count < 0) return 0;
			next += count;
			len -= count;
			total += count;
		}
		if (color & BG_BIT) {
			int count = snprintf(next, len, "%s", bg_table[(color & BG_MASK) >> BG_SHIFT]);
			if (count < 0) return 0;
			next += count;
			len -= count;
			total += count;
		}
	}
	va_list args;
	va_start(args, format);
	int count = vsnprintf(next, len, format, args);
	va_end(args);
	if (count < 0) return 0;
	next += count;
	len -= count;
	total += count;
	if (colors_enabled && (color & (FG_BIT | BG_BIT))) {
		int count = snprintf(next, len, "%s", clear);
		if (count < 0) return 0;
		total += count;
	}
	return total;
}
