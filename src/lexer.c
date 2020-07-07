
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>

#include "arena.h"
#include "lexer.h"
#include "utf8.h"
#include "stretchy_buffer.h"

#include "keywords.impl.gen.h"

#define MAX_LOOKAHEAD 4
#define ASCII_MAX 127
#define MAX_DIGITS 256

struct _lex_state {
	Token token_buf[MAX_LOOKAHEAD];
	FILE* src;
	unsigned int next_tok, tokens_buffered, total_tokens_emitted;
	unsigned int line_no, column;
	void* arena_block; // The arena used for literal text for tokens, raw lines, and strings
	char* next_literal;  // Rolling pointer used for storing
	char* string_buffer; // The rolling pointer where strings and identifiers get allocated
	char* line_buffer;   // Rolling pointer used for storing lines of the file
	const char** lines;
	char* current_line;
	int line_offset;
	int line_length;
	bool is_last_line;
};

Lexer lexer_create(const char* filename) {
	// Assumption: filename outlives the lexer
	FILE* src = fopen(filename, "r");
	if (!src) {
		return NULL;
	}
	fseek(src, 0, SEEK_END);
	size_t src_size = ftell(src);
	rewind(src);
	Lexer self = calloc(1, sizeof(struct _lex_state));
	if (!self) {
		fclose(src);
		return NULL;
	}
	self->src = src;
	self->next_literal = self->arena_block = malloc(src_size * 5 + 1);
	self->string_buffer = ((char*) self->arena_block) + src_size * 2;
	self->line_buffer = ((char*) self->arena_block) + src_size * 4; // size = src_size + 1 byte (for the null at the end)
	self->line_length = -1;
	self->line_no = 1;
	self->column = 1;
	// All other fields are zero, and that is fine.
	return self;
}

void lexer_destroy(Lexer self) {
	fclose(self->src);
	free(self);
}

const char** lexer_get_lines(Lexer self, int* len) {
	if (len) *len = sb_count(self->lines);
	return self->lines;
}

static int lexer_fwdc(Lexer self) {
	if (self->line_offset > self->line_length) {
		self->line_offset = 0;
		self->line_length = 0;
		sb_push(self->lines, self->line_buffer);
		self->current_line = self->line_buffer;

		while (1) {
			int c;
			switch (c = fgetc(self->src)) {
				case 0: break;  // ignore null bytes
				case EOF:
					self->is_last_line = true;
					// drop through is intentional
				case '\n':
					*self->line_buffer++ = 0;
					goto emit_char;
				default:
					*self->line_buffer++ = c;
					self->line_length++;
			}
		}
	}
	emit_char:
	if (self->line_offset < 0) {
		self->line_offset = 0;
		return '\n';
	}
	else if (self->line_offset == self->line_length) {
		if (self->is_last_line) return EOF;
		self->line_offset++;
		return '\n';
	}
	return self->current_line[self->line_offset++];
}

static bool lexer_fwd_line(Lexer self) {
	self->line_offset = self->line_length + 1;
	return self->is_last_line;
}

static void lexer_backc(Lexer self) {
	// back up until we're not on a continuation byte
	while ((self->current_line[--self->line_offset] & 0xC0) == 0x80);
}

static int read_utf8_cont(Lexer self, int first, char** text_ptr) {
	int scratch = first;
	int result = first;
	unsigned int antimask = 0xffffffe0;
	while ((scratch & 0xc0) == 0xc0) {  // scratch == 0b11xx_xxxx
		scratch = scratch << 1;
		int c = lexer_fwdc(self);
		*(*text_ptr)++ = c;
		if ((c & 0xc0) != 0x80) return -1;
		result = (result << 6) | (c & 0x3f);
		antimask = antimask << 5;
	}
	return result & ~antimask;
}


// NOTE: all characters are assumed to take up one column. This is not an accurate assumption. Notably:
// Tab has a width of 4 or 8 - terminals use a width of 8
// CJK characters typically have a width of 2
// Emojis are insane and don't behave nicely in fixed-width text

#define FWD() ( \
	current->end_col = self->column++, \
	*text_ptr++ = cur_ch = lexer_fwdc(self) \
)

