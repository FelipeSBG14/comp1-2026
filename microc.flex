/*
 * microc.flex
 *
 * Analisador lexico para a linguagem Micro C.
 * Disciplina: Compiladores I - FACOM
 */
%{
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    UNDEF,
    ID,
    END_OF_FILE,

    INTEGERCONST,
    CHARCONST,
    STRINGCONST,

    PLUS, MINUS, MUL, DIV, MOD,

    EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,

    ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN,
    LBRACE, RBRACE, LBRACKET, RBRACKET,

    MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
} TokenType;

static const char *nome_token[] = {
    "UNDEF", "ID", "END_OF_FILE",
    "INTEGERCONST", "CHARCONST", "STRINGCONST",
    "PLUS", "MINUS", "MUL", "DIV", "MOD",
    "EQ", "NEQ", "LT", "GT", "LEQ", "GEQ", "AND", "OR", "NOT",
    "ASSIGN", "SEMICOLON", "COMMA", "LPAREN", "RPAREN",
    "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
    "MAIN", "IF", "ELSE", "FOR", "RETURN", "INT", "CHAR", "PRINT"
};

typedef struct {
    char *symbol;
    size_t symbol_len;
    char *error_msg;
} YYSTYPE;

YYSTYPE microc_yylval = { NULL, 0, NULL };

extern char *yytext;
extern int yyleng;

int linha_atual = 1;
int linha_token = 1;

#define LITERAL_MAX 4096

static char literal_buffer[LITERAL_MAX];
static size_t literal_len = 0;
static int linha_literal = 1;
static int char_quantidade = 0;
static int tem_ultimo_token = 0;
static TokenType ultimo_token = END_OF_FILE;

static char *duplica_n(const char *texto, size_t tamanho) {
    char *copia = (char *)malloc(tamanho + 1);
    if (copia == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(1);
    }
    memcpy(copia, texto, tamanho);
    copia[tamanho] = '\0';
    return copia;
}

static char *duplica_texto(const char *texto) {
    return duplica_n(texto, strlen(texto));
}

static void limpa_yylval(void) {
    free(microc_yylval.symbol);
    free(microc_yylval.error_msg);
    microc_yylval.symbol = NULL;
    microc_yylval.symbol_len = 0;
    microc_yylval.error_msg = NULL;
}

static void guarda_texto(const char *texto, size_t tamanho) {
    free(microc_yylval.symbol);
    microc_yylval.symbol = duplica_n(texto, tamanho);
    microc_yylval.symbol_len = tamanho;
}

static void guarda_lexema(void) {
    guarda_texto(yytext, yyleng);
}

static void guarda_erro(const char *mensagem) {
    free(microc_yylval.error_msg);
    microc_yylval.error_msg = duplica_texto(mensagem);
}

static void reinicia_literal(void) {
    literal_len = 0;
    char_quantidade = 0;
}

static void adiciona_literal(char caractere) {
    if (literal_len + 1 >= LITERAL_MAX) {
        guarda_erro("Literal excede o tamanho maximo");
        return;
    }
    literal_buffer[literal_len++] = caractere;
}

static void adiciona_texto_literal(const char *texto, size_t tamanho) {
    size_t i;
    for (i = 0; i < tamanho; i++) {
        adiciona_literal(texto[i]);
    }
}

static void adiciona_escape_literal(char escape) {
    switch (escape) {
    case 'n':
        adiciona_literal('\n');
        break;
    case 't':
        adiciona_literal('\t');
        break;
    case '\\':
        adiciona_literal('\\');
        break;
    case '"':
        adiciona_literal('"');
        break;
    case '\'':
        adiciona_literal('\'');
        break;
    case '0':
        adiciona_literal('\0');
        break;
    default:
        guarda_erro("Sequencia de escape invalida");
        break;
    }
}

static void guarda_literal(void) {
    guarda_texto(literal_buffer, literal_len);
}

static int token_pode_anteceder_menos_unario(TokenType token) {
    switch (token) {
    case ASSIGN:
    case LPAREN:
    case LBRACE:
    case LBRACKET:
    case COMMA:
    case SEMICOLON:
    case RETURN:
    case PLUS:
    case MINUS:
    case MUL:
    case DIV:
    case MOD:
    case EQ:
    case NEQ:
    case LT:
    case GT:
    case LEQ:
    case GEQ:
    case AND:
    case OR:
    case NOT:
        return 1;
    default:
        return 0;
    }
}

static int menos_deve_ser_inteiro_negativo(void) {
    if (!tem_ultimo_token) {
        return 1;
    }
    return token_pode_anteceder_menos_unario(ultimo_token);
}

