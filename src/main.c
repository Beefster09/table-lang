
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "colors.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define color_is_supported() 0
#elif __linux__
	#define color_is_supported() (strncmp(getenv("TERM"), "xterm", 5) == 0)
#else
	#define color_is_supported() 0
#endif

int main(int argc, char *argv[]) {
	int status = 0;
	setlocale(LC_ALL, "en_US.utf8");
	if (color_is_supported()) color_enable();
	if (argc >= 2) {
		Parser parser = parser_create(argv[1]);
		if (!parser) {
			perror("Unable to open file");
		}
		AST_Node* root = parser_execute(parser);
		if (root) {
			color_fprintf(stderr, TERM_FG_GREEN, "Parsing success!\n");
			print_ast(stderr, root);
		}
		else {
			color_fprintf(stderr, TERM_FG_RED, "Parsing failed.\n");
			status = 1;
		}
		parser_destroy(parser);
	}
	return status;
}
