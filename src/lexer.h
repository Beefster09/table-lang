#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#include "keywords.gen.h"

typedef struct _lex_state* Lexer;

typedef struct Token {
	enum {
		TOK_EMPTY = 0,

		TOK_KEYWORD = 0x100,
			// Also includes keywords
		TOK_IDENT     = 1,
		TOK_DIRECTIVE = 2,
		TOK_INT       = 3,
		TOK_FLOAT     = 4,
		TOK_STRING    = 5,
		TOK_CHAR      = 6,
		TOK_BOOL      = 7,

		TOK_BACKSLASH = '\\',
		TOK_AT        = '@',
		TOK_DOLLAR    = '$',
		TOK_COLON     = ':',
		TOK_ASSIGN    = '=',
		TOK_DOT       = '.',
		TOK_RANGE     = 16,
		TOK_ELLIPSIS  = 17,
		TOK_COMMA     = ',',
		TOK_SEMICOLON = ';',
		TOK_QMARK     = '?',
		TOK_BANG      = '!',
		TOK_PLUS      = '+',
		TOK_MINUS     = '-',
		TOK_STAR      = '*',
		TOK_SLASH     = '/',
		TOK_PERCENT   = '%',
		TOK_CARET     = '^',
		TOK_AMP       = '&',
		TOK_BAR       = '|',
		TOK_TILDE     = '~',
		TOK_ARROW     = 18,

		TOK_EQ = 20,
		TOK_NE = 21,
		TOK_LT = '<',
		TOK_GT = '>',
		TOK_LE = 22,
		TOK_GE = 23,

		TOK_CUSTOM_OPERATOR = 19,

		TOK_LPAREN  = '(',
		TOK_RPAREN  = ')',
		TOK_LSQUARE = '[',
		TOK_RSQUARE = ']',
		TOK_LBRACE  = '{',
		TOK_RBRACE  = '}',

		TOK_EOL = 0xE01,

		TOK_EOF = 0xE0F,
		TOK_ERROR = -1
	} type;
	unsigned int start_line, start_col, end_line, end_col;
	const char* literal_text;
	union {
		const char* str_value;
		intmax_t int_value;
		long double float_value;
		bool bool_value;
		int32_t char_value;
		Keyword kw_value;
	};
} Token;

Lexer lexer_create(const char* filename);
void lexer_destroy(Lexer);

/// Provides the pointer to the memory block that holds this lexer's string data
void* lexer_get_arena(Lexer);

/// Returns the array of lines in the file.
const char** lexer_get_lines(Lexer, int* len);

const Token* lexer_peek_token(Lexer, int offset);
const Token* lexer_pop_token(Lexer);

const char* token_repr(const Token*);
const char* token_to_string(const Token*);
