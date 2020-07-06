
#include <stdio.h>
#include <locale.h>

#include "lexer.h"
#include "parser.h"

int main(int argc, char *argv[]) {
	setlocale(LC_ALL, "en_US.utf8");
	if (argc >= 2) {
		Parser parser = parser_create(argv[1]);
		if (!parser) {
			perror("Unable to open file");
		}
		parser_execute(parser);
		parser_destroy(parser);
	}
	return 0;
}
