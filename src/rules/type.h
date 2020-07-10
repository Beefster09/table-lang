

#define POINTER_PRECEDENCE 100
#define ARRAY_PRECEDENCE    50
#define UNION_PRECEDENCE    20
#define FUNCTYPE_PRECEDENCE 15



#define FUNC_TYPE_RHS() do { \
	POP();  /* '=>' */ \
	switch (TOP().type) { \
		default: \
			break; \
		case TOK_LPAREN: \
			if (LOOKAHEAD(1).type == TOK_RPAREN && LOOKAHEAD(2).type != TOK_ARROW) { \
				POP();  /* '(' */ \
				POP();  /* ')' */ \
				break; \
			} \
			/* Drop through is intentional */ \
		case TOK_ARROW: \
		case TOK_IDENT: \
		case TOK_AT: \
		case TOK_LSQUARE: \
		case TOK_BAR: \
			APPLY(func->return_type, type, allow_eol, FUNCTYPE_PRECEDENCE); \
			break; \
	} \
} while(0)

static AST_Node* type(Parser self, bool allow_eol, int precedence_before) {
	AST_Node* sub_type = 0;
	while (1) {
		switch(TOP().type) {
			case TOK_IDENT:
				if (sub_type) SYNTAX_ERROR("Unexpected identifier in type");
				else {
					APPLY(sub_type, simple_type, allow_eol);
				}
				break;

			case TOK_AT:
				if (sub_type) SYNTAX_ERROR("Pointer designations must precdede a type");
				else {
					POP();  // '@'
					APPLY(sub_type, pointer_type, allow_eol);
				}
				break;

			case TOK_LSQUARE:
				if (!sub_type) SYNTAX_ERROR("Array types require an element type to the left");
				else if (ARRAY_PRECEDENCE > precedence_before) {
					APPLY(sub_type, array_type, sub_type, allow_eol);
				}
				else return sub_type;
				break;

			case TOK_BAR:
				if (!sub_type) SYNTAX_ERROR("Union type chain requires a type to the left");
				else if (UNION_PRECEDENCE > precedence_before) {
					NEW_NODE_FROM(union_chain, NODE_UNION, sub_type);
					arrpush(union_chain->variants, sub_type);
					do {
						POP();  // '|'
						APPEND(union_chain->variants, type, allow_eol, UNION_PRECEDENCE);
					} while (TOP().type == TOK_BAR);
					FINISH(union_chain);
					sub_type = union_chain;
				}
				else return sub_type;
				break;

			case TOK_ARROW:
				if (FUNCTYPE_PRECEDENCE >= precedence_before) {
					AST_FuncType* func = node_create(self, NODE_FUNC_TYPE);
					if (sub_type) {
						func->start_line = sub_type->start_line;
						func->start_col = sub_type->start_col;
						arrpush(func->param_types, sub_type);
					}
					else {
						Token t = TOP();
						func->start_line = t.start_line;
						func->start_col = t.start_col;
					}
					FUNC_TYPE_RHS();
					FINISH(func);
					sub_type = func;
				}
				else return sub_type;
				break;

			case TOK_LPAREN:
				if (sub_type) SYNTAX_ERROR("Template type not valid here");
				else if (LOOKAHEAD(1).type == TOK_RPAREN && LOOKAHEAD(2).type == TOK_ARROW) {
					NEW_NODE(func, NODE_FUNC_TYPE);
					POP();  // '('
					POP();  // ')'
					FUNC_TYPE_RHS();
					FINISH(func);
					sub_type = func;
				}
				else {
					POP();  // '('
					APPLY(sub_type, type, true, precedence_before & 1);
					if (TOP().type == TOK_COMMA) {
						// Oh boy! We have a function type!
						NEW_NODE_FROM(func, NODE_FUNC_TYPE, sub_type);
						arrpush(func->param_types, sub_type);
						do {
							POP();  // ','
							APPEND(func->param_types, type, true, 0);
						} while (TOP().type == TOK_COMMA);
						EXPECT(TOK_RPAREN, "Expected end of parameter type list here");
						POP();  // ')'
						EXPECT(TOK_ARROW, "Expected function arrow here");
						POP();  // '=>'
						if (TOP().type == TOK_LPAREN && LOOKAHEAD(1).type == TOK_RPAREN) {
							POP();
							POP();
						}
						else {
							APPLY(func->return_type, type, allow_eol, FUNCTYPE_PRECEDENCE);
						}
						sub_type = func;
					}
					else {
						EXPECT(TOK_RPAREN, "Expected matching parenthesis here");
						POP();  // ')'
					}
					FINISH(sub_type);
				}
				break;

			case TOK_EOL:
				if (allow_eol) {
					POP();
					break;
				}
				// drop through is intentional
			default:
				if (sub_type) return sub_type;
				else SYNTAX_ERROR("Expected a type here");
		}
	}
}

