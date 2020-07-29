// To be included *only* from parser.c

#define ADD_ITEM(C, K, V, FMT, ...) do { \
	if ((K)->name[0] == '_' && ((K)->name[1] == '_' || (K)->name[1] == 0)) { \
		SYNTAX_ERROR_FROM(K, "'%s' is a reserved identifier", (K)->name); \
	} \
	else if (shgeti(C, (K)->name) >= 0) { \
		SYNTAX_ERROR_FROM(K, "Something named '%s' already exists in " FMT ".", (K)->name, ##__VA_ARGS__); \
	} \
	shput(C, (K)->name, V); \
} while (0)

#define ADD_DECL(K, V) ADD_ITEM(module->scope, K, V, "module '%s'", self->filename)

static int toplevel_item(Parser self, AST_Module* module) {
	while (TOP().type == TOK_EOL) POP();  // filter empty lines

	bool is_pub = TOP().type == KW_PUB? (POP(), true) : false;

	retry:
	switch (TOP().type) {
		case KW_PUB:
			if (is_pub) {
				SYNTAX_WARNING("Redundant 'pub'");
				POP();
				goto retry;
			}
			else SYNTAX_ERROR("Unexpected 'pub'");

		case KW_IMPORT: {
			if (is_pub) SYNTAX_ERROR_NONFATAL("'pub' cannot be applied to import statements");
			AST_Import* imp = import(self);
			if (imp) {
				if (imp->local_name) {
					ADD_DECL(imp->local_name, imp);
					return 1;
				}
				else if (imp->is_using) {
					char namebuf[32];
					snprintf(namebuf, sizeof(namebuf), "<import_%d>", shlen(module->scope));
					shput(module->scope, namebuf, imp);
				}
			}
			else return 0;
		} break;

		case KW_FUNC: {
			AST_FuncDef* func = func_def(self);
			if (func) {
				if (!func->name) {
					OUTPUT_ERROR(
						func->start_line, func->start_col + 4,
						func->start_line, func->start_col + 4,
						"Error", "This function in module scope does not have a name.");
					self->error_count++;
					return 0;
				}
				ADD_DECL(func->name, func);
				CONSUME(TOK_EOL, "Expected end-of-line after function definition");
				return 1;
			}
			else return 0;
		} break;

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
						constant->pub = is_pub;
						ADD_DECL(constant->name, constant);
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
					constant->pub = is_pub;
					ADD_DECL(constant->name, constant);
				}
				else return 0;
				CONSUME(TOK_EOL, "Expected end of line after const");
				return 1;
			}
		} break;

		case KW_STRUCT: {
			AST_Struct* structure = struct_def(self);
			if (structure) {
				ADD_DECL(structure->name, structure);
				CONSUME(TOK_EOL, "Expected end-of-line after struct definition");
				return 1;
			}
			else return 0;
		} break;
		// case KW_TABLE: APPEND_DECL(table_def);

		case DIR_OVERLOAD: {  // TODO: move this logic to the lexer
			NEW_NODE(overload, NODE_FUNC_OVERLOAD);
			POP();  // '#overload'
			switch (TOP().type) {
				case TOK_IDENT:
				case TOK_PLUS:
				case TOK_MINUS:
				case TOK_STAR:
				case TOK_SLASH:
				case TOK_TILDE:
				case TOK_PERCENT:
				case TOK_CARET:
				case TOK_AMP:
				case TOK_BAR:
				case TOK_EQ:
				case TOK_NE:
				case TOK_LT:
				case TOK_LE:
				case TOK_GT:
				case TOK_GE:
				case TOK_CUSTOM_OPERATOR:
					APPLY(overload->name, simple_name);
					break;
				default:
					SYNTAX_ERROR("Expected an identifier or operator to name the overload");
			}
			// while (TOP().type == TOK_EOL) POP();  // support multiple brace styles
			CONSUME(TOK_LBRACE, "Expected '{' to start overload list");
			while (TOP().type != TOK_RBRACE) {
				switch (TOP().type) {
					case TOK_IDENT:
						APPEND(overload->overloads, simple_name);
						EXPECT(TOK_EOL, "Expected end of line after named overload");
						// intentional drop-thru
					case TOK_EOL:
						POP();
						break;
					default:
						SYNTAX_ERROR("Unexpected '%s' in overload list", _token_);
				}
			}
			POP();  // '}'
			FINISH(overload);
			ADD_DECL(overload->name, overload);
			break;
		}

		case DIR_TEST: {
			AST_Test* test = test_def(self);
			if (test) {
				arrpush(module->tests, test);
				CONSUME(TOK_EOL, "Expected end-of-line after test");
				return 1;
			}
			else return 0;
		} break;

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
			return 0;
	}
	CONSUME(TOK_EOL, "Expected end-of-line after top-level declaration");
	return 1;
}

