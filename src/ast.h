#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

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
    NodeTag* tags; \
    unsigned int start_line, start_col, end_line, end_col;

typedef struct _empty_node {
    AST_NODE_COMMON_FIELDS
} AST_Node;

#include "ast_nodes.h"

void print_ast(FILE* stream, const AST_Node* root);
