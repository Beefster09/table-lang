
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "lexer.h"
#include "parser.h"
#include "stb_ds.h"
#include "util.h"
#include "colors.h"

#ifndef ARENA_SIZE
#define ARENA_SIZE (16 * 1024)
#endif

bool RULE_DEBUG = 1;

struct _parse_state {
	const char* src;
	Lexer lex;
	void** arenas;
	void* arena_current;
	int error_count;
	int warning_count;
};

Parser parser_create(const char* filename) {
	char* src = malloc(strlen(filename) + 1);
	strcpy(src, filename);
	Lexer lex = lexer_create(src);
	if (!lex) {
		free(src);
		return 0;
	}
	Parser self = calloc(1, sizeof(struct _parse_state));
	self->src = src;
	self->lex = lex;
	return self;
}

void parser_destroy(Parser self) {

}

static size_t size_table[] = {
	sizeof(AST_Node),

    #define XMAC(NAME) sizeof(struct NAME),
    #include "ast_node_types.gen.h"
    #undef XMAC

    0
};

static inline void* arena_alloc(Parser self, size_t n_bytes) {
	if (!self->arena_current || self->arena_current - arrlast(self->arenas) + n_bytes > ARENA_SIZE) {
		self->arena_current = calloc(ARENA_SIZE, 1);  // ensure all nodes from this arena are zeroed out
		assert(self->arena_current && "Unable to allocate next block of arena!!!");
		arrpush(self->arenas, self->arena_current);
	}
	void* result = self->arena_current;
	self->arena_current = (void*) (
		((char*) self->arena_current)
		// align the next node for pointers (assuming pointers have 2^n size)
		+ ((n_bytes & (sizeof(void*) - 1))? (n_bytes | (sizeof(void*) - 1)) + 1 : n_bytes)
	);
	return result;
}

static AST_Node* node_create(Parser self, NodeType type) {
	if (type >= NODE_MAX || type <= NODE_EMPTY) return 0;
	AST_Node* node = arena_alloc(self, size_table[type]);
	node->node_type = type;
	node->src_file = self->src;
	return node;
}

// Local utility macros

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

#define TOP() (*lexer_peek_token(self->lex, 0))
#define LOOKAHEAD(n) (*lexer_peek_token(self->lex, n))
#define POP() (*lexer_pop_token(self->lex))
// #define TOP0() (*lexer_peek_token_no_eol(self->lex, 0))
// #define LOOKAHEAD0(n) (*lexer_peek_token_no_eol(self->lex, n))
// #define POP0() (*lexer_pop_token_no_eol(self->lex))
// #define TOPX() (allow_eol? (*lexer_peek_token_no_eol(self->lex, 0)) : (*lexer_peek_token(self->lex, 0)))
// #define LOOKAHEADX(n) (allow_eol? (*lexer_peek_token_no_eol(self->lex, n)) : (*lexer_peek_token(self->lex, n)))
// #define POPX() (allow_eol? (*lexer_pop_token_no_eol(self->lex)) : (*lexer_pop_token(self->lex)))
#define CONSUME_EOLS() + + +

#define NEW_NODE(N, T) \
	struct T* N = node_create(self, T); \
	do { \
		Token* _top_token_ = lexer_peek_token(self->lex, 0); \
		N->start_line = _top_token_->start_line; \
		N->start_col = _top_token_->start_col; \
	} while (0)

#define NEW_NODE_FROM(N, T, B) \
	struct T* N = node_create(self, T); \
	do { \
		N->start_line = B->start_line; \
		N->start_col = B->start_col; \
	} while (0)

#define SYNTAX_WARNING(fmt, ...) do { \
	Token* _top_token_ = lexer_peek_token(self->lex, 0); \
	const char** lines = lexer_get_lines(self->lex, 0); \
	if (RULE_DEBUG) fprintf(stderr, "(Emitted from rule '%s' @ %s:%d)\n", __func__, __FILE__, __LINE__); \
	fprintf(stderr, "Syntax warning in '%s' on line %d, column %d: " fmt "\n", \
		self->src , _top_token_->start_line, _top_token_->start_col, ##__VA_ARGS__ ); \
	show_error_line(stderr, \
		lines[_top_token_->start_line - 1], _top_token_->start_line, _top_token_->start_col, \
		(_top_token_->end_line > _top_token_->start_line)? \
			strlen(lines[_top_token_->start_line - 1]) \
			: _top_token_->end_col); \
	self->warning_count++; \
} while (0)

#define SYNTAX_ERROR_NONFATAL(fmt, ...) do { \
	Token* _top_token_ = lexer_peek_token(self->lex, 0); \
	const char** lines = lexer_get_lines(self->lex, 0); \
	if (RULE_DEBUG) fprintf(stderr, "(Emitted from rule '%s' @ %s:%d)\n", __func__, __FILE__, __LINE__); \
	fprintf(stderr, "Syntax error in '%s' on line %d, column %d: " fmt "\n", \
		self->src , _top_token_->start_line, _top_token_->start_col, ##__VA_ARGS__ ); \
	show_error_line(stderr, \
		lines[_top_token_->start_line - 1], _top_token_->start_line, _top_token_->start_col, \
		(_top_token_->end_line > _top_token_->start_line)? \
			strlen(lines[_top_token_->start_line - 1]) \
			: _top_token_->end_col); \
	self->error_count++; \
} while (0)