#define BACK() do { \
	--text_ptr; \
	current->end_col = --self->column - 1;  /* since the column counter is already ahead */ \
	lexer_backc(self); \
} while (0)

#define EMIT(TOKTYPE) do { \
	current->type = TOKTYPE; \
	*text_ptr++ = 0; \
	self->next_literal = text_ptr; \
	return; \
} while (0)

#define UTF8() (cur_ch = read_utf8_cont(self, cur_ch, &text_ptr))
#define FWD_UTF8() do { FWD(); if (cur_ch > ASCII_MAX) UTF8(); } while (0)

static void lexer_emit_token(Lexer self) {
	Token* current = &self->token_buf[(self->next_tok + self->tokens_buffered++) % MAX_LOOKAHEAD];
	bool raw_string = false;
	int cur_ch = 0;

	current->type = TOK_EMPTY;

	reset:
	current->start_line = current->end_line = self->line_no;
	current->start_col = self->column;
	current->literal_text = self->next_literal;
	char* text_ptr = self->next_literal;
	FWD();
	switch (cur_ch) {
		case EOF: {
			current->type = TOK_EOF;
			strcpy(current->literal_text, "<EOF>");
			return;
		}
		case '\n': emit_eol:
			current->end_line++;
			current->end_col = 0;
			self->line_no++;
			self->column = 1;
			EMIT(TOK_EOL);
		case ':': EMIT(TOK_COLON);
		case ';': EMIT(TOK_SEMICOLON);
		case ',': EMIT(TOK_COMMA);
		case '$': EMIT(TOK_DOLLAR);
		case '@': EMIT(TOK_AT);
		case '?': EMIT(TOK_QMARK);
		case '(': EMIT(TOK_LPAREN);
		case ')': EMIT(TOK_RPAREN);
		case '{': EMIT(TOK_LBRACE);
		case '}': EMIT(TOK_RBRACE);
		case '[': EMIT(TOK_LSQUARE);
		case ']': EMIT(TOK_RSQUARE);
		case '\\':
			switch (FWD()) {
				case '\\':  // Comment
					if (lexer_fwd_line(self)) EMIT(TOK_EOF);
					goto emit_eol;
				case '"':  // Raw String
					raw_string = true;
					goto handle_string;
				case '\n':
					self->line_no++;
					self->column = 1;
					goto reset;
				default: // Just a backslash
					BACK();
					EMIT(TOK_BACKSLASH);
			}
		case '.':
			if (FWD() == '.') {
				if (FWD() == '.') {
					EMIT(TOK_ELLIPSIS);
				}
				else {
					BACK();
					EMIT(TOK_RANGE);
				}
			}
			else {
				BACK();
				EMIT(TOK_DOT);
			}
		case '=':
			switch (FWD()) {
				case '>': EMIT(TOK_ARROW);
				case '=': EMIT(TOK_EQ);
				default:
					BACK();
					EMIT(TOK_ASSIGN);
			}
		case '<':
			if (FWD() == '=') {
				EMIT(TOK_LE);
			}
			else {
				BACK();
				EMIT(TOK_LT);
			}
		case '>':
			if (FWD() == '=') {
				EMIT(TOK_GE);
			}
			else {
				BACK();
				EMIT(TOK_GT);
			}
		case '!':
			if (FWD() == '=') {
				EMIT(TOK_NE);
			}
			else {
				BACK();
				EMIT(TOK_BANG);
			}
		case '+': current->type = TOK_PLUS; goto handle_operators;
		case '-': current->type = TOK_MINUS; goto handle_operators;
		case '*': current->type = TOK_STAR; goto handle_operators;
		case '/': current->type = TOK_SLASH; goto handle_operators;
		case '%': current->type = TOK_PERCENT; goto handle_operators;
		case '^': current->type = TOK_CARET; goto handle_operators;
		case '&': current->type = TOK_AMP; goto handle_operators;
		case '|': current->type = TOK_BAR; goto handle_operators;
		case '~': current->type = TOK_TILDE; goto handle_operators;

		handle_operators:
			while (1) {
				switch (FWD()) {
					case '+': case '-':
					case '*': case '/': case '%':
					case '^':
					case '&': case '|':
					case '~':
						current->type = TOK_CUSTOM_OPERATOR;
						current->str_value = current->literal_text;
						break;

					default: BACK(); EMIT(current->type);
				}
			}

		case '0':
		case '1': case '2': case '3':
		case '4': case '5': case '6':
		case '7': case '8': case '9': { // some sort of number
			char num_buf[MAX_DIGITS + 1];
			char *digit = num_buf;
			int radix = 10;

			if (cur_ch == '0') {
				switch (FWD()) {
					case 'x': case 'X': // hex
						radix = 16;
						while (1) {
							FWD();
							if (cur_ch == '_') continue;
							else if (isxdigit(cur_ch)) *digit++ = cur_ch;
							else goto handle_int;
						}
					case 'o': case 'O': // octal
						radix = 8;
						while (1) {
							FWD();
							if (cur_ch == '_') continue;
							else if (cur_ch >= '0' && cur_ch <= '7') *digit++ = cur_ch;
							else goto handle_int;
						}
					case 'b': case 'B': // binary
						radix = 2;
						while (1) {
							FWD();
							if (cur_ch == '_') continue;
							else if (cur_ch == '0' || cur_ch == '1') *digit++ = cur_ch;
							else goto handle_int;
						}
					case '.': // float
						*digit++ = cur_ch;
						goto handle_float;

					default:
						current->int_value = 0;
						BACK();
						EMIT(TOK_INT);

					case '0':
					case '1': case '2': case '3':
					case '4': case '5': case '6':
					case '7': case '8': case '9':
						// drop through is intentional
						*digit++ = cur_ch;
					case '_': break;
				}
			}
			else {
				*digit++ = cur_ch;
			}

			while (1) {
				FWD();
				if (cur_ch == '_') continue;
				else if (cur_ch == '.') {
					*digit++ = cur_ch;
					goto handle_float;
				}
				else if (isdigit(cur_ch)) *digit++ = cur_ch;
				else break;
			}
			handle_int:
			*digit = 0;
			BACK();
			current->int_value = strtoll(num_buf, &digit, radix);
			EMIT(TOK_INT);

			handle_float:
			while (isdigit(FWD())) *digit++ = cur_ch;
			if (cur_ch == 'e' || cur_ch == 'E') {
				FWD();
				if (cur_ch == '-' || cur_ch == '+' || isdigit(cur_ch)) {
					*digit++ = cur_ch;
				}
				else {
					BACK();
					EMIT(TOK_ERROR);
				}
				while (isdigit(FWD())) *digit++ = cur_ch;
			}
			*digit = 0;
			BACK();
			current->float_value = strtold(num_buf, &digit);
			EMIT(TOK_FLOAT);
		}

		handle_string:
		case '"': {
			bool triple_quote = false;
			current->str_value = self->string_buffer;
			if (FWD() == '"') {
				if (FWD() == '"') { // triple quoted
					triple_quote = true;
					FWD(); // So that we're looking at the first char of the string
				}
				else { // empty string
					BACK();
					goto emit_str;
				}
			}
			// We're already looking at the first char of the string
			while (1) {
				switch (cur_ch) {
					case '"':
						if (triple_quote) {
							if (FWD() == '"') {
								if (FWD() == '"') goto emit_str;
								*self->string_buffer++ = '"';
							}
							*self->string_buffer++ = '"';
						}
						else goto emit_str;
					case '\\':
						if (raw_string) goto str_normal_char;
						switch (FWD()) {
							case '0': *self->string_buffer++ = 0; break;
							case 'n': *self->string_buffer++ = '\n'; break;
							case 'r': *self->string_buffer++ = '\r'; break;
							case 't': *self->string_buffer++ = '\t'; break;
							case 'a': *self->string_buffer++ = '\a'; break;
							case 'b': *self->string_buffer++ = '\b'; break;
							case 'f': *self->string_buffer++ = '\f'; break;
							case 'v': *self->string_buffer++ = '\v'; break;
							case 'e': *self->string_buffer++ = '\x1b'; break;
							case '\'': *self->string_buffer++ = '\''; break;
							case '"': *self->string_buffer++ = '"'; break;
							case '\\': *self->string_buffer++ = '\\'; break;
							case 'o': {
								char buf[4];
								for (int i = 0; i < 3; i++) {
									buf[i] = FWD();
									if (cur_ch < '0' || cur_ch > '7') EMIT(TOK_ERROR);
								}
								buf[3] = 0;
								int codepoint = strtol(buf, 0, 8);
								self->string_buffer = utf8_write(self->string_buffer, codepoint);
							} break;

							{
								char buf[8];
								int n_digits;
							case 'U': n_digits = 6; goto str_handle_hex_escape;
							case 'u': n_digits = 4; goto str_handle_hex_escape;
							case 'x': n_digits = 2; goto str_handle_hex_escape;
							str_handle_hex_escape:
								for (int i = 0; i < n_digits; i++) {
									buf[i] = FWD();
									if (!isxdigit(cur_ch)) EMIT(TOK_ERROR);
								}
								buf[n_digits] = 0;
								int codepoint = strtol(buf, 0, 16);
								self->string_buffer = utf8_write(self->string_buffer, codepoint);
							} break;
							default: EMIT(TOK_ERROR);
						}
						break;
					case '\n': if (!triple_quote) EMIT(TOK_ERROR);
					str_normal_char:
					default:
						*self->string_buffer++ = cur_ch;
				}
				FWD();
			}
			emit_str:
			*self->string_buffer++ = 0;
			EMIT(TOK_STRING);
		}

		case '\'':
			FWD();
			if (cur_ch == '\\') {
				switch (FWD()) {  // Escapes
					case '0': current->char_value = 0; break;
					case 'n': current->char_value = '\n'; break;
					case 'r': current->char_value = '\r'; break;
					case 't': current->char_value = '\t'; break;
					case 'a': current->char_value = '\a'; break;
					case 'b': current->char_value = '\b'; break;
					case 'f': current->char_value = '\f'; break;
					case 'v': current->char_value = '\v'; break;
					case 'e': current->char_value = '\x1b'; break;
					case '\'': current->char_value = '\''; break;
					case '"': current->char_value = '"'; break;
					case '\\': current->char_value = '\\'; break;
					case 'o':
						for (int i = 0; i < 3; i++) {
							FWD();
							if (cur_ch < '0' || cur_ch > '7') EMIT(TOK_ERROR);
						}
						*text_ptr = 0;
						current->char_value = strtol(current->literal_text + 3, 0, 8);
						break;
					#define FWDX() do { if (!isxdigit(FWD())) EMIT(TOK_ERROR); } while (0)
					case 'U': FWDX(); FWDX();
					case 'u': FWDX(); FWDX();
					case 'x': FWDX(); FWDX();
					#undef FWDX
						*text_ptr = 0;
						current->char_value = strtol(current->literal_text + 3, 0, 16);
						break;
					default:
						if (isspace(cur_ch)) {
							current->char_value = '\\';
							BACK();
							*text_ptr = 0;
							EMIT(TOK_CHAR);
						}
						else EMIT(TOK_ERROR);
				}
			}
			else if (cur_ch == '\n' || cur_ch == '\r') current->char_value = ' ';
			else if (cur_ch > ASCII_MAX) current->char_value = UTF8();
			else current->char_value = cur_ch;
			if (FWD() != '\'') BACK();
			EMIT(TOK_CHAR);

		case '#': // Directive
			do {
				FWD();
			} while (cur_ch == '_' || isalnum(cur_ch));
			BACK();
			*text_ptr = 0;
			current->str_value = current->literal_text + 1;
			if (*current->str_value == 0) {  // Lone # is an identifier
				current->str_value = current->literal_text;
				EMIT(TOK_IDENT);
			}
			else EMIT(TOK_DIRECTIVE);

		case '`': // Forced Identifier
			FWD();
			if (cur_ch != '_' && !isalpha(cur_ch)) EMIT(TOK_ERROR);
			do {
				FWD();
			} while (cur_ch == '_' || isalnum(cur_ch));
			if (cur_ch != '`') {
				current->str_value = current->literal_text + 1;
				BACK();
			}
			else {
				*text_ptr = 0;
				size_t len = strlen(current->literal_text) - 1;
				char* buf = arena_alloc__noabc(&self->string_buffer, len);
				strncpy(buf, current->literal_text + 1, len - 1);
				current->str_value = buf;
			}
			EMIT(TOK_IDENT);

		default:
			if (isspace(cur_ch)) goto reset;
			if (cur_ch > ASCII_MAX) UTF8();

			if (cur_ch == '_' || iswalpha(cur_ch)) {  // Identifier or Keyword
				do {
					FWD_UTF8();
				} while (cur_ch == '_' || iswalnum(cur_ch));
				BACK();  // NOTE: this is currently a problem for non-alnum unicodes b/c BACK() doesn't handle UTF-8 correctly.
				// This probably doesn't matter because that would be an error anyway
				*text_ptr = 0;
				Keyword kw = str_to_kw(current->literal_text);
				if (kw) {
					current->kw_value = kw;
					EMIT(/* TOK_KEYWORD | */ kw);
				}
				else if (strcmp(current->literal_text, "true") == 0) {
					current->bool_value = true;
					EMIT(TOK_BOOL);
				}
				else if (strcmp(current->literal_text, "false") == 0) {
					current->bool_value = false;
					EMIT(TOK_BOOL);
				}
				else {
					current->str_value = current->literal_text;
					EMIT(TOK_IDENT);
				}
			}
	}
	EMIT(TOK_ERROR);
}

