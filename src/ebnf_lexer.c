#include "gerador.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int atual(const LexerEbnf *lexer)
{
    if (lexer->posicao >= lexer->tamanho) {
        return EOF;
    }
    return (unsigned char)lexer->entrada[lexer->posicao];
}

static int seguinte(const LexerEbnf *lexer)
{
    if (lexer->posicao + 1 >= lexer->tamanho) {
        return EOF;
    }
    return (unsigned char)lexer->entrada[lexer->posicao + 1];
}

static void avancar(LexerEbnf *lexer)
{
    int c;
    c = atual(lexer);
    if (c == EOF) {
        return;
    }
    lexer->posicao++;
    if (c == '\n') {
        lexer->linha++;
        lexer->coluna = 1;
    } else {
        lexer->coluna++;
    }
}

void lexer_ebnf_inicializar(LexerEbnf *lexer, const char *entrada,
                            size_t tamanho, const char *arquivo)
{
    lexer->entrada = entrada;
    lexer->tamanho = tamanho;
    lexer->posicao = 0;
    lexer->linha = 1;
    lexer->coluna = 1;
    lexer->arquivo = arquivo;
    lexer->erros = 0;
}

static void erro_lexer(LexerEbnf *lexer, int linha, int coluna,
                       const char *mensagem)
{
    fprintf(stderr, "%s:%d:%d: erro: %s\n", lexer->arquivo, linha,
            coluna, mensagem);
    lexer->erros++;
}

static void ignorar_espacos_comentarios(LexerEbnf *lexer)
{
    int linha;
    int coluna;
    for (;;) {
        while (isspace(atual(lexer))) {
            avancar(lexer);
        }
        if (atual(lexer) != '(' || seguinte(lexer) != '*') {
            break;
        }
        linha = lexer->linha;
        coluna = lexer->coluna;
        avancar(lexer);
        avancar(lexer);
        while (atual(lexer) != EOF &&
               !(atual(lexer) == '*' && seguinte(lexer) == ')')) {
            avancar(lexer);
        }
        if (atual(lexer) == EOF) {
            erro_lexer(lexer, linha, coluna, "comentario nao terminado");
            return;
        }
        avancar(lexer);
        avancar(lexer);
    }
}

static char traduzir_escape(int c)
{
    if (c == 'n') return '\n';
    if (c == 'r') return '\r';
    if (c == 't') return '\t';
    if (c == '0') return '\0';
    return (char)c;
}

static TokenEbnf criar_token(TipoTokenEbnf tipo, char *texto,
                             int linha, int coluna)
{
    TokenEbnf token;
    token.tipo = tipo;
    token.texto = texto;
    token.linha = linha;
    token.coluna = coluna;
    return token;
}

static TokenEbnf ler_literal(LexerEbnf *lexer)
{
    int delimitador;
    int linha;
    int coluna;
    size_t capacidade;
    size_t tamanho;
    char *texto;
    int c;
    delimitador = atual(lexer);
    linha = lexer->linha;
    coluna = lexer->coluna;
    capacidade = 32;
    tamanho = 0;
    texto = (char *)xmalloc(capacidade);
    avancar(lexer);
    while (atual(lexer) != EOF && atual(lexer) != delimitador &&
           atual(lexer) != '\n') {
        c = atual(lexer);
        avancar(lexer);
        if (c == '\\') {
            if (atual(lexer) == EOF || atual(lexer) == '\n') {
                break;
            }
            c = traduzir_escape(atual(lexer));
            avancar(lexer);
        }
        if (tamanho + 1 >= capacidade) {
            capacidade *= 2;
            texto = (char *)xrealloc(texto, capacidade);
        }
        texto[tamanho++] = (char)c;
    }
    if (atual(lexer) != delimitador) {
        erro_lexer(lexer, linha, coluna, "literal nao terminado");
        free(texto);
        return criar_token(TK_INVALIDO, xstrdup(""), linha, coluna);
    }
    avancar(lexer);
    texto[tamanho] = '\0';
    return criar_token(TK_LITERAL, texto, linha, coluna);
}

static TokenEbnf ler_regex(LexerEbnf *lexer)
{
    int linha;
    int coluna;
    size_t inicio;
    size_t fim;
    linha = lexer->linha;
    coluna = lexer->coluna;
    avancar(lexer);
    inicio = lexer->posicao;
    while (atual(lexer) != EOF && atual(lexer) != '\n') {
        if (atual(lexer) == '\\') {
            avancar(lexer);
            if (atual(lexer) != EOF) {
                avancar(lexer);
            }
        } else if (atual(lexer) == '/') {
            fim = lexer->posicao;
            avancar(lexer);
            return criar_token(TK_REGEX,
                xstrndup(lexer->entrada + inicio, fim - inicio),
                linha, coluna);
        } else {
            avancar(lexer);
        }
    }
    erro_lexer(lexer, linha, coluna, "expressao regular nao terminada");
    return criar_token(TK_INVALIDO, xstrdup(""), linha, coluna);
}

TokenEbnf lexer_ebnf_proximo(LexerEbnf *lexer)
{
    int linha;
    int coluna;
    int c;
    size_t inicio;
    char *texto;
    ignorar_espacos_comentarios(lexer);
    linha = lexer->linha;
    coluna = lexer->coluna;
    c = atual(lexer);
    if (c == EOF) return criar_token(TK_EOF, NULL, linha, coluna);
    if (isalpha(c) || c == '_') {
        inicio = lexer->posicao;
        while (isalnum(atual(lexer)) || atual(lexer) == '_') {
            avancar(lexer);
        }
        texto = xstrndup(lexer->entrada + inicio, lexer->posicao - inicio);
        if (strcmp(texto, "TOKENS") == 0) {
            free(texto);
            return criar_token(TK_TOKENS, NULL, linha, coluna);
        }
        if (strcmp(texto, "RULES") == 0) {
            free(texto);
            return criar_token(TK_RULES, NULL, linha, coluna);
        }
        return criar_token(TK_ID, texto, linha, coluna);
    }
    if (c == '\'' || c == '"') return ler_literal(lexer);
    if (c == '/') return ler_regex(lexer);
    avancar(lexer);
    if (c == '=') return criar_token(TK_IGUAL, NULL, linha, coluna);
    if (c == ';') return criar_token(TK_PONTO_VIRGULA, NULL, linha, coluna);
    if (c == '|') return criar_token(TK_BARRA, NULL, linha, coluna);
    if (c == '(') return criar_token(TK_ABRE_PAR, NULL, linha, coluna);
    if (c == ')') return criar_token(TK_FECHA_PAR, NULL, linha, coluna);
    if (c == '[') return criar_token(TK_ABRE_COL, NULL, linha, coluna);
    if (c == ']') return criar_token(TK_FECHA_COL, NULL, linha, coluna);
    if (c == '{') return criar_token(TK_ABRE_CHAVE, NULL, linha, coluna);
    if (c == '}') return criar_token(TK_FECHA_CHAVE, NULL, linha, coluna);
    erro_lexer(lexer, linha, coluna, "caractere invalido na gramatica");
    texto = (char *)xmalloc(2);
    texto[0] = (char)c;
    texto[1] = '\0';
    return criar_token(TK_INVALIDO, texto, linha, coluna);
}

void token_ebnf_liberar(TokenEbnf *token)
{
    free(token->texto);
    token->texto = NULL;
}
