
#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"

#include "keywords.impl.gen.h"

#define MAX_LOOKAHEAD 4
#define ARENA_SIZE (16 * 1024)
#define ASCII_MAX 127

struct _lex_state {
	Token token_buf[MAX_LOOKAHEAD];
	FILE* src;
	int next_char;
	unsigned int next_tok, tok_count;
	void* arena_start;
	void* arena_current;
	size_t arena_size;
};

Lexer lexer_create(const char* filename) {
	FILE* src = fopen(filename, "r");
	if (!src) {
		return NULL;
	}
	Lexer self = calloc(1, sizeof(struct _lex_state));
	if (!self) {
		fclose(src);
		return NULL;
	}
	self->src = src;
	self->arena_start = malloc(ARENA_SIZE);
	self->arena_current = self->arena_current;
	self->arena_size = ARENA_SIZE;
	return self;
}

void lexer_destroy(Lexer self) {
	fclose(self->src);
	free(self->arena_start);
	free(self);
}

static void lex_token(Lexer self) {
	Token* current = &self->token_buf[self->tok_count];
	current->type = TOK_ERROR;
	self->tok_count++;

	FILE* src = self->src;

next_char:
	int c = fgetc(src);
	switch (c) {
		case ':': current->type = TOK_COLON; return;
		case ';': current->type = TOK_SEMICOLON; return;
		case '$': current->type = TOK_DOLLAR; return;
		case '@': current->type = TOK_AT; return;
		case '?': current->type = TOK_QMARK; return;
		case '(': current->type = TOK_LPAREN; return;
		case ')': current->type = TOK_RPAREN; return;
		case '{': current->type = TOK_LBRACE; return;
		case '}': current->type = TOK_RBRACE; return;
		case '[': current->type = TOK_LSQUARE; return;
		case ']': current->type = TOK_RSQUARE; return;
		case '.':
			if ((c = fgetc(src)) == '.') {
				if ((c = fgetc(src)) == '.') {
					current->type = TOK_ELLIPSIS;
					return;
				}
				else {
					ungetc(c, src);
					current->type = TOK_RANGE;
					return;
				}
			}
			else {
				ungetc(c, src);
				current->type = TOK_DOT;
				return;
			}
		default:
			if (c > ASCII_MAX) {} // TODO: fancy unicode stuff
	}
}

Token lexer_peek_token(Lexer self, unsigned int offset) {
	if (offset >= MAX_LOOKAHEAD) return (Token) { .type = TOK_ERROR };
	while (offset + 1 > self->tok_count) lex_token(self);
	return self->token_buf[(self->next_tok + offset) % MAX_LOOKAHEAD];
}

Token lexer_pop_token(Lexer self) {
	if (!self->tok_count) lex_token(self);
	Token* result = &self->token_buf[self->next_tok];
	self->next_tok = (self->next_tok + 1) % MAX_LOOKAHEAD;
	self->tok_count--;
	return *result;
}

#define REPR_SIZE 80

