#pragma once

void show_error_line(FILE* stream, const char* line, int line_no, int start_col, int end_col);
const char* read_entire_file(const char* filename);
