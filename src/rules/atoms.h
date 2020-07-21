// To be included *only* from parser.c

static AST_Name* simple_name(Parser self) {
	NEW_NODE(n, NODE_NAME);
	n->name = POP().str_value;
	RETURN(n);
}

static AST_Qualname* qualname(Parser self) {
	NEW_NODE(qn, NODE_QUALNAME);
	EXPECT(TOK_IDENT, "Expected an identifier here");
	do {
		arrpush(qn->parts, POP().str_value);
		if (TOP().type != TOK_DOT) RETURN(qn);
		POP();  // '.'
	} while (1);
}

static const char* join_qualname(Parser self, AST_Qualname* qn) {
	int len = arrlen(qn->parts);
	int total_len = len;
	for (int i = 0; i < len; i++) {
		total_len += strlen(qn->parts[i]);
	}
	char* buf = arena_alloc(self, total_len);
	const char* result = buf;
	for (int i = 0; i < len; i++) {
		const char* other = qn->parts[i];
		while (*other) *buf++ = *other++;
		if (i + 1 < len) *buf++ = '.';
	}
	// Adding a null byte at the end is unnecessary becase arena_alloc returns zeroed memory
	return result;
}

static AST_Int* null_literal(Parser self) {
	NEW_NODE(leaf, NODE_NULL);
	EXPECT(TOK_NULL, "Expected null here");
	POP();
	RETURN(leaf);
}

static AST_Int* int_literal(Parser self) {
	NEW_NODE(leaf, NODE_INT);
	EXPECT(TOK_INT, "Expected an integer here");
	leaf->value = POP().int_value;
	RETURN(leaf);
}

static AST_Float* float_literal(Parser self) {
	NEW_NODE(leaf, NODE_FLOAT);
	EXPECT(TOK_FLOAT, "Expected a float here");
	leaf->value = POP().float_value;
	RETURN(leaf);
}

static AST_Bool* bool_literal(Parser self) {
	NEW_NODE(leaf, NODE_BOOL);
	EXPECT(TOK_BOOL, "Expected a boolean here");
	leaf->value = POP().bool_value;
	RETURN(leaf);
}

static AST_Char* char_literal(Parser self) {
	NEW_NODE(leaf, NODE_CHAR);
	EXPECT(TOK_CHAR, "Expected a character here");
	leaf->value = POP().char_value;
	RETURN(leaf);
}

static AST_String* string_literal(Parser self) {
	NEW_NODE(leaf, NODE_STRING);
	EXPECT(TOK_STRING, "Expected a string here");
	const char* first = POP().str_value;
	if (TOP().type == TOK_STRING) {
		const char** strings = 0;
		arrpush(strings, first);
		do {
			arrpush(strings, POP().str_value);
		} while (TOP().type == TOK_STRING);
		int len = arrlen(strings);
		int total_len = 0;
		for (int i = 0; i < len; i++) {
			total_len += strlen(strings[i]);
		}
		char* buf = arena_alloc(self, total_len + 1);
		leaf->value = buf;
		for (int i = 0; i < len; i++) {
			const char* other = strings[i];
			while (*other) *buf++ = *other++;
		}
		// Adding a null byte at the end is unnecessary becase arena_alloc returns zeroed memory
		arrfree(strings);
	}
	else leaf->value = first;
	RETURN(leaf);
}

static AST_Node* atom(Parser self) {
	switch (TOP().type) {
		case TOK_NULL: return null_literal(self);
		case TOK_INT: return int_literal(self);
		case TOK_FLOAT: return float_literal(self);
		case TOK_BOOL: return bool_literal(self);
		case TOK_STRING: return string_literal(self);
		case TOK_CHAR: return char_literal(self);
		case TOK_IDENT: return qualname(self);
		default: SYNTAX_ERROR("Expected atom (an integer, float, boolean, string, or qualified name), not %s", _token_);
	}
}
