
#define UNARY_PRECEDENCE 100
#define CMP_PRECEDENCE 10
#define NOT_PRECEDENCE 8
#define AND_PRECEDENCE 6
#define OR_PRECEDENCE 4

static int precedence_of(char first_char) {
    switch (first_char) {
        case '^':
            return 9001;  // this is actually special-cased due to being right-associative and tigher than unary
        case '*': case '/': case '%': case '&':
            return 50;
        case '+': case '-': case '~':
            return 40;
        default:  // word operators
            return 30;
        case '|':
            return 20;
        case '=': case '>': case '<':
            return 10;
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
    do {
        switch (TOP().type) {
            case TOK_INT:
            case TOK_FLOAT:
            case TOK_BOOL:
            case TOK_STRING:
            case TOK_CHAR:
            case TOK_IDENT:
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
            case TOK_CUSTOM_OPERATOR:
                if (sub_expr) {
                    char first_char = TOP().literal_text[0];
                    int precedence = precedence_of(first_char);
                    if (first_char == '^' || precedence > precedence_before) {
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

            case KW_NOT:
                if (sub_expr) SYNTAX_ERROR("Unexpected boolean 'not'");
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
