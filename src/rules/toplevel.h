// To be included *only* from parser.c

static AST_Const* const_def(Parser self) {
	NEW_NODE(constant, NODE_CONST);

	EXPECT(TOK_IDENT, "Expected name of constant");
	constant->name = POP().str_value;

	switch (TOP().type) {
		case TOK_COLON:
			POP();  // ':'
			if (TOP().type != TOK_ASSIGN) {
				APPLY(constant->type, type, 0);
				EXPECT(TOK_ASSIGN, "Expected '=' after type");
			}
			// drop through is intentional
		case TOK_ASSIGN:
			POP();  // '='
			APPLY(constant->value, expr, 0);
			break;
		default: SYNTAX_ERROR("Expected ':' or '=' after constant name");
	}

	RETURN(constant);
}

static AST_Node* table_def(Parser self) {
	return 0;
}

static AST_Node* struct_def(Parser self) {
	return 0;
}

static AST_Import* import(Parser self) {
	NEW_NODE(imp, NODE_IMPORT);
	POP(); // import
	switch (TOP().type) {
		case TOK_IDENT: break; // valid, but not sure which form this is
		case TOK_EOL: SYNTAX_ERROR("import statement is missing its target");
		default: SYNTAX_ERROR("Invalid target of import");
	}
	// Note: first token is an identifier
	switch (LOOKAHEAD(1).type) {
		case TOK_EOL:
		case TOK_DOT: {  // qualified name form
			APPLY(imp->qualified_name, qualname);  // TODO: decide what should be imported
			imp->local_name = join_qualname(self, imp->qualified_name);
			EXPECT(TOK_EOL, "Expected end-of-line after qualified name import");
			RETURN(imp);
		}
		case TOK_ASSIGN: {  // localname form
			imp->local_name = POP().str_value;
			POP();  // =
			switch(TOP().type) {
				case TOK_STRING:
					imp->imported_file = POP().str_value;
					EXPECT(TOK_EOL, "Expected end-of-line after pathname import");
					RETURN(imp);
				case TOK_IDENT:
					APPLY(imp->qualified_name, qualname);
					EXPECT(TOK_EOL, "Expected end-of-line after localized import");
					RETURN(imp);
				case TOK_EOL: SYNTAX_ERROR("localized import statement is missing its target");
				default: SYNTAX_ERROR("Invalid target of localized import");
			}
			break;
		}
		default:
			POP();
			SYNTAX_ERROR("Unexpected token in import statement: '%s'", _token_);
	}
}

static AST_Function* func_def(Parser self) {
	return 0;
}

static int toplevel_item(Parser self, AST_Module* module) {
	while (TOP().type == TOK_EOL) POP();  // filter empty lines

	bool is_pub = TOP().type == KW_PUB? (POP(), true) : false;

	switch (TOP().type) {
		case KW_PUB:
			if (is_pub) SYNTAX_ERROR("Repeated 'pub'");
		case KW_IMPORT: {
			if (is_pub) SYNTAX_ERROR_NONFATAL("'pub' cannot be applied to import statements");
			AST_Import* imp = import(self);
			if (imp) {
				if (shgeti(module->scope, imp->local_name) >= 0) {
					SYNTAX_ERROR_NONFATAL("Something named '%s' already exists in this module.", imp->local_name);
				}
				arrpush(module->imports, imp);
				shput(module->scope, imp->local_name, imp);
				return module;
			}
			else return 0;
		} break;
		// case KW_FUNC: APPEND_DECL(func_def);
		case KW_CONST: {
			POP(); // 'const'
			if (TOP().type == TOK_LBRACE) {
				// multiple constants
				POP(); // '{'
				EXPECT(TOK_EOL, "Expected end of line to begin const block.");
				POP(); // EOL
				while (TOP().type != TOK_RBRACE) {
					if (TOP().type == TOK_EOL) { // skip empty lines
						POP();
						continue;
					}
					AST_Const* constant = const_def(self);
					if (constant) {
						if (shgeti(module->scope, constant->name) >= 0) {
							SYNTAX_ERROR_NONFATAL("Something named '%s' already exists in this module.", constant->name);
						}
						else if (is_pub) arrpush(module->exports, constant->name);
						shput(module->scope, constant->name, constant);
					}
					else return 0;
					EXPECT(TOK_EOL, "Expected end of line after block constant");
					POP(); // EOL
				}
				POP(); // '}'
				EXPECT(TOK_EOL, "Expected end of line after const block");
				return 0;
			}
			else {
				AST_Const* constant = const_def(self);
				if (constant) {
					if (shgeti(module->scope, constant->name) >= 0) {
						SYNTAX_ERROR_NONFATAL("Something named '%s' already exists in this module.", constant->name);
					}
					else if (is_pub) arrpush(module->exports, constant->name);
					shput(module->scope, constant->name, constant);
				}
				else return 0;
				EXPECT(TOK_EOL, "Expected end of line after const");
				POP();
				return constant;
			}
		} break;
		// case KW_STRUCT: APPEND_DECL(struct_def);
		// case KW_TABLE: APPEND_DECL(table_def);
		case TOK_RPAREN: SYNTAX_ERROR("Unmatched parenthesis");
		case TOK_RBRACE: SYNTAX_ERROR("Unmatched curly brace");
		case TOK_RSQUARE: SYNTAX_ERROR("Unmatched square bracket");
		case TOK_EOL:
			if (is_pub) {
				SYNTAX_ERROR("'pub' must by followed by a top-level declaration");
			}
			// drop-through is intentional
		case TOK_EOF:
			return 1;
		default:
			SYNTAX_ERROR("Top level scope cannot begin with '%s'", _token_);
	}
	POP();
	return 0;
}
