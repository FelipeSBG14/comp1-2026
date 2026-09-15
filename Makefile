CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic
FLEX ?= flex

LEXER = lexer
FLEX_SRC = microc.flex
GEN_SRC = lex.yy.c
TESTS = tests/test.mc \
	tests/palavras_reservadas.mc \
	tests/literais.mc \
	tests/operadores.mc \
	tests/erros.mc \
	tests/recuperacao.mc

.PHONY: all test clean

all: $(LEXER)

$(LEXER): $(GEN_SRC)
	$(CC) $(CFLAGS) $(GEN_SRC) -o $(LEXER)

$(GEN_SRC): $(FLEX_SRC)
	$(FLEX) $(FLEX_SRC)

test: $(LEXER)
	./$(LEXER) tests/test.mc
	./$(LEXER) tests/palavras_reservadas.mc
	./$(LEXER) tests/literais.mc
	./$(LEXER) tests/operadores.mc
	./$(LEXER) tests/erros.mc
	./$(LEXER) tests/recuperacao.mc

clean:
	rm -f $(LEXER) $(LEXER).exe $(GEN_SRC)