static AST_Const* const_def(Parser self) {
	NEW_NODE(constant, NODE_CONST);

	EXPECT(TOK_IDENT, "Expected name of constant");
	APPLY(constant->name, simple_name);

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
			APPLY(constant->value, expression, 0);
			break;
		default: SYNTAX_ERROR("Expected ':' or '=' after constant name");
	}

	RETURN(constant);
}

#define ADD_FIELD(M, F, T) ADD_ITEM((M)->fields, (F)->name, F, T " '%s'", (M)->name)

static AST_Node* table_def(Parser self) {
	return 0;
}

static AST_Node* struct_def(Parser self) {
	NEW_NODE(structure, NODE_STRUCT);
	POP();  // 'struct'

	EXPECT(TOK_IDENT, "Expected name of struct");
	APPLY(structure->name, simple_name);

	CONSUME(TOK_LBRACE, "Expected '{' to begin struct");
	CONSUME(TOK_EOL, "Expected end-of-line to begin struct");

	while (true) {
		switch (TOP().type) {
			case TOK_IDENT: {
				NEW_NODE(field, NODE_FIELD);
				APPLY(field->name, simple_name);
				switch (TOP().type) {
					case TOK_COMMA: {
						AST_Field** fields = NULL;  // NOTE: this leaks memory on fatal errors.
						do {
							POP();  // ','
							NEW_NODE(f, NODE_FIELD);
							EXPECT(TOK_IDENT, "Expected an identifier to name this field");
							APPLY(f->name, simple_name);
							arrpush(fields, f);
						} while (TOP().type == TOK_COMMA);
						CONSUME(TOK_COLON, "Expected a type for field list starting with '%s'", field->name->name);
						APPLY(field->type, type, 0);
						if (TOP().type == TOK_ASSIGN) {
							POP();
							APPLY(field->default_value, expression, 0);
							if (TOP().type == TOK_COMMA) {
								FINISH(field);
								ADD_FIELD(structure, field, "struct");
								int n_fields = arrlen(fields);
								for (int i = 0; i < n_fields; i++) {
									CONSUME(TOK_COMMA, "Expected another default value expression");
									AST_Field* f = fields[i];
									f->type = field->type;
									APPLY(f->default_value, expression, 0);
									FINISH(f);
									ADD_FIELD(structure, f, "struct");
								}
								arrfree(fields);
								switch (TOP().type) {
									case TOK_COMMA:
										SYNTAX_ERROR("Too many default values in struct field list");
										break;
									case TOK_EOL:
										POP();
										break;
									default:
										SYNTAX_ERROR("Expected end-of-line after struct field list");
								}
								break;
							}
						}
						FINISH(field);
						ADD_FIELD(structure, field, "struct");
						int n_fields = arrlen(fields);
						for (int i = 0; i < n_fields; i++) {
							AST_Field* f = fields[i];
							f->type = field->type;
							f->default_value = field->default_value;
							FINISH(f);
							ADD_FIELD(structure, f, "struct");
						}
						arrfree(fields);
						CONSUME(TOK_EOL, "Expected end-of-line after struct field list");
					} break;

					case TOK_COLON: {
						POP();
						APPLY(field->type, type, 0);
						if (TOP().type == TOK_ASSIGN) {
							POP();
							APPLY(field->default_value, expression, 0);
						}
						FINISH(field);
						ADD_FIELD(structure, field, "struct");
						CONSUME(TOK_EOL, "Expected end-of-line after struct field");
					} break;

					default:
						SYNTAX_ERROR("Expected ':' and type or comma-separated identifier list here");
				}
			} break;
			case DIR_CONSTRAINT: {
				POP();
				APPEND(structure->constraints, expression, 0);
				CONSUME(TOK_EOL, "Expected end-of-line after struct constraint");
			} break;
			case TOK_EOL: POP(); break;
			case TOK_RBRACE: goto exit_loop;
			default: SYNTAX_ERROR("Expected a field definition here");
		}
	}
	exit_loop:

	CONSUME(TOK_RBRACE, "Expected '}' to end struct");

	RETURN(structure);
}

