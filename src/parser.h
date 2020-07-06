#pragma once
#include <stdint.h>

#include "ast.h"

typedef struct _parse_state* Parser;

Parser parser_create(const char* filename);
void parser_destroy(Parser parser);

AST_Node* parser_execute(Parser parser);
