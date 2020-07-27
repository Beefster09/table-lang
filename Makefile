
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wformat=2 -Wfloat-equal -I generated -g
LINK = gcc
SOURCES = $(wildcard src/*.c)
OBJECTS = $(subst src/,build/,$(SOURCES:.c=.o))

.PHONY : ALL clean

ALL: compiler

build:
	mkdir -p build
generated:
	mkdir -p generated

build/%.d: src/%.c | build
	$(CC) -MM -MG -MT 'build/$*.o' $< | sed -r 's|([A-Za-z0-9_\.]+\.gen\.h)|generated/\1|g' > $@
	$(CC) -MM -MG -MT 'build/$*.d' $< | sed -r 's|([A-Za-z0-9_\.]+\.gen)\.h|src/\1|g' >> $@

include $(subst src/,build/,$(SOURCES:.c=.d))

build/%.o: src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

generated/%.gen.h: src/%.gen | generated
	$< > $@

$(wildcard generated/keywords*.gen.h): src/keywords.txt src/directives.list
$(wildcard generated/ast_*.gen.h): src/ast_nodes.h
generated/rule_prototypes.gen.h: $(wildcard src/rules/*.h)

$(OBJECTS): | build

compiler: $(OBJECTS) | build
	$(LINK) $^ -o $@

clean:
	rm -rf build generated