static int registra_token(TokenType token, int linha) {
    linha_token = linha;
    if (token != END_OF_FILE && token != UNDEF) {
        ultimo_token = token;
        tem_ultimo_token = 1;
    }
    return token;
}

static int registra_erro(int linha) {
    linha_token = linha;
    return UNDEF;
}

static int token_reservado_ou_id(void) {
    if (strcmp(yytext, "main") == 0) {
        return registra_token(MAIN, linha_atual);
    }
    if (strcmp(yytext, "if") == 0) {
        return registra_token(IF, linha_atual);
    }
    if (strcmp(yytext, "else") == 0) {
        return registra_token(ELSE, linha_atual);
    }
    if (strcmp(yytext, "for") == 0) {
        return registra_token(FOR, linha_atual);
    }
    if (strcmp(yytext, "return") == 0) {
        return registra_token(RETURN, linha_atual);
    }
    if (strcmp(yytext, "int") == 0) {
        return registra_token(INT, linha_atual);
    }
    if (strcmp(yytext, "char") == 0) {
        return registra_token(CHAR, linha_atual);
    }
    if (strcmp(yytext, "print") == 0) {
        return registra_token(PRINT, linha_atual);
    }
    guarda_lexema();
    return registra_token(ID, linha_atual);
}

static const char *lexema_do_token(int token, size_t *tamanho) {
    if ((token == ID || token == INTEGERCONST ||
         token == CHARCONST || token == STRINGCONST) &&
        microc_yylval.symbol != NULL) {
        *tamanho = microc_yylval.symbol_len;
        return microc_yylval.symbol;
    }
    *tamanho = yyleng;
    return yytext;
}

static void imprime_lexema(const char *texto, size_t tamanho) {
    size_t i;
    for (i = 0; i < tamanho; i++) {
        unsigned char c = (unsigned char)texto[i];
        switch (c) {
        case '\n':
            fputs("\\n", stdout);
            break;
        case '\t':
            fputs("\\t", stdout);
            break;
        case '\0':
            fputs("\\0", stdout);
            break;
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '\'':
            fputs("\\'", stdout);
            break;
        default:
            if (isprint(c)) {
                putchar(c);
            } else {
                printf("\\x%02X", c);
            }
            break;
        }
    }
}
%}

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%x COMMENT
%x STRING
%x CHAR_LITERAL
%x STRING_ERROR
%x CHAR_ERROR

%option noinput nounput

%%

<INITIAL><<EOF>>    { return registra_token(END_OF_FILE, linha_atual); }

\n                  { linha_atual++; }
[ \t\r]+            { }

"//".*              { }
"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { linha_atual++; }
<COMMENT><<EOF>>    {
                        guarda_erro("EOF em comentario");
                        BEGIN(INITIAL);
                        return registra_erro(linha_atual);
                    }
<COMMENT>.          { }
"*/"                {
                        guarda_erro("Comentario nao iniciado");
                        return registra_erro(linha_atual);
                    }

{LETRA}{ALFANUM}*   { return token_reservado_ou_id(); }

-{DIGIT}+           {
                        if (menos_deve_ser_inteiro_negativo()) {
                            guarda_lexema();
                            return registra_token(INTEGERCONST, linha_atual);
                        }
                        yyless(1);
                        return registra_token(MINUS, linha_atual);
                    }
{DIGIT}+            {
                        guarda_lexema();
                        return registra_token(INTEGERCONST, linha_atual);
                    }

"'"                 {
                        linha_literal = linha_atual;
                        reinicia_literal();
                        BEGIN(CHAR_LITERAL);
                    }
