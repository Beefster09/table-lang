
#include <stdio.h>
#include <locale.h>

#include "lexer.h"
#include "parser.h"

#define DKGRAY "\x1b[90m"
#define CLEAR_COLOR "\x1b[0m"

int main(int argc, char *argv[]) {
	setlocale(LC_ALL, "en_US.utf8");
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
		int n_lines;
		char** lines = lexer_get_lines(lex, &n_lines);
		for (int i = 0; i < n_lines; i++) {
			printf(DKGRAY "% 3d" CLEAR_COLOR " %s\n", i+1, lines[i]);
		}
		lexer_destroy(lex);
	}
	return 0;
}