#define SYNTAX_ERROR(fmt, ...) do { \
	SYNTAX_ERROR_NONFATAL(fmt, ##__VA_ARGS__); \
	return 0; \
} while (0)

#define _token_ _top_token_->literal_text  // the literal text of the top token (for syntax errors)

#define EXPECT(TTYPE, fmt, ...) do { \
	if (TOP().type != TTYPE) SYNTAX_ERROR(fmt, ##__VA_ARGS__); \
} while (0)

#define APPLY(X, RULE, ...) do { \
	X = RULE(self, ##__VA_ARGS__); \
	if (!X) return 0; \
} while (0)

#define APPEND(ARR, RULE, ...) do { \
	AST_Node* _result_ = RULE(self, ##__VA_ARGS__); \
	if (!_result_) return 0; \
	arrpush(ARR, _result_); \
} while (0)

#define FINISH(N) do { \
	Token* _prev_token_ = lexer_peek_token(self->lex, -1); \
	if (_prev_token_->type) { \
		N->end_line = _prev_token_->end_line; \
		N->end_col = _prev_token_->end_col; \
	} \
	else { \
		N->end_line = N->start_line; \
		N->end_col = N->start_col; \
	} \
} while (0)

#define RETURN(N) do { \
	FINISH(N); \
	return N; \
} while (0)

// Disable warnings about switches using values from other enums
#pragma GCC diagnostic ignored "-Wswitch"

#include "rule_prototypes.gen.h"

#include "rules/atoms.h"
#include "rules/toplevel.h"
#include "rules/expr.h"
#include "rules/type.h"

static AST_Function* func_def(Parser self) {
	return 0;
}

AST_Node* parser_execute(Parser self) {
	NEW_NODE(module, NODE_MODULE);
	bool is_pub = false;

	#define APPEND_DECL(RULE) \
		if (is_pub) { \
			APPEND(module->public_decls, RULE); \
		} \
		else { \
			APPEND(module->private_decls, RULE); \
		}

	while (1) {
		Token tok = TOP();
		switch (tok.type) {
			case KW_PUB:
				if (is_pub) SYNTAX_WARNING("Repeated 'pub'");
				is_pub = true;
				break;
			case KW_IMPORT: {
				if (is_pub) SYNTAX_ERROR_NONFATAL("'pub' cannot be applied to import statements");
				APPEND(module->imports, import);
			} break;
			// case KW_FUNC: APPEND_DECL(func_def);
			case KW_CONST: {
				POP(); // 'const'
				if (TOP().type == TOK_LBRACE) {
					// multiple constants
					POP(); // '{'
					EXPECT(TOK_EOL, "Expected end of line to begin const block.");
					POP(); // EOL
					while (TOP().type != TOK_RBRACE) {
						if (TOP().type == TOK_EOL) { // skip empty lines
							POP();
							continue;
						}
						APPEND_DECL(const_def);
						EXPECT(TOK_EOL, "Expected end of line after block constant");
						POP(); // EOL
					}
					POP(); // '}'
					EXPECT(TOK_EOL, "Expected end of line after const block");
				}
				else {
					APPEND_DECL(const_def);
					EXPECT(TOK_EOL, "Expected end of line after const");
				}
				is_pub = false;
			} break;
			// case KW_STRUCT: APPEND_DECL(struct_def);
			// case KW_TABLE: APPEND_DECL(table_def);
			case TOK_RPAREN: SYNTAX_ERROR_NONFATAL("Unmatched parenthesis"); break;
			case TOK_RBRACE: SYNTAX_ERROR_NONFATAL("Unmatched curly brace"); break;
			case TOK_RSQUARE: SYNTAX_ERROR_NONFATAL("Unmatched square bracket"); break;
			case TOK_EOL:
				if (is_pub) {
					SYNTAX_ERROR_NONFATAL("'pub' must by followed by a top-level declaration");
					is_pub = false;
				}
				break;
			case TOK_EOF:
				if (is_pub) SYNTAX_ERROR("'pub' must be followed by a top-level declaration");
				if (self->error_count) return 0;
				RETURN(module);
			default:
				SYNTAX_ERROR_NONFATAL("Top level declarations cannot begin with %s", _token_);
				do { POP(); } while (TOP().type != TOK_EOL && TOP().type != TOK_EOF);
				POP();
		}
		POP();
	}

	fprintf(stderr, "This code should be unreachable: %s, line %d", __func__, __LINE__);
	return 0;
}

// === AST Printing ===

#include "ast_print.impl.gen.h"

void print_ast(FILE* stream, const AST_Node* root) {
	fprintf(stream, "From '%s':\n  ", root->src_file);
	print_ast_node(stream, root, 1);
}