<CHAR_LITERAL>"'"   {
                        if (char_quantidade == 1) {
                            guarda_literal();
                            BEGIN(INITIAL);
                            return registra_token(CHARCONST, linha_literal);
                        }
                        guarda_erro(char_quantidade == 0
                                    ? "Constante de caractere vazia"
                                    : "Constante de caractere invalida");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<CHAR_LITERAL>\\[nt\\\"\'0] {
                        adiciona_escape_literal(yytext[1]);
                        char_quantidade++;
                    }
<CHAR_LITERAL>\\.   {
                        guarda_erro("Sequencia de escape invalida");
                        BEGIN(CHAR_ERROR);
                        return registra_erro(linha_literal);
                    }
<CHAR_LITERAL>\n    {
                        linha_atual++;
                        guarda_erro("Constante de caractere nao terminada");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<CHAR_LITERAL>\0    {
                        guarda_erro("Constante de caractere contem caractere nulo");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<CHAR_LITERAL><<EOF>> {
                        guarda_erro("EOF em caractere");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<CHAR_LITERAL>[^\\'\n\0] {
                        adiciona_literal(yytext[0]);
                        char_quantidade++;
                    }
<CHAR_ERROR>"'"     { BEGIN(INITIAL); }
<CHAR_ERROR>\n      {
                        linha_atual++;
                        BEGIN(INITIAL);
                    }
<CHAR_ERROR><<EOF>> { return registra_token(END_OF_FILE, linha_atual); }
<CHAR_ERROR>.       { }

\"                  {
                        linha_literal = linha_atual;
                        reinicia_literal();
                        BEGIN(STRING);
                    }
<STRING>\"          {
                        guarda_literal();
                        BEGIN(INITIAL);
                        return registra_token(STRINGCONST, linha_literal);
                    }
<STRING>\\[nt\\\"0] {
                        adiciona_escape_literal(yytext[1]);
                    }
<STRING>\\.         {
                        guarda_erro("Sequencia de escape invalida");
                        BEGIN(STRING_ERROR);
                        return registra_erro(linha_literal);
                    }
<STRING>\n          {
                        linha_atual++;
                        guarda_erro("String nao terminada");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<STRING>\0          {
                        guarda_erro("String contem caractere nulo");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<STRING><<EOF>>     {
                        guarda_erro("EOF em string");
                        BEGIN(INITIAL);
                        return registra_erro(linha_literal);
                    }
<STRING>[^\\\"\n\0]+ {
                        adiciona_texto_literal(yytext, yyleng);
                    }
<STRING_ERROR>\"    { BEGIN(INITIAL); }
<STRING_ERROR>\n    {
                        linha_atual++;
                        BEGIN(INITIAL);
                    }
<STRING_ERROR><<EOF>> { return registra_token(END_OF_FILE, linha_atual); }
<STRING_ERROR>.     { }

"=="                { return registra_token(EQ, linha_atual); }
"!="                { return registra_token(NEQ, linha_atual); }
"<="                { return registra_token(LEQ, linha_atual); }
">="                { return registra_token(GEQ, linha_atual); }
"&&"                { return registra_token(AND, linha_atual); }
"||"                { return registra_token(OR, linha_atual); }
"="                 { return registra_token(ASSIGN, linha_atual); }
"!"                 { return registra_token(NOT, linha_atual); }
"<"                 { return registra_token(LT, linha_atual); }
">"                 { return registra_token(GT, linha_atual); }

"+"                 { return registra_token(PLUS, linha_atual); }
"-"                 { return registra_token(MINUS, linha_atual); }
"*"                 { return registra_token(MUL, linha_atual); }
"/"                 { return registra_token(DIV, linha_atual); }
"%"                 { return registra_token(MOD, linha_atual); }
";"                 { return registra_token(SEMICOLON, linha_atual); }
","                 { return registra_token(COMMA, linha_atual); }
"("                 { return registra_token(LPAREN, linha_atual); }
")"                 { return registra_token(RPAREN, linha_atual); }
"{"                 { return registra_token(LBRACE, linha_atual); }
"}"                 { return registra_token(RBRACE, linha_atual); }
"["                 { return registra_token(LBRACKET, linha_atual); }
"]"                 { return registra_token(RBRACKET, linha_atual); }

.                   {
                        microc_yylval.error_msg = duplica_texto(yytext);
                        return registra_erro(linha_atual);
                    }

%%

int yywrap(void) {
    return 1;
}

int main(int argc, char **argv) {
    FILE *arquivo_fonte;
    int tipo;

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo.mc>\n", argv[0]);
        return 1;
    }

    arquivo_fonte = fopen(argv[1], "rb");
    if (arquivo_fonte == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", argv[1]);
        return 1;
    }

    yyin = arquivo_fonte;
    while ((tipo = yylex()) != END_OF_FILE) {
        if (tipo == UNDEF) {
            fprintf(stderr, "ERRO LEXICO (linha %d): %s\n",
                    linha_token,
                    microc_yylval.error_msg != NULL ? microc_yylval.error_msg : yytext);
            limpa_yylval();
            continue;
        }

        size_t tamanho_lexema = 0;
        const char *lexema = lexema_do_token(tipo, &tamanho_lexema);
        printf("Token: tipo = %-13s lexema = '", nome_token[tipo]);
        imprime_lexema(lexema, tamanho_lexema);
        printf("'  linha = %d\n", linha_token);
        limpa_yylval();
    }

    limpa_yylval();
    fclose(arquivo_fonte);
    return 0;
}
