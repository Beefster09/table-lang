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
			APPLY(constant->expr_value, expr, 0);
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
		default: SYNTAX_ERROR("");
	}
}