#undef FWD
#undef BACK
#undef EMIT

Token lexer_peek_token(Lexer self, int offset) {
	if (offset < 0) {
		// check that the token exists and has not yet been overwritten
		if (self->total_tokens_emitted + offset >= 0
				&& self->tokens_buffered - offset < MAX_LOOKAHEAD ) {
			return self->token_buf[(self->next_tok + MAX_LOOKAHEAD + offset) % MAX_LOOKAHEAD];
		}
		else return (Token) { .type = TOK_EMPTY };
	}
	if (offset >= MAX_LOOKAHEAD) return (Token) { .type = TOK_ERROR };
	while (offset + 1 > self->tokens_buffered) lexer_emit_token(self);
	return self->token_buf[(self->next_tok + offset) % MAX_LOOKAHEAD];
}

Token lexer_pop_token(Lexer self) {
	if (!self->tokens_buffered) lexer_emit_token(self);
	Token* result = &self->token_buf[self->next_tok];
	self->next_tok = (self->next_tok + 1) % MAX_LOOKAHEAD;
	self->tokens_buffered--;
	return *result;
}

#define REPR_SIZE 80

const char* token_repr(const Token* tok) {
	char* out = malloc(REPR_SIZE); // <-- sloppy memory management here
	switch (tok->type) {
		case TOK_EMPTY:
			snprintf(out, REPR_SIZE, "<EMPTY>");
			break;

		case TOK_KEYWORD:
			snprintf(out, REPR_SIZE, "<KEYWORD \?\?\? : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_IDENT:
			snprintf(out, REPR_SIZE, "<IDENT %s : %d,%d..%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_DIRECTIVE:
			snprintf(out, REPR_SIZE, "<DIRECTIVE #%s : %d,%d..%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_INT:
			snprintf(out, REPR_SIZE, "<INT %lld : %d,%d..%d,%d>",
				(long long int) tok->int_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_FLOAT:
			snprintf(out, REPR_SIZE, "<FLOAT %Lg : %d,%d..%d,%d>",
				tok->float_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_STRING:
			// TODO: better string shortening
			snprintf(out, REPR_SIZE, "<STRING \"%s\" : %d,%d..%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_CHAR: {
			const char* fmt = 0;
			if (tok->char_value < ASCII_MAX) {
				fmt = isprint(tok->char_value)?
					"<CHAR %c : %d,%d..%d,%d>" : "<CHAR \\x%02x : %d,%d..%d,%d>";
			}
			else {
				fmt = (tok->char_value > 0xFFFF) ?
					"<CHAR \\U%06x : %d,%d..%d,%d>" : "<CHAR \\u%04x : %d,%d..%d,%d>";
			}
			snprintf(out, REPR_SIZE, fmt,
				tok->char_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
		} break;
		case TOK_BOOL:
			snprintf(out, REPR_SIZE, "<BOOL %s : %d,%d..%d,%d>",
				tok->bool_value? "true" : "false",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_BACKSLASH:
			snprintf(out, REPR_SIZE, "<BACKSLASH : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_AT:
			snprintf(out, REPR_SIZE, "<AT : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_DOLLAR:
			snprintf(out, REPR_SIZE, "<DOLLAR : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_COLON:
			snprintf(out, REPR_SIZE, "<COLON : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_ASSIGN:
			snprintf(out, REPR_SIZE, "<ASSIGN : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_DOT:
			snprintf(out, REPR_SIZE, "<DOT : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RANGE:
			snprintf(out, REPR_SIZE, "<RANGE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_ELLIPSIS:
			snprintf(out, REPR_SIZE, "<ELLIPSIS : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_COMMA:
			snprintf(out, REPR_SIZE, "<COMMA : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_SEMICOLON:
			snprintf(out, REPR_SIZE, "<SEMICOLON : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_QMARK:
			snprintf(out, REPR_SIZE, "<QMARK : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_BANG:
			snprintf(out, REPR_SIZE, "<BANG : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_PLUS:
			snprintf(out, REPR_SIZE, "<PLUS : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_MINUS:
			snprintf(out, REPR_SIZE, "<MINUS : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_STAR:
			snprintf(out, REPR_SIZE, "<STAR : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_SLASH:
			snprintf(out, REPR_SIZE, "<SLASH : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_PERCENT:
			snprintf(out, REPR_SIZE, "<PERCENT : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_CARET:
			snprintf(out, REPR_SIZE, "<CARET : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_AMP:
			snprintf(out, REPR_SIZE, "<AMP : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_BAR:
			snprintf(out, REPR_SIZE, "<BAR : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_TILDE:
			snprintf(out, REPR_SIZE, "<TILDE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_ARROW:
			snprintf(out, REPR_SIZE, "<ARROW : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_EQ:
			snprintf(out, REPR_SIZE, "<EQ : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_NE:
			snprintf(out, REPR_SIZE, "<NE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LT:
			snprintf(out, REPR_SIZE, "<LT : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_GT:
			snprintf(out, REPR_SIZE, "<GT : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LE:
			snprintf(out, REPR_SIZE, "<LE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_GE:
			snprintf(out, REPR_SIZE, "<GE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_CUSTOM_OPERATOR:
			snprintf(out, REPR_SIZE, "<OPERATOR %s : %d,%d..%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_LPAREN:
			snprintf(out, REPR_SIZE, "<LPAREN : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RPAREN:
			snprintf(out, REPR_SIZE, "<RPAREN : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LSQUARE:
			snprintf(out, REPR_SIZE, "<LSQUARE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RSQUARE:
			snprintf(out, REPR_SIZE, "<RSQUARE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LBRACE:
			snprintf(out, REPR_SIZE, "<LBRACE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RBRACE:
			snprintf(out, REPR_SIZE, "<RBRACE : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_EOL:
			snprintf(out, REPR_SIZE, "<EOL : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_EOF:
			snprintf(out, REPR_SIZE, "<EOF : %d,%d..%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_ERROR:
			snprintf(out, REPR_SIZE, "<ERROR %s : %d,%d..%d,%d>",
				tok->literal_text,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		default:
			if ((tok->type & TOK_KEYWORD) == TOK_KEYWORD) {
				snprintf(out, REPR_SIZE, "<KEYWORD %s : %d,%d..%d,%d>",
					kw_to_str(tok->kw_value),
					tok->start_line, tok->start_col,
					tok->end_line, tok->end_col
				);
			}
			else snprintf(out, REPR_SIZE, "<\?\?\?>");
			break;
	}
	return out;
}

const char* token_to_string(const Token*);
