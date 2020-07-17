
#define END_STMT(retval, fmt, ...) do { \
	FINISH(retval); \
	switch (TOP().type) { \
		case TOK_EOL: \
			POP(); \
		case TOK_RBRACE: \
			return retval; \
		default: \
			SYNTAX_ERROR("Expected end-of-line or '}' at end of " fmt, ##__VA_ARGS__); \
	} \
} while (0);

static AST_Node* statement(Parser self) {
	AST_Node* stmt = 0;
	while (!stmt) {
		switch (TOP().type) {
			case TOK_EOL: POP(); return NULL;

			case KW_IF:    return if_stmt(self);
			case KW_WHILE: return while_loop(self);
			case KW_FOR:   return for_loop(self);
			case TOK_LBRACE: return block(self);

			case TOK_IDENT:
				if (LOOKAHEAD(1).type == TOK_COLON) {
					return declaration(self);
				}
				// Drop-thru is intentional
			default: {
				AST_Node* expression = expr(self, 0);
				if (expression) {
					switch (TOP().type) {
						case TOK_ASSIGN:
							return assignment(self, expression);
						case TOK_COMMA: // maybe there's multiple things that can happen here?
							return assign_many(self, expression);
						case TOK_PLUS:
						case TOK_MINUS:
						case TOK_STAR:
						case TOK_SLASH:
						case TOK_TILDE:
						case TOK_PERCENT:
						case TOK_CARET:
						case TOK_AMP:
						case TOK_BAR:
						case TOK_CUSTOM_OPERATOR:
							return op_assignment(self, expression);
						case TOK_EOL:
							POP();
							return expression;
						default:
							SYNTAX_ERROR("Expected end of line or assignment here");
					}
				}
				else return NULL;
			}
		}
	}
}

static AST_VarDecl* declaration(Parser self) {
	NEW_NODE(var, NODE_VAR_DECL);

	EXPECT(TOK_IDENT, "Expected name of variable");
	APPLY(var->name, simple_name);

	CONSUME(TOK_COLON, "Expected colon in variable declaration");
	if (TOP().type != TOK_ASSIGN) {
		APPLY(var->type, type, 0);
		EXPECT(TOK_ASSIGN, "Expected '=' after type");
	}
	switch (TOP().type) {
		case TOK_EOL:
			break;
		case TOK_ASSIGN:
			POP();  // '='
			APPLY(var->value, expr, 0);
			break;
		default: SYNTAX_ERROR("Unexpected token after type of variable declaration");
	}
	END_STMT(var, "variable declaration");
}

static AST_Block* block(Parser self) {
	NEW_NODE(blk, NODE_BLOCK);
	POP();  // '{'
	int errors_before = self->error_count;
	while (TOP().type != TOK_RBRACE) {
		AST_Node* stmt = statement(self);
		if (stmt) {
			arrpush(blk->body, stmt);
		}
		else if (self->error_count > errors_before) {
			if (RULE_DEBUG) printf("Syntax error from previous statement. Discarding rest of block.\n");
			int depth = 0;
			while (1) {
				switch (POP().type) {
					case TOK_LBRACE: depth++; break;
					case TOK_RBRACE: depth--; break;
					case TOK_EOL: if (depth == 0) return NULL;
				}
				if (depth < 0) return NULL;
			}
			return NULL;
		}
	}
	CONSUME(TOK_RBRACE, "Expected end of block");
	RETURN(blk);
}

static AST_IfStatement* if_stmt(Parser self) {
	NEW_NODE(cond, NODE_IF_STMT);
	POP();  // 'if'
	APPLY(cond->condition, expr, 0);
	while (TOP().type == TOK_EOL) POP();  // support ALL the brace styles
	EXPECT(TOK_LBRACE, "Expected if condition to be followed by a block");
	APPLY(cond->body, block);
	while (TOP().type == TOK_EOL) POP();
	if (TOP().type == KW_ELSE) {
		POP();
		while (TOP().type == TOK_EOL) POP();
		switch(TOP().type) {
			case KW_IF:
				APPLY(cond->alternative, if_stmt);
				break;
			case TOK_LBRACE:
				APPLY(cond->alternative, block);
				break;
			default: SYNTAX_ERROR("Expected 'if' or '{' after 'else'");
		}
	}
	RETURN(cond);
}

static AST_WhileLoop* while_loop(Parser self) {
	NEW_NODE(loop, NODE_WHILE_LOOP);
	POP();  // 'while'
	APPLY(loop->condition, expr, 0);
	while (TOP().type == TOK_EOL) POP();  // support ALL the brace styles
	EXPECT(TOK_LBRACE, "Expected while condition to be followed by a block");
	APPLY(loop->body, block);
	RETURN(loop);
}

static AST_ForLoop* for_loop(Parser self) {
	NEW_NODE(loop, NODE_FOR_LOOP);
	POP();  // 'for'
	SYNTAX_ERROR("for loops are not yet implemented");
}

static AST_AssignChain* assignment(Parser self, AST_Node* lhs) {
	NEW_NODE_FROM(assign, NODE_ASSIGN, lhs);
	arrpush(assign->dest_exprs, lhs);
	while (1) {
		POP();  // '='
		AST_Node* part;
		APPLY(part, expr, 0);
		if (TOP().type == TOK_ASSIGN) {
			arrpush(assign->dest_exprs, part);
		}
		else {
			assign->src_expr = part;
			switch (TOP().type) {
				case TOK_PLUS:
				case TOK_MINUS:
				case TOK_STAR:
				case TOK_SLASH:
				case TOK_TILDE:
				case TOK_PERCENT:
				case TOK_CARET:
				case TOK_AMP:
				case TOK_BAR:
				case TOK_CUSTOM_OPERATOR:
					SYNTAX_ERROR("Compound assignments cannot be chained.");
			}
			END_STMT(assign, "assignment%s", (arrlen(assign->dest_exprs) > 1)? " chain" : "");
		}
	}
}

static AST_AssignParallel* assign_many(Parser self, AST_Node* ARRAY lhs) {
	SYNTAX_ERROR("Parallel assignments are not yet implemented");
}

static AST_OpAssign* op_assignment(Parser self, AST_Node* lhs) {
	NEW_NODE_FROM(assign, NODE_OP_ASSIGN, lhs);
	assign->dest_expr = lhs;
	assign->op = POP().literal_text;
	CONSUME(TOK_ASSIGN, "Expected '=' in '%s' compound assignment", assign->op);
	APPLY(assign->src_expr, expr, 0);
	END_STMT(assign, "'%s' compound assignment", assign->op);
}
