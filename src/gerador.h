#ifndef GERADOR_H
#define GERADOR_H

#include <stdio.h>
#include <stddef.h>

typedef enum {
    NO_VAZIO,
    NO_SIMBOLO,
    NO_SEQUENCIA,
    NO_ALTERNATIVA,
    NO_OPCIONAL,
    NO_REPETICAO
} TipoNo;

typedef struct Conjunto {
    unsigned char *dados;
    int tamanho;
} Conjunto;

typedef struct No {
    TipoNo tipo;
    char *texto;
    int linha;
    int coluna;
    int classe_simbolo;
    int referencia;
    struct No **filhos;
    int quantidade;
    int capacidade;
    int anulavel;
    int produtivo;
    Conjunto first;
    Conjunto follow;
    int conjunto_esperado;
} No;

typedef struct EstadoNfa {
    int tipo;
    int saida1;
    int saida2;
    unsigned char classe[32];
} EstadoNfa;

typedef struct Nfa {
    EstadoNfa *estados;
    int quantidade;
    int capacidade;
    int inicial;
} Nfa;

typedef struct EstadoDfa {
    int transicoes[256];
    int final;
} EstadoDfa;

typedef struct Dfa {
    EstadoDfa *estados;
    int quantidade;
    int inicial;
} Dfa;

typedef struct Terminal {
    char *nome;
    char *padrao;
    char *literal;
    int linha;
    int coluna;
    int ignorado;
    int literal_fixo;
    int indice;
    Nfa nfa;
    Dfa dfa;
} Terminal;

typedef struct Regra {
    char *nome;
    No *expressao;
    int linha;
    int coluna;
    int alcancavel;
    int produtivo;
} Regra;

typedef struct Gramatica {
    char *arquivo;
    Terminal *terminais;
    int quantidade_terminais;
    int capacidade_terminais;
    Regra *regras;
    int quantidade_regras;
    int capacidade_regras;
    int quantidade_tokens;
    int erros;
    int avisos;
} Gramatica;

typedef enum {
    TK_EOF,
    TK_ID,
    TK_REGEX,
    TK_LITERAL,
    TK_TOKENS,
    TK_RULES,
    TK_IGUAL,
    TK_PONTO_VIRGULA,
    TK_BARRA,
    TK_ABRE_PAR,
    TK_FECHA_PAR,
    TK_ABRE_COL,
    TK_FECHA_COL,
    TK_ABRE_CHAVE,
    TK_FECHA_CHAVE,
    TK_INVALIDO
} TipoTokenEbnf;

typedef struct TokenEbnf {
    TipoTokenEbnf tipo;
    char *texto;
    int linha;
    int coluna;
} TokenEbnf;

typedef struct LexerEbnf {
    const char *entrada;
    size_t tamanho;
    size_t posicao;
    int linha;
    int coluna;
    const char *arquivo;
    int erros;
} LexerEbnf;

void *xmalloc(size_t tamanho);
void *xrealloc(void *ponteiro, size_t tamanho);
char *xstrdup(const char *texto);
char *xstrndup(const char *texto, size_t tamanho);
char *ler_arquivo(const char *caminho, size_t *tamanho);
void diagnostico_erro(Gramatica *g, int linha, int coluna,
                      const char *formato, ...);
void diagnostico_aviso(Gramatica *g, int linha, int coluna,
                       const char *formato, ...);

void conjunto_inicializar(Conjunto *c, int tamanho);
void conjunto_liberar(Conjunto *c);
void conjunto_limpar(Conjunto *c);
int conjunto_adicionar(Conjunto *c, int elemento);
int conjunto_unir(Conjunto *destino, const Conjunto *origem);
int conjunto_intersecta(const Conjunto *a, const Conjunto *b);
int conjunto_contem(const Conjunto *c, int elemento);
int conjunto_igual(const Conjunto *a, const Conjunto *b);

void lexer_ebnf_inicializar(LexerEbnf *lexer, const char *entrada,
                            size_t tamanho, const char *arquivo);
TokenEbnf lexer_ebnf_proximo(LexerEbnf *lexer);
void token_ebnf_liberar(TokenEbnf *token);

void gramatica_inicializar(Gramatica *g, const char *arquivo);
void gramatica_liberar(Gramatica *g);
int gramatica_ler(Gramatica *g, const char *entrada, size_t tamanho);
void gramatica_imprimir(const Gramatica *g, FILE *saida);
int gramatica_buscar_regra(const Gramatica *g, const char *nome);
int gramatica_buscar_terminal(const Gramatica *g, const char *nome);
const char *gramatica_nome_token(const Gramatica *g, int indice);

int verificar_gramatica(Gramatica *g);
int calcular_conjuntos(Gramatica *g);
void imprimir_conjuntos(const Gramatica *g, FILE *saida);
int verificar_ll1(Gramatica *g);
int gerar_codigo(Gramatica *g, const char *prefixo);

int regex_compilar(const char *padrao, Nfa *nfa, char *erro,
                   size_t tamanho_erro);
void nfa_liberar(Nfa *nfa);
int dfa_construir_minimo(const Nfa *nfa, Dfa *dfa);
void dfa_liberar(Dfa *dfa);

#endif
