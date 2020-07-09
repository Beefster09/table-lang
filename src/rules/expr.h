
// Note: precedence uses odd/even to indicate associativity
//   Even = left associative
//   Odd = right associative

#define REREF_PRECEDENCE   150
#define EXP_PRECEDENCE     101
#define UNARY_PRECEDENCE    99
#define MULDIV_PRECEDENCE   70
#define ADDSUB_PRECEDENCE   60
#define WORD_PRECEDENCE     50
#define ORELSE_PRECEDENCE   40
#define TERNARY_PRECEDENCE  30
#define BAR_PRECEDENCE      20
#define LAMBDA_PRECEDENCE   15
#define CMP_PRECEDENCE      10
#define NOT_PRECEDENCE       8
#define AND_PRECEDENCE       6
#define OR_PRECEDENCE        4
#define SEMICOLON_PRECEDENCE 2

static int precedence_of(char first_char) {
	switch (first_char) {
		case '^':
			return EXP_PRECEDENCE;
		case '*': case '/': case '%': case '&':
			return MULDIV_PRECEDENCE;
		case '+': case '-': case '~':
			return ADDSUB_PRECEDENCE;
		default:  // word operators
			return WORD_PRECEDENCE;
		case '?':
			return ORELSE_PRECEDENCE;
		case '|':
			return BAR_PRECEDENCE;
		case '=': case '>': case '<':
			return CMP_PRECEDENCE;
		case ';':
			return SEMICOLON_PRECEDENCE;
	}
}

static bool is_comparison(int type) {
	switch (type) {
		case TOK_EQ:
		case TOK_NE:
		case TOK_LT:
		case TOK_LE:
		case TOK_GT:
		case TOK_GE:
			return true;
		default:
			return false;
	}
}

