
#define END_STMT(retval, fmt, ...) do { \
	FINISH(retval); \
	switch (TOP().type) { \
		case TOK_EOL: \
			POP(); /* Drop-thru is intentional */ \
		case TOK_RBRACE: \
			return retval; \
		default: \
			SYNTAX_ERROR("Expected end-of-line or '}' at end of " fmt, ##__VA_ARGS__); \
	} \
	return NULL; /* unreachable */ \
} while (0);

static AST_Node* statement(Parser self) {
	AST_Node* stmt = NULL;
	while (!stmt) {
		switch (TOP().type) {
			case TOK_EOL: POP(); return NULL;

			case KW_IF:    return if_stmt(self);
			case KW_WHILE: return while_loop(self);
			case KW_FOR:   return for_loop(self);
			case KW_WITH:  return with_block(self);
			case KW_MATCH: return pattern_match(self);
			case KW_TYPE:
				if (LOOKAHEAD(1).type == KW_MATCH) {
					type_match(self);
				}
				else return expression(self, 0);

			case KW_RETURN: {
				NEW_NODE(ret, NODE_RETURN);
				POP();
				APPLY(ret->value, expression, 0);
				END_STMT(ret, "return statement");
			}

			case KW_FAIL: {
				NEW_NODE(fail, NODE_FAIL);
				POP();
				if (TOP().type != TOK_COLON) {
					APPLY(fail->value, expression, 0);
				}
				if (TOP().type == TOK_COLON) {
					POP();
					APPLY(fail->message, expression, 0);
				}
				END_STMT(fail, "fail statement");
			}

			case KW_ASSERT: {
				NEW_NODE(assert_stmt, NODE_ASSERT);
				POP();
				APPLY(assert_stmt->value, expression, 0);
				if (TOP().type == TOK_COLON) {
					POP();
					APPLY(assert_stmt->message, expression, 0);
				}
				END_STMT(assert_stmt, "assertion");
			}

			case KW_DEFER: {
				if (LOOKAHEAD(1).type == KW_DEFER) SYNTAX_ERROR("Repeated 'defer'");
				NEW_NODE(defer, NODE_DEFER);
				POP();
				APPLY(defer->code, statement);
				FINISH(defer); // eol already eaten
				return defer;
			}

			case KW_CANCEL: {
				NEW_NODE(cancel, NODE_CANCEL);
				POP();
				APPLY(cancel->target, expression, 0);
				END_STMT(cancel, "cancel statement");
			}

			case KW_BREAK: {
				NEW_NODE(brk, NODE_BREAK);
				POP();
				if (TOP().type == TOK_IDENT) {
					APPLY(brk->label, simple_name);
				}
				END_STMT(brk, "break statement");
			}

			case KW_SKIP: {
				NEW_NODE(skip, NODE_SKIP);
				POP();
				if (TOP().type == TOK_IDENT) {
					APPLY(skip->label, simple_name);
				}
				END_STMT(skip, "skip statement");
			}

			case TOK_LBRACE: return block(self);

			case TOK_IDENT:
				if (LOOKAHEAD(1).type == TOK_COLON) {
					return declaration(self);
				}
				// Drop-thru is intentional
			default: {
				AST_Node* expr = expression(self, 0);
				if (expr) {
					switch (TOP().type) {
						case TOK_ASSIGN:
							return assignment(self, expr);
						case TOK_COMMA: // maybe there's multiple things that can happen here?
							return assign_many(self, expr);
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
							return op_assignment(self, expr);
						case TOK_EOL:
							POP();  // drop through is intentional
						case TOK_RBRACE: // allow inline expressions
							return expr;
						default:
							SYNTAX_ERROR("Expected end of line or assignment here");
					}
				}
				else return NULL;
			}
		}
	}
	return stmt;
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
			APPLY(var->value, expression, 0);
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
	APPLY(cond->condition, expression, 0);
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
	APPLY(loop->condition, expression, 0);
	while (TOP().type == TOK_EOL) POP();  // support ALL the brace styles
	EXPECT(TOK_LBRACE, "Expected while condition to be followed by a block");
	APPLY(loop->body, block);
	RETURN(loop);
}

