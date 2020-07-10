#include <string.h>

#include "colors.h"

TermColor line_number_color = TERM_FG_GRAY;
TermColor pointer_color = TERM_FG_LYELLOW;

int unicode_arrow = 1;

void show_error_line(FILE* stream, const char* line, int line_no, int start_col, int end_col) {
	color_fprintf(stream, line_number_color, "% 5d |\t", line_no);
	fprintf(stream, "%s\n\t", line);
	color_start(stream, pointer_color);
	for (int i = 1; i <= end_col; i++) {
		if (i < start_col) fprintf(stream, " ");
		if (i == start_col || i == end_col) fprintf(stream, unicode_arrow ? "\u2191" : "^");
		if (i > start_col && i < end_col) fprintf(stream, unicode_arrow ? "\u2191" : "^");
	}
	color_end(stream);
	fprintf(stream, "\n");
}

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