static AST_Node* expr(Parser self, bool allow_eol, int precedence_before) {
	AST_Node* sub_expr = 0;
	bool ternary_seen = TERNARY_PRECEDENCE == precedence_before;  // to make ternary non-associative
	do {
		switch (TOP().type) {
			case TOK_IDENT:
				if (!sub_expr && LOOKAHEAD(1).type == TOK_ARROW) {
					APPLY(sub_expr, lambda, allow_eol);
				}

			case TOK_INT:
			case TOK_FLOAT:
			case TOK_BOOL:
			case TOK_STRING:
			case TOK_CHAR:
				if (sub_expr) SYNTAX_ERROR("Unexpected atom in expression");
				APPLY(sub_expr, atom, allow_eol);
				break;

			case TOK_BACKSLASH:  // word operator
				if (sub_expr) {
					POP();  // '\'
					EXPECT(TOK_IDENT, "Expected qualified name here (for word operator)");
					APPLY(sub_expr, word_op, sub_expr, allow_eol);
				}
				else {
					SYNTAX_ERROR("Unexpected backslash");
				}

			case TOK_PLUS:
			case TOK_MINUS:
			case TOK_STAR:
			case TOK_SLASH:
			case TOK_TILDE:
			case TOK_PERCENT:
			case TOK_CARET:
			case TOK_AMP:
			case TOK_BAR:
			case TOK_QMARK:
			case TOK_SEMICOLON:
			case TOK_CUSTOM_OPERATOR:
				if (sub_expr) {
					char first_char = TOP().literal_text[0];
					int precedence = precedence_of(first_char);
					if (precedence >= (precedence_before | 1)) {
						NEW_NODE_FROM(op, NODE_BINOP, sub_expr);
						op->lhs = sub_expr;
						op->op = POP().literal_text;
						APPLY(op->rhs, expr, allow_eol, precedence_of(op->op[0]));
						FINISH(op);
						sub_expr = op;
					}
					else RETURN(sub_expr);
				}
				else {
					NEW_NODE(op, NODE_UNARY);
					op->op = POP().literal_text;
					APPLY(op->expr, expr, allow_eol, UNARY_PRECEDENCE);
					FINISH(op);
					sub_expr = op;
				}
				break;

			case TOK_AT:
				if (sub_expr) SYNTAX_ERROR("Re-referencing must occur before a value");
				else {
					NEW_NODE(reref, NODE_REREFERENCE);
					do {
						POP();  // '@'
						reref->levels++;
						if (allow_eol) { while (TOP().type == TOK_EOL) POP(); }
					}
					while (TOP().type == TOK_AT);
					APPLY(reref->target, expr, allow_eol, REREF_PRECEDENCE);
					FINISH(reref);
					sub_expr = reref;
				}
				break;

			case KW_NOT:
				if (sub_expr) SYNTAX_ERROR("Boolean 'not' must precede a value");
				else {
					POP();  // 'not'
					NEW_NODE(op, NODE_NOT);
					APPLY(op->expr, expr, allow_eol, UNARY_PRECEDENCE);
					FINISH(op);
					sub_expr = op;
				}
				break;

			case KW_AND:
				if (!sub_expr) SYNTAX_ERROR("Boolean 'and' requires an expression to its left");
				else if (AND_PRECEDENCE > precedence_before) {
					POP();  // 'and'
					NEW_NODE_FROM(op, NODE_AND, sub_expr);
					op->lhs = sub_expr;
					APPLY(op->rhs, expr, allow_eol, AND_PRECEDENCE);
					FINISH(op);
					sub_expr = op;
				}
				else RETURN(sub_expr);
				break;

			case KW_OR:
				if (!sub_expr) SYNTAX_ERROR("Boolean 'or' requires an expression to its left");
				else if (OR_PRECEDENCE > precedence_before) {
					POP();  // 'or'
					NEW_NODE_FROM(op, NODE_OR, sub_expr);
					op->lhs = sub_expr;
					APPLY(op->rhs, expr, allow_eol, OR_PRECEDENCE);
					FINISH(op);
					sub_expr = op;
				}
				else RETURN(sub_expr);
				break;

			case TOK_EQ:
			case TOK_NE:
			case TOK_LT:
			case TOK_LE:
			case TOK_GT:
			case TOK_GE:
				if (!sub_expr) SYNTAX_ERROR("Comparison operator is missing left side expression");
				else if (CMP_PRECEDENCE > precedence_before) {
					NEW_NODE_FROM(chain, NODE_COMPARISON, sub_expr);
					sb_push(chain->operands, sub_expr);
					do {
						const char* cmp = POP().literal_text;  // TODO: consider enum representations
						sb_push(chain->comparisons, cmp);
						APPEND(chain->operands, expr, allow_eol, CMP_PRECEDENCE);
					} while (is_comparison(TOP().type));
					FINISH(chain);
					sub_expr = chain;
				}
				else RETURN(sub_expr);
				break;

			case KW_IF:
				if (!sub_expr) SYNTAX_ERROR("'if' requires a preceding sub-expression in an expression context");
				else if (TERNARY_PRECEDENCE > precedence_before) {
					if (ternary_seen) SYNTAX_ERROR("ternary is non-associative");
					NEW_NODE_FROM(ternary, NODE_TERNARY, sub_expr);
					ternary->true_expr = sub_expr;
					POP();  // 'if'
					APPLY(ternary->condition, expr, allow_eol, 0);
					EXPECT(KW_ELSE, "Expected 'else' after ternary condition");
					POP(); // 'else'
					APPLY(ternary->false_expr, expr, allow_eol, TERNARY_PRECEDENCE);
					FINISH(ternary);
					sub_expr = ternary;
					ternary_seen = true;
				}
				else RETURN(sub_expr);
				break;

			case KW_ELSE:
				if (!sub_expr || TERNARY_PRECEDENCE < precedence_before) SYNTAX_ERROR("Unexpected 'else' in expression");
				else RETURN(sub_expr);

			case TOK_LPAREN:
				if (sub_expr) {
					POP();  // '('
					APPLY(sub_expr, func_call, sub_expr);
					EXPECT(TOK_RPAREN, "Expected ')' at end of parenthesized sub-expression");
					POP();  // ')'
				}
				else {
					POP();  // '('
					APPLY(sub_expr, expr, true, 0);
					EXPECT(TOK_RPAREN, "Expected ')' at end of parenthesized sub-expression");
					POP();  // ')'
				}
				FINISH(sub_expr);
				break;

			case TOK_LSQUARE:
				if (sub_expr) {
					POP();  // '['
					APPLY(sub_expr, subscript, sub_expr);
					EXPECT(TOK_RSQUARE, "Expected ']' at end of array literal");
					POP();  // ']'
				}
				else {
					POP();  // '['
					APPLY(sub_expr, array_literal);
					EXPECT(TOK_RSQUARE, "Expected ']' at end of array literal");
					POP();  // ']'
				}
				FINISH(sub_expr);
				break;

			case TOK_DOT:
				if (!sub_expr) SYNTAX_ERROR("Expected value before field access");
				else {
					POP();  // '.'
					NEW_NODE_FROM(field_access, NODE_FIELD_ACCESS, sub_expr);
					field_access->base = sub_expr;
					APPLY(field_access->field, qualname, allow_eol);
					sub_expr = field_access;
					FINISH(sub_expr);
				}
				break;

			case TOK_EOL:
				if (allow_eol) {
					POP();
					break;
				}
				// Drop through is intentional
			case TOK_RPAREN:
			case TOK_RBRACE:
			case TOK_RSQUARE:
				if (sub_expr) RETURN(sub_expr);
				// Drop through is intentional
			default: SYNTAX_ERROR("Expected an expression here");
		}
	} while (1);
}


static AST_FuncCall* word_op(Parser self, AST_Node* left_side, bool allow_eol) {
	SYNTAX_ERROR("Word operators are not implemented yet");
}

static AST_Array* array_literal(Parser self) {
	SYNTAX_ERROR("Array literals are not implemented yet");
}

static AST_FuncCall* func_call(Parser self, AST_Node* func) {
	SYNTAX_ERROR("Function calls are not implemented yet");
}

static AST_Subscript* subscript(Parser self, AST_Node* array) {
	SYNTAX_ERROR("Subscripts are not implemented yet");
}

static AST_FuncCall* lambda(Parser self, bool allow_eol) {
	NEW_NODE(anon_func, NODE_FUNC);
	SYNTAX_ERROR("Lambdas are not implemented yet (and might never be?)");
}