inline static AST_Node* for_simple_or_range(Parser self, AST_Name* name, AST_Node* expr) {
	AST_Node* base = name? name : expr;
	if (expr) {
		if (TOP().type == TOK_RANGE) {
			NEW_NODE_FROM(range, NODE_FOR_RANGE, base);
			POP();  // '..'
			range->name = name;
			range->start = expr;
			switch (TOP().type) {
				case TOK_LBRACE:
				case TOK_SEMICOLON:
					break;
				default:
					APPLY(range->end, expression, 0);
			}
			RETURN(range);
		}
		else {
			NEW_NODE_FROM(simple, NODE_FOR_SIMPLE, base);
			simple->name = name;
			simple->iterable = expr;
			RETURN(simple);
		}
	}
	else return NULL;
}

static AST_Node* for_range(Parser self) {
	if (TOP().type == TOK_IDENT) {
		switch (LOOKAHEAD(1).type) {
			case TOK_COLON: {
				AST_Name* name = simple_name(self);
				POP();  // ':'
				AST_Node* expr = expression(self, 0);
				return for_simple_or_range(self, name, expr);
			}

			case TOK_COMMA: {
				NEW_NODE(parallel, NODE_FOR_PARALLEL);
				while (1) {
					APPEND(parallel->names, simple_name);
					if (TOP().type == TOK_COMMA) {
						POP();
						if (TOP().type == TOK_COLON) break;  // Allow trailing comma
					}
					else break;
				}
				CONSUME(TOK_COLON, "Expected a colon in zipped for range");
				int n_vars = arrlen(parallel->names);
				for (int i = 0; i < n_vars; i++) {
					AST_Node* iterable = expression(self, 0);
					if (iterable) {
						const char* key = parallel->names[i]->name;
						if (shgeti(parallel->zips, key) >= 0) {
							SYNTAX_ERROR_FROM_NONFATAL(
								parallel->names[i],
								"Repeated variable name '%s' on left side of zipped for range",
								key
							);
						}
						shput(parallel->zips, key, iterable);
					}
					else return NULL;
					if (i + 1 < n_vars) {
						CONSUME(TOK_COMMA, "Expected comma in zipped for range");
					}
					else if (TOP().type == TOK_COMMA) POP(); // Allow trailing comma
				}
				switch (TOP().type) {
					case TOK_LBRACE:
					case TOK_SEMICOLON:
						RETURN(parallel);
					default:
						SYNTAX_ERROR("Unexpected token on right side of zipped for range"
							" (perhaps there are too many expressions)");
				}
			}

			default: break;
		}
	}
	AST_Node* expr = expression(self, 0);
	return for_simple_or_range(self, NULL, expr);
}

static AST_ForLoop* for_loop(Parser self) {
	NEW_NODE(loop, NODE_FOR_LOOP);
	POP();  // 'for'
	while (1) {
		APPEND(loop->iterables, for_range);
		if (TOP().type == TOK_SEMICOLON) {
			POP();  // ';'
			while (TOP().type == TOK_EOL) POP();  // Allow each range to appear on its own line
			if (TOP().type == TOK_LBRACE) break;  // Allow trailing semicolon
		}
		else break;
	}
	while (TOP().type == TOK_EOL) POP();  // support ALL the brace styles
	EXPECT(TOK_LBRACE, "Expected body of for loop");
	APPLY(loop->body, block);
	RETURN(loop);
}

static AST_AssignChain* assignment(Parser self, AST_Node* lhs) {
	NEW_NODE_FROM(assign, NODE_ASSIGN, lhs);
	arrpush(assign->dest_exprs, lhs);
	while (1) {
		POP();  // '='
		AST_Node* part;
		APPLY(part, expression, 0);
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
					SYNTAX_ERROR("Compound assignments cannot be part of an assignment chain.");
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
	APPLY(assign->src_expr, expression, 0);
	END_STMT(assign, "'%s' compound assignment", assign->op);
}

static AST_Node* type_match(Parser self) {
	SYNTAX_ERROR("Type matching not yet supported");
}

static AST_Node* pattern_match(Parser self) {
	SYNTAX_ERROR("Pattern matching not yet supported");
}

static AST_With* with_block(Parser self) {
	SYNTAX_ERROR("'with' blocks not yet supported");
}
