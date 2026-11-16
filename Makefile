CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c89 -pedantic
FLEX ?= flex

GERADOR = gerador
FONTES = src/main.c src/apoio.c src/ebnf_lexer.c src/ebnf_parser.c \
	src/verificacao.c src/conjuntos.c src/ll1.c src/regex.c src/codegen.c
OBJETOS = $(FONTES:.c=.o)

.PHONY: all test test-tp1 microc-parser microc-parser-tp1 clean

all: $(GERADOR)

$(GERADOR): $(OBJETOS)
	$(CC) $(CFLAGS) $(OBJETOS) -o $(GERADOR)

src/%.o: src/%.c src/gerador.h
	$(CC) $(CFLAGS) -c $< -o $@

src/codegen.o: src/runtime_tokens.inc

test: $(GERADOR)
	sh ./tests_tp2/executar_testes.sh

microc-parser: $(GERADOR)
	./$(GERADOR) -o parser_microc grammars/microc.ebnf
	$(CC) $(CFLAGS) parser_microc.c -o parser_microc

microc-parser-tp1: $(GERADOR)
	./$(GERADOR) -o parser_microc grammars/microc.ebnf
	$(FLEX) -o microc_scanner.c src/microc_scanner.flex
	$(CC) $(CFLAGS) -DSCANNER_EXTERNO parser_microc.c microc_scanner.c -o parser_microc

test-tp1: microc.flex
	$(FLEX) microc.flex
	$(CC) -Wall -Wextra -std=c89 -pedantic lex.yy.c -o lexer

clean:
	rm -f $(GERADOR) $(GERADOR).exe $(OBJETOS) parser.c parser.h parser parser.exe
	rm -f parser_*.c parser_*.h parser_* parser_*.exe microc_scanner.c
