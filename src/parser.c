
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wunused-value"

// #pragma GCC diagnostic push
#define TOP() (*lexer_peek_token(self->lex, 0))
#define LOOKAHEAD(n) (*lexer_peek_token(self->lex, n))
#define POP() (*lexer_pop_token(self->lex))
// #pragma GCC diagnostic pop

#define NEW_NODE(N, T) \
	struct T* N = node_create(self, T); \
	do { \
		const Token* _top_token_ = lexer_peek_token(self->lex, 0); \
		N->start_line = _top_token_->start_line; \
		N->start_col = _top_token_->start_col; \
	} while (0)

#define NEW_NODE_FROM(N, T, B) \
	struct T* N = node_create(self, T); \
	do { \
		N->start_line = B->start_line; \
		N->start_col = B->start_col; \
	} while (0)

#define OUTPUT_ERROR(l0, c0, l1, c1, err_type, fmt, ...) do { \
	const char** lines = lexer_get_lines(self->lex, 0); \
	if (RULE_DEBUG) fprintf(stderr, "(Emitted from rule '%s' @ %s:%d)\n", __func__, strrchr(__FILE__, '/') + 1, __LINE__); \
	fprintf(stderr, err_type " in '%s' at line %d, column %d: " fmt "\n", \
		self->src, (l0), (c0), ##__VA_ARGS__ ); \
	show_error_line(stderr, \
		lines[(l0) - 1], (l0), (c0), \
		((l1) > (l0))? strlen(lines[(l0) - 1]) : (c1)); \
} while (0)

#define SYNTAX_WARNING(fmt, ...) do { \
	const Token* _top_token_ = lexer_peek_token(self->lex, 0); \
	OUTPUT_ERROR( \
		_top_token_->start_line, _top_token_->start_col, \
		_top_token_->end_line, _top_token_->end_col, \
		"Syntax warning", fmt, ##__VA_ARGS__); \
	self->warning_count++; \
} while (0)

#define SYNTAX_ERROR_NONFATAL(fmt, ...) do { \
	const Token* _top_token_ = lexer_peek_token(self->lex, 0); \
	OUTPUT_ERROR( \
		_top_token_->start_line, _top_token_->start_col, \
		_top_token_->end_line, _top_token_->end_col, \
		"Syntax error", fmt, ##__VA_ARGS__); \
	self->error_count++; \
} while (0)

#define SYNTAX_ERROR(fmt, ...) do { \
	SYNTAX_ERROR_NONFATAL(fmt, ##__VA_ARGS__); \
	return 0; \
} while (0)

#define SYNTAX_ERROR_FROM_NONFATAL(X, fmt, ...) do { \
	OUTPUT_ERROR( \
		(X)->start_line, (X)->start_col, \
		(X)->end_line, (X)->end_col, \
		"Syntax error", fmt, ##__VA_ARGS__); \
	self->error_count++; \
} while (0)

#define _token_ _top_token_->literal_text  // the literal text of the top token (for syntax errors)

#define EXPECT(TTYPE, fmt, ...) do { \
	if (TOP().type != TTYPE) SYNTAX_ERROR(fmt, ##__VA_ARGS__); \
} while (0)

#define CONSUME(TTYPE, fmt, ...) do { \
	if (TOP().type != TTYPE) SYNTAX_ERROR(fmt, ##__VA_ARGS__); \
	POP(); \
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
	const Token* _prev_token_ = lexer_peek_token(self->lex, -1); \
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
#include "rules/expr.h"
#include "rules/type.h"
#include "rules/toplevel.h"

AST_Node* parser_execute(Parser self) {
	NEW_NODE(module, NODE_MODULE);
	sh_new_arena(module->scope);
	bool is_pub = false;
	while (TOP().type != TOK_EOF) {
		if (!toplevel_item(self, module)) {
			lexer_seek_toplevel(self->lex);
		}
	}
	if (self->error_count) return NULL;
	return module;
}

// === AST Printing ===

#include "ast_print.impl.gen.h"

void print_ast(FILE* stream, const AST_Node* root) {
	fprintf(stream, "From '%s':\n  ", root->src_file);
	print_ast_node(stream, root, 1);
}