const char* token_repr(const Token* tok) {
	char* out = malloc(REPR_SIZE);
	switch (tok->type) {
		case TOK_EMPTY:
			snprintf(out, REPR_SIZE, "<EMPTY>");
			break;

		case TOK_KEYWORD:
			snprintf(out, REPR_SIZE, "<KEYWORD %s : %d,%d-%d,%d>",
				kw_to_str(tok->kw_value),
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_IDENT:
			snprintf(out, REPR_SIZE, "<IDENT %s : %d,%d-%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_DIRECTIVE:
			snprintf(out, REPR_SIZE, "<DIRECTIVE #%s : %d,%d-%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_INT:
			snprintf(out, REPR_SIZE, "<INT %lld : %d,%d-%d,%d>",
				(long long int) tok->int_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_FLOAT:
			snprintf(out, REPR_SIZE, "<FLOAT %g : %d,%d-%d,%d>",
				tok->float_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_STRING:
			// TODO: better string shortening
			snprintf(out, REPR_SIZE, "<STRING \"%s\" : %d,%d-%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		// case TOK_STRING_TEMPLATE_START:
			// snprintf(out, REPR_SIZE, "<STRING_TEMPLATE_START : %d,%d-%d,%d>",
				// tok->start_line, tok->start_col,
				// tok->end_line, tok->end_col);
			// break;
		// case TOK_STRING_TEMPLATE_MIDDLE:
			// snprintf(out, REPR_SIZE, "<STRING_TEMPLATE_MIDDLE : %d,%d-%d,%d>",
				// tok->start_line, tok->start_col,
				// tok->end_line, tok->end_col);
			// break;
		// case TOK_STRING_TEMPLATE_END:
			// snprintf(out, REPR_SIZE, "<STRING_TEMPLATE_END : %d,%d-%d,%d>",
				// tok->start_line, tok->start_col,
				// tok->end_line, tok->end_col);
			// break;
		case TOK_CHAR:
			snprintf(out, REPR_SIZE, "<CHAR %lc : %d,%d-%d,%d>",
				tok->char_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_BOOL:
			snprintf(out, REPR_SIZE, "<BOOL %s : %d,%d-%d,%d>",
				tok->bool_value? "true" : "false",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_BACKSLASH:
			snprintf(out, REPR_SIZE, "<BACKSLASH : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_AT:
			snprintf(out, REPR_SIZE, "<AT : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_DOLLAR:
			snprintf(out, REPR_SIZE, "<DOLLAR : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_COLON:
			snprintf(out, REPR_SIZE, "<COLON : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_ASSIGN:
			snprintf(out, REPR_SIZE, "<ASSIGN : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_DOT:
			snprintf(out, REPR_SIZE, "<DOT : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RANGE:
			snprintf(out, REPR_SIZE, "<RANGE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_ELLIPSIS:
			snprintf(out, REPR_SIZE, "<ELLIPSIS : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_COMMA:
			snprintf(out, REPR_SIZE, "<COMMA : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_SEMICOLON:
			snprintf(out, REPR_SIZE, "<SEMICOLON : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_QMARK:
			snprintf(out, REPR_SIZE, "<QMARK : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_BANG:
			snprintf(out, REPR_SIZE, "<BANG : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_PLUS:
			snprintf(out, REPR_SIZE, "<PLUS : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_MINUS:
			snprintf(out, REPR_SIZE, "<MINUS : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_STAR:
			snprintf(out, REPR_SIZE, "<STAR : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_SLASH:
			snprintf(out, REPR_SIZE, "<SLASH : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_PERCENT:
			snprintf(out, REPR_SIZE, "<PERCENT : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_CARET:
			snprintf(out, REPR_SIZE, "<CARET : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_AMP:
			snprintf(out, REPR_SIZE, "<AMP : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_BAR:
			snprintf(out, REPR_SIZE, "<BAR : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_TILDE:
			snprintf(out, REPR_SIZE, "<TILDE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_EQ:
			snprintf(out, REPR_SIZE, "<EQ : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_NE:
			snprintf(out, REPR_SIZE, "<NE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LT:
			snprintf(out, REPR_SIZE, "<LT : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_GT:
			snprintf(out, REPR_SIZE, "<GT : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LE:
			snprintf(out, REPR_SIZE, "<LE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_GE:
			snprintf(out, REPR_SIZE, "<GE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_CUSTOM_OPERATOR:
			snprintf(out, REPR_SIZE, "<OPERATOR %s : %d,%d-%d,%d>",
				tok->str_value,
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_LPAREN:
			snprintf(out, REPR_SIZE, "<LPAREN : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RPAREN:
			snprintf(out, REPR_SIZE, "<RPAREN : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LSQUARE:
			snprintf(out, REPR_SIZE, "<LSQUARE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RSQUARE:
			snprintf(out, REPR_SIZE, "<RSQUARE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_LBRACE:
			snprintf(out, REPR_SIZE, "<LBRACE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		case TOK_RBRACE:
			snprintf(out, REPR_SIZE, "<RBRACE : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_EOL:
			snprintf(out, REPR_SIZE, "<EOL : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;

		case TOK_EOF:
			snprintf(out, REPR_SIZE, "<EOF : %d,%d-%d,%d>",
				tok->start_line, tok->start_col,
				tok->end_line, tok->end_col
			);
			break;
		default:
			snprintf(out, REPR_SIZE, "<?""?""?>");
			break;
	}
	return out;
}

const char* token_to_string(const Token*);
