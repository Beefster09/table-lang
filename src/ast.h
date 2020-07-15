#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// For communicating intent about how node fields are intended to be used
// These also help the node printer generator to work properly
#define MAP *
#define ARRAY *

typedef struct _node_tag NodeTag;

typedef enum _node_type {
	NODE_EMPTY = 0,

	#define XMAC(NAME) NAME,
	#include "ast_node_types.gen.h"
	#undef XMAC

	NODE_MAX,
} NodeType;

#define AST_NODE_COMMON_FIELDS \
	NodeType node_type; \
	const char* src_file; \
	NodeTag ARRAY tags; \
	unsigned int start_line, start_col, end_line, end_col;

typedef struct NODE_EMPTY {
	AST_NODE_COMMON_FIELDS
} AST_Node;

typedef int Rune;

typedef struct {
	const char* key;
	AST_Node* value;
} ASTMAP_NodeEntry;

#include "ast_nodes.h"

void print_ast(FILE* stream, const AST_Node* root);
void ast_to_json(FILE* stream, const AST_Node* root);