static AST_Import* import(Parser self) {
	NEW_NODE(imp, NODE_IMPORT);
	POP();  // 'import'
	switch (TOP().type) {
		case KW_USING: {  // 'using' form
			imp->is_using = true;
			POP();
			switch (TOP().type) {
				case TOK_STRING:
					imp->imported_file = POP().str_value;
					EXPECT(TOK_EOL, "Expected end-of-line after 'using' pathname import");
					RETURN(imp);
				case TOK_IDENT: {
					NEW_NODE(local_name, NODE_NAME);
					imp->local_name = local_name;
					APPLY(imp->qualified_name, qualname);
					local_name->name = join_qualname(self, imp->qualified_name);
					FINISH(local_name);
					EXPECT(TOK_EOL, "Expected end-of-line after 'using' qualified name import");
					RETURN(imp);
				}
				default: SYNTAX_ERROR("Invalid target of 'using' import");
			}
		}
		case TOK_IDENT: break; // valid, but not sure which form this is
		case TOK_EOL: SYNTAX_ERROR("import statement is missing its target");
		default: SYNTAX_ERROR("Invalid target of import");
	}
	// First token is an identifier
	switch (LOOKAHEAD(1).type) {
		case TOK_EOL:
		case TOK_DOT: {  // qualified name form
			APPLY(imp->qualified_name, qualname);  // TODO: decide what should be imported
			NEW_NODE_FROM(local_name, NODE_NAME, imp->qualified_name);
			local_name->name = join_qualname(self, imp->qualified_name);
			FINISH(local_name);
			imp->local_name = local_name;
			EXPECT(TOK_EOL, "Expected end-of-line after qualified name import");
			RETURN(imp);
		}
		case TOK_ASSIGN: {  // localname form
			APPLY(imp->local_name, simple_name);
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

static AST_FuncDef* func_def(Parser self) {
	NEW_NODE(func, NODE_FUNC_DEF);
	POP();  // 'func'
	switch (TOP().type) {
		case TOK_IDENT:
			APPLY(func->name, simple_name);
			break;
		case TOK_LPAREN:
			break;  // no name
		default:
			SYNTAX_ERROR("Expected an identifier or operator to name this function");
	}
	CONSUME(TOK_LPAREN, "Expected a function parameter list");
	bool vararg_seen = false;
	while (TOP().type != TOK_RPAREN) {
		if (!vararg_seen && TOP().type == TOK_ELLIPSIS) {
			POP();
			vararg_seen = true;
			CONSUME(TOK_COMMA, "Expected comma after lone ellipsis in parameter list");
			EXPECT(TOK_IDENT, "Expected a keyword-only parameter after lone ellipsis");
		}
		if (TOP().type != TOK_IDENT) SYNTAX_ERROR("Expected the name of a parameter");
		NEW_NODE(param, NODE_PARAM);
		param->is_kw_only = vararg_seen;
		APPLY(param->name, simple_name);
		if (shgeti(func->params, param->name->name) >= 0) {
			SYNTAX_ERROR_FROM_NONFATAL(
				param->name, "There is already a parameter named '%s' in function '%s'",
				param->name->name, func->name? func->name->name : "<anonymous>"
			);
		}
		if (TOP().type == TOK_COLON) {
			POP();
			APPLY(param->type, type, 0);
			if (TOP().type == TOK_ELLIPSIS) {
				if (vararg_seen) SYNTAX_ERROR_NONFATAL("Parameter lists may only include one vararg");
				POP();
				param->is_vararg = true;
				vararg_seen = true;
			}
		}
		if (TOP().type == TOK_ASSIGN) {
			if (param->is_vararg) SYNTAX_ERROR_NONFATAL("Varargs cannot have a default value");
			POP();
			APPLY(param->default_value, expression, 0);
		}
		shput(func->params, param->name->name, param);
		if (TOP().type == TOK_COMMA) {
			POP();
		}
		else EXPECT(TOK_RPAREN, "Expected comma or end of parameter list");
	}
	POP();  // ')'

	if (TOP().type == TOK_COLON) {
		POP();
		APPLY(func->ret_type, type, 0);
	}

	EXPECT(TOK_LBRACE, "Expected function body");
	APPLY(func->body, block);

	RETURN(func);
}

static AST_Test* test_def(Parser self) {
	NEW_NODE(test, NODE_TEST);
	POP();  // 'test'
	if (TOP().type == TOK_STRING) {
		APPLY(test->description, string_literal);
	}
	EXPECT(TOK_LBRACE, "Expected test body");
	APPLY(test->body, block);

	RETURN(test);
}
