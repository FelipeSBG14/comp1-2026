/*
 * Adaptacao do scanner do TP1 para uso sob demanda pelo parser gerado.
 * Gerar com: flex -o microc_scanner.c src/microc_scanner.flex
 * Compilar junto de parser_microc.c com -DSCANNER_EXTERNO.
 */
%option noyywrap noinput nounput prefix="microc_yy"

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_microc.h"

static FILE *arquivo_microc;
static char *copia_lexema;
static int linha_atual = 1;
static int coluna_atual = 1;
static int linha_token = 1;
static int coluna_token = 1;
static int tem_ultimo;
static int ultimo_tipo;
static const char *mensagem_erro;

extern FILE *microc_yyin;
extern char *microc_yytext;
extern int microc_yyleng;
int microc_yylex(void);
int microc_yylex_destroy(void);
void microc_yyrestart(FILE *arquivo);

static void atualiza_posicao(const char *texto, int tamanho)
{
    int i;
    linha_token = linha_atual;
    coluna_token = coluna_atual;
    for (i = 0; i < tamanho; i++) {
        if (texto[i] == '\n') {
            linha_atual++;
            coluna_atual = 1;
        } else {
            coluna_atual++;
        }
    }
}

#define YY_USER_ACTION atualiza_posicao(microc_yytext, microc_yyleng);

static int permite_menos_unario(void)
{
    if (!tem_ultimo) return 1;
    return ultimo_tipo == T_ASSIGN || ultimo_tipo == T_LPAREN ||
           ultimo_tipo == T_LBRACE || ultimo_tipo == T_LBRACKET ||
           ultimo_tipo == T_COMMA || ultimo_tipo == T_SEMICOLON ||
           ultimo_tipo == T_RETURN || ultimo_tipo == T_PLUS ||
           ultimo_tipo == T_MINUS || ultimo_tipo == T_MUL ||
           ultimo_tipo == T_DIV || ultimo_tipo == T_MOD ||
           ultimo_tipo == T_EQ || ultimo_tipo == T_NEQ ||
           ultimo_tipo == T_LT || ultimo_tipo == T_GT ||
           ultimo_tipo == T_LEQ || ultimo_tipo == T_GEQ ||
           ultimo_tipo == T_AND || ultimo_tipo == T_OR ||
           ultimo_tipo == T_NOT;
}
%}

DIGITO         [0-9]
LETRA          [a-zA-Z_]
ALFANUM        [a-zA-Z0-9_]
CHAR_VALIDO    \'([^\\\'\n]|\\[nt\\\'0])\'
STRING_VALIDA  \"([^\\\"\n]|\\[nt\\\"0])*\"

%x COMMENT

%%

<INITIAL><<EOF>>         { return T_END_OF_FILE; }
[ \t\r\n]+            { }
"//"[^\n]*             { }
"/*"                    { BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT><<EOF>>         {
                            mensagem_erro = "EOF em comentario";
                            linha_token = linha_atual;
                            coluna_token = coluna_atual;
                            BEGIN(INITIAL);
                            return T_UNDEF;
                         }
<COMMENT>(.|\n)          { }
"*/"                    {
                            mensagem_erro = "Comentario nao iniciado";
                            return T_UNDEF;
                         }

"main"                  { return T_MAIN; }
"if"                    { return T_IF; }
"else"                  { return T_ELSE; }
"for"                   { return T_FOR; }
"return"                { return T_RETURN; }
"int"                   { return T_INT; }
"char"                  { return T_CHAR; }
"print"                 { return T_PRINT; }

"=="                    { return T_EQ; }
"!="                    { return T_NEQ; }
"<="                    { return T_LEQ; }
">="                    { return T_GEQ; }
"&&"                    { return T_AND; }
"||"                    { return T_OR; }
"="                     { return T_ASSIGN; }
"!"                     { return T_NOT; }
"<"                     { return T_LT; }
">"                     { return T_GT; }
"+"                     { return T_PLUS; }
"*"                     { return T_MUL; }
"/"                     { return T_DIV; }
"%"                     { return T_MOD; }
";"                     { return T_SEMICOLON; }
","                     { return T_COMMA; }
"("                     { return T_LPAREN; }
")"                     { return T_RPAREN; }
"{"                     { return T_LBRACE; }
"}"                     { return T_RBRACE; }
"["                     { return T_LBRACKET; }
"]"                     { return T_RBRACKET; }

-{DIGITO}+              {
                            if (permite_menos_unario()) return T_INTEGERCONST;
                            coluna_atual -= microc_yyleng - 1;
                            yyless(1);
                            return T_MINUS;
                         }
"-"                     { return T_MINUS; }
{DIGITO}+               { return T_INTEGERCONST; }
{CHAR_VALIDO}           { return T_CHARCONST; }
{STRING_VALIDA}         { return T_STRINGCONST; }
\'\'                    {
                            mensagem_erro = "Constante de caractere vazia";
                            return T_UNDEF;
                         }
\'([^\'\n]|\\.)*\'     {
                            mensagem_erro = "Constante de caractere invalida";
                            return T_UNDEF;
                         }
\'([^\'\n]|\\.)*       {
                            mensagem_erro = "Constante de caractere nao terminada";
                            return T_UNDEF;
                         }
\"([^\"\n]|\\.)*\"   {
                            mensagem_erro = "Sequencia de escape invalida";
                            return T_UNDEF;
                         }
\"([^\"\n]|\\.)*      {
                            mensagem_erro = "String nao terminada";
                            return T_UNDEF;
                         }
{LETRA}{ALFANUM}*       { return T_ID; }
.                       {
                            mensagem_erro = "Caractere invalido";
                            return T_UNDEF;
                         }

%%

int carregar_fonte(const char *caminho)
{
    arquivo_microc = fopen(caminho, "rb");
    if (arquivo_microc == NULL) return 0;
    microc_yyin = arquivo_microc;
    microc_yyrestart(arquivo_microc);
    BEGIN(INITIAL);
    linha_atual = 1;
    coluna_atual = 1;
    linha_token = 1;
    coluna_token = 1;
    tem_ultimo = 0;
    mensagem_erro = NULL;
    return 1;
}

void liberar_fonte(void)
{
    microc_yylex_destroy();
    if (arquivo_microc != NULL) fclose(arquivo_microc);
    arquivo_microc = NULL;
    free(copia_lexema);
    copia_lexema = NULL;
}

Token proximo_token(void)
{
    Token token;
    int tipo;
    tipo = microc_yylex();
    if (tipo == T_END_OF_FILE) {
        linha_token = linha_atual;
        coluna_token = coluna_atual;
    }
    free(copia_lexema);
    copia_lexema = (char *)malloc((size_t)microc_yyleng + 1);
    if (copia_lexema == NULL) {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(2);
    }
    memcpy(copia_lexema, microc_yytext, (size_t)microc_yyleng);
    copia_lexema[microc_yyleng] = '\0';
    token.tipo = tipo;
    token.lexema = copia_lexema;
    token.tamanho = tipo == T_END_OF_FILE ? 0 : microc_yyleng;
    token.linha = linha_token;
    token.coluna = coluna_token;
    token.erro = tipo == T_UNDEF ? mensagem_erro : NULL;
    if (tipo != T_UNDEF && tipo != T_END_OF_FILE) {
        ultimo_tipo = tipo;
        tem_ultimo = 1;
    }
    return token;
}
