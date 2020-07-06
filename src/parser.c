
#include <string.h>
#include <stdio.h>

#include "lexer.h"
#include "parser.h"
#include "stretchy_buffer.h"
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

static AST_Node* node_create(Parser self, NodeType type) {
	if (type >= NODE_MAX || type <= NODE_EMPTY) return 0;
	size_t sz = size_table[type];
	if (!self->arena_current || self->arena_current - sb_last(self->arenas) + sz > ARENA_SIZE) {
		self->arena_current = calloc(ARENA_SIZE, 1);  // ensure all nodes from this arena are zeroed out
		if (!self->arena_current) return 0;
		sb_push(self->arenas, self->arena_current);
	}
	AST_Node* node = self->arena_current;
	self->arena_current = (void*) (
		((char*) self->arena_current)
		// align the next node for pointers (assuming 2^n size)
		+ ((sz & (sizeof(void*) - 1))? (sz | (sizeof(void*) - 1)) + 1 : sz)
	);
	node->type = type;
	node->src_file = self->src;
	return node;
}

// static void print_current_line(Parser self) {
// 	const char** lines = lexer_get_lines(self->lex, 0);
// 	printf("%s\n", lines[lexer_peek_token(self->lex, 0).start_line - 1]);
// }

// Local utility macros

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

#define TOP() lexer_peek_token(self->lex, 0)
#define LOOKAHEAD(lh) lexer_peek_token(self->lex, lh)
#define POP() lexer_pop_token(self->lex)

#define NEW_NODE(N, T) \
	struct T* N = node_create(self, T); \
	do { \
		Token _top_token_ = TOP(); \
		N->start_line = _top_token_.start_line; \
		N->start_col = _top_token_.start_col; \
	} while (0)

#define SYNTAX_ERROR(fmt, ...) do { \
	Token _top_token_ = TOP(); \
	const char** lines = lexer_get_lines(self->lex, 0); \
	if (RULE_DEBUG) fprintf(stderr, "(From rule '%s')\n", __func__); \
	fprintf(stderr, "Syntax error in '%s' on line %d, column %d: " fmt "\n", \
		self->src , _top_token_.start_line, _top_token_.start_col, ##__VA_ARGS__ ); \
	show_error_line(stderr, \
		lines[_top_token_.start_line - 1], _top_token_.start_line, _top_token_.start_col, \
		(_top_token_.end_line > _top_token_.start_line)? \
			strlen(lines[_top_token_.start_line - 1]) \
			: _top_token_.end_col); \
	return 0; \
} while (0)
#define _token_ _top_token_.literal_text  // the literal text of the top token (for syntax errors)

// Disable warnings about switches using values from other enums
#pragma GCC diagnostic ignored "-Wswitch"

static AST_Qualname* qualname(Parser self) {
	NEW_NODE(qn, NODE_QUALNAME);
	if (TOP().type != TOK_IDENT) SYNTAX_ERROR("Unexpected token in qualified name: %s", _token_);
	do {
		sb_push(qn->parts, POP().str_value);
		if (TOP().type != TOK_DOT) return qn;
		POP();
	} while (1);
}

static AST_Import* import(Parser self) {
	NEW_NODE(imp, NODE_IMPORT);
	POP(); // import
	Token tok = TOP();
	switch (tok.type) {
		case TOK_IDENT: break; // valid, but not sure which form this is
		case TOK_EOL: SYNTAX_ERROR("import statement is missing its target");
		default: SYNTAX_ERROR("Invalid target of import");
	}
	// Note: first token is an identifier
	switch (LOOKAHEAD(1).type) {
		case TOK_EOL:
		case TOK_DOT: {  // qualified name form
			imp->qualified_name = qualname(self);  // TODO: decide what should be imported
			if (!imp->qualified_name) return 0;
			if (TOP().type != TOK_EOL) SYNTAX_ERROR("Expected end-of-line after qualified name import");
			return imp;
		}
		case TOK_ASSIGN: {  // localname form
			imp->local_name = POP().str_value;
			POP();  // =
			switch(TOP().type) {
				case TOK_STRING:
					imp->imported_file = POP().str_value;
					if (TOP().type != TOK_EOL) SYNTAX_ERROR("Expected end-of-line after pathname import");
					return imp;
				case TOK_IDENT:
					imp->qualified_name = qualname(self);
					if (!imp->qualified_name) return 0;
					if (TOP().type != TOK_EOL) SYNTAX_ERROR("Expected end-of-line after localized import");
					return imp;
				case TOK_EOL: SYNTAX_ERROR("localized import statement is missing its target");
				default: SYNTAX_ERROR("Invalid target of localized import");
			}
			break;
		}
		default: SYNTAX_ERROR("");
	}
}

AST_Node* parser_execute(Parser self) {
	NEW_NODE(module, NODE_MODULE);

	bool is_pub = false;
	while (1) {
		Token tok = TOP();
		switch (tok.type) {
			case KW_PUB:
				if (is_pub) SYNTAX_ERROR("Repeated 'pub'");
				is_pub = true;
				break;
			case KW_IMPORT: {
				if (is_pub) SYNTAX_ERROR("'pub' cannot be applied to import statement");
				AST_Import* imp = import(self);
				if (!imp) return 0;
				sb_push(module->imports, imp);
			} break;
			case TOK_EOL:
				if (is_pub) SYNTAX_ERROR("'pub' must by followed by a top-level declaration");
				break;
			default: SYNTAX_ERROR("Unexpected token: %s", _token_);
		}
		POP();
	}

	return (AST_Node*) module;
}
