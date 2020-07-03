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
		TOK_IDENT = 1,
		TOK_DIRECTIVE,
		TOK_INT,
		TOK_FLOAT,
		TOK_STRING,
		// TOK_STRING_TEMPLATE_START,
		// TOK_STRING_TEMPLATE_MIDDLE,
		// TOK_STRING_TEMPLATE_END,
		TOK_CHAR,
		TOK_BOOL,

		TOK_BACKSLASH,
		TOK_AT,
		TOK_DOLLAR,
		TOK_COLON,
		TOK_ASSIGN,
		TOK_DOT,
		TOK_RANGE,
		TOK_ELLIPSIS,
		TOK_COMMA,
		TOK_SEMICOLON,
		TOK_QMARK,
		TOK_BANG,
		TOK_PLUS,
		TOK_MINUS,
		TOK_STAR,
		TOK_SLASH,
		TOK_PERCENT,
		TOK_CARET,
		TOK_AMP,
		TOK_BAR,
		TOK_TILDE,

		TOK_EQ, TOK_NE,
		TOK_LT, TOK_GT,
		TOK_LE, TOK_GE,

		TOK_CUSTOM_OPERATOR,

		TOK_LPAREN, TOK_RPAREN,
		TOK_LSQUARE, TOK_RSQUARE,
		TOK_LBRACE, TOK_RBRACE,

		TOK_EOL = 0xE01,

		TOK_EOF = 0xE0F,
		TOK_ERROR = -1
	} type;
	unsigned int start_line, start_col, end_line, end_col;
	const char* literal_text;
	union {
		const char* str_value;
		intmax_t int_value;
		double float_value;
		bool bool_value;
		int32_t char_value;
		Keyword kw_value;
	};
} Token;

Lexer lexer_create(const char* filename);
void lexer_destroy(Lexer);

Token lexer_peek_token(Lexer, unsigned int offset);
Token lexer_pop_token(Lexer);

const char* token_repr(const Token*);
const char* token_to_string(const Token*);
