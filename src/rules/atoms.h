// To be included *only* from parser.c

static AST_Qualname* qualname(Parser self, bool allow_eol) {
    NEW_NODE(qn, NODE_QUALNAME);
    EXPECT(TOK_IDENT, "Unexpected token in qualified name: %s", _token_);
    do {
        sb_push(qn->parts, POP().str_value);
        if (allow_eol) { while(TOP().type == TOK_EOL) POP(); }
        if (TOP().type != TOK_DOT) RETURN(qn);
        POP();
        if (allow_eol) { while(TOP().type == TOK_EOL) POP(); }
    } while (1);
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

static AST_String* string_literal(Parser self, bool allow_eol) {
    NEW_NODE(leaf, NODE_STRING);
    EXPECT(TOK_STRING, "Expected a string here");
    const char* first = POP().str_value;
    if (TOP().type == TOK_STRING) {
        const char** strings = 0;
        sb_push(strings, first);
        do {
            sb_push(strings, POP().str_value);
            if (allow_eol) { while(TOP().type == TOK_EOL) POP(); }
        } while (TOP().type == TOK_STRING);
        int len = sb_count(strings);
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
        sb_free(strings);
    }
    else leaf->value = first;
    RETURN(leaf);
}

static AST_Node* atom(Parser self, bool allow_eol) {
    switch (TOP().type) {
        case TOK_INT: return int_literal(self);
        case TOK_FLOAT: return float_literal(self);
        case TOK_BOOL: return bool_literal(self);
        case TOK_STRING: return string_literal(self, allow_eol);
        case TOK_CHAR: return char_literal(self);
        case TOK_IDENT: return qualname(self, allow_eol);
        default: SYNTAX_ERROR("Expected atom (an integer, float, boolean, string, or qualified name), not %s", _token_);
    }
}
