
#include <stdio.h>

#include "lexer.h"
#include "parser.h"

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		Lexer lex = lexer_create(argv[1]);
		if (!lex) {
			perror("Unable to open file");
		}
		Token tok;
		do {
			tok = lexer_pop_token(lex);
			const char* tok_as_str = token_repr(&tok);
			printf("%s\n", tok_as_str);
		} while (tok.type > 0 && tok.type != TOK_EOF);
		lexer_destroy(lex);
	}
	return 0;
}