static AST_SimpleType* simple_type(Parser self, bool allow_eol) {
	NEW_NODE(simple, NODE_SIMPLE_TYPE);
	APPLY(simple->base, qualname, allow_eol);
	while (1) {
		switch (TOP().type) {
			case TOK_QMARK:
				if (simple->is_nullable) SYNTAX_WARNING("Duplicated '?'");
				POP();
				simple->is_nullable = true;
				break;
			case TOK_BANG:
				if (simple->is_mutable) SYNTAX_WARNING("Duplicated '!'");
				POP();
				simple->is_mutable = true;
				break;
			case TOK_EOL:
				if (allow_eol) {
					POP();
					break;
				}
				// drop through is intentional
			default: RETURN(simple);
		}
	}
}

static AST_PointerType* pointer_type(Parser self, bool allow_eol) {
	NEW_NODE(pointer, NODE_POINTER_TYPE);
	while (1) {
		switch (TOP().type) {
			case TOK_QMARK:
				if (pointer->is_nullable) SYNTAX_WARNING("Duplicated '?'");
				POP();
				pointer->is_nullable = true;
				break;
			case TOK_BANG:
				if (pointer->is_mutable) SYNTAX_WARNING("Duplicated '!'");
				POP();
				pointer->is_mutable = true;
				break;
			case TOK_EOL:
				if (allow_eol) {
					POP();
					break;
				}
				else RETURN(pointer);
			default:
				APPLY(pointer->base, type, allow_eol, POINTER_PRECEDENCE);
				RETURN(pointer);
		}
	}
}

static AST_ArrayType* array_type(Parser self, AST_Node* element_type, bool allow_eol) {
	NEW_NODE_FROM(array, NODE_ARRAY_TYPE, element_type);
	array->element_type = element_type;

	POP();  // '['

	while (TOP().type == TOK_EOL) POP();

	switch (TOP().type) {
		case TOK_COLON:  // fixed-dimension array
			POP();
			EXPECT(TOK_INT, "Integer dimensionality required here");
			int dim_count = TOP().int_value;
			if (dim_count == 0) SYNTAX_ERROR_NONFATAL("Arrays cannot be zero-dimensional");
			POP();
			for (int i = 0; i < dim_count; i++) arrpush(array->dimensions, -1);
			break;

		case TOK_RSQUARE:
			array->is_dynamic = true;
			break;

		case TOK_INT: case TOK_STAR:
			do {
				if (TOP().type == TOK_INT) {
					arrpush(array->dimensions, POP().int_value);
				}
				else {
					POP();
					arrpush(array->dimensions, -1);
				}

				while (TOP().type == TOK_EOL) POP();
				if (TOP().type == TOK_RSQUARE) break;
				EXPECT(TOK_COMMA, "Expected comma or end of array dimensions");
				POP();  // ','
				while (TOP().type == TOK_EOL) POP();

			} while (TOP().type == TOK_INT || TOP().type == TOK_STAR);
			break;

		default: SYNTAX_ERROR("Expected dimensionality information or empty square brackets");
	}

	EXPECT(TOK_RSQUARE, "Expected right square bracket to end array dimensions");
	POP();  // ']'

	while (1) {
		switch (TOP().type) {
			case TOK_QMARK:
				if (array->is_nullable) SYNTAX_WARNING("Duplicated '?'");
				POP();
				array->is_nullable = true;
				break;
			case TOK_BANG:
				if (array->is_mutable) SYNTAX_WARNING("Duplicated '!'");
				POP();
				array->is_mutable = true;
				break;
			case TOK_EOL:
				if (allow_eol) {
					POP();
					break;
				}
				// drop through is intentional
			default: RETURN(array);
		}
	}
}
