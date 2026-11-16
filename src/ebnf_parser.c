#include "gerador.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ParserEbnf {
    Gramatica *g;
    LexerEbnf lexer;
    TokenEbnf atual;
} ParserEbnf;

static No *novo_no(TipoNo tipo, int linha, int coluna)
{
    No *no;
    no = (No *)xmalloc(sizeof(No));
    memset(no, 0, sizeof(No));
    no->tipo = tipo;
    no->linha = linha;
    no->coluna = coluna;
    no->referencia = -1;
    no->conjunto_esperado = -1;
    return no;
}

static void no_adicionar(No *pai, No *filho)
{
    if (pai->quantidade == pai->capacidade) {
        pai->capacidade = pai->capacidade == 0 ? 4 : pai->capacidade * 2;
        pai->filhos = (No **)xrealloc(pai->filhos,
            (size_t)pai->capacidade * sizeof(No *));
    }
    pai->filhos[pai->quantidade++] = filho;
}

static void no_liberar(No *no)
{
    int i;
    if (no == NULL) return;
    for (i = 0; i < no->quantidade; i++) {
        no_liberar(no->filhos[i]);
    }
    free(no->filhos);
    free(no->texto);
    conjunto_liberar(&no->first);
    conjunto_liberar(&no->follow);
    free(no);
}

void gramatica_inicializar(Gramatica *g, const char *arquivo)
{
    memset(g, 0, sizeof(Gramatica));
    g->arquivo = xstrdup(arquivo);
}

void gramatica_liberar(Gramatica *g)
{
    int i;
    for (i = 0; i < g->quantidade_terminais; i++) {
        free(g->terminais[i].nome);
        free(g->terminais[i].padrao);
        free(g->terminais[i].literal);
        nfa_liberar(&g->terminais[i].nfa);
        dfa_liberar(&g->terminais[i].dfa);
    }
    for (i = 0; i < g->quantidade_regras; i++) {
        free(g->regras[i].nome);
        no_liberar(g->regras[i].expressao);
    }
    free(g->terminais);
    free(g->regras);
    free(g->arquivo);
    memset(g, 0, sizeof(Gramatica));
}

int gramatica_buscar_regra(const Gramatica *g, const char *nome)
{
    int i;
    for (i = 0; i < g->quantidade_regras; i++) {
        if (strcmp(g->regras[i].nome, nome) == 0) return i;
    }
    return -1;
}

int gramatica_buscar_terminal(const Gramatica *g, const char *nome)
{
    int i;
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (strcmp(g->terminais[i].nome, nome) == 0) return i;
    }
    return -1;
}

const char *gramatica_nome_token(const Gramatica *g, int indice)
{
    int i;
    if (indice == g->quantidade_tokens) return "$";
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (g->terminais[i].indice == indice) {
            if (g->terminais[i].literal_fixo) return g->terminais[i].literal;
            return g->terminais[i].nome;
        }
    }
    return "?";
}

static void avancar_parser(ParserEbnf *p)
{
    token_ebnf_liberar(&p->atual);
    p->atual = lexer_ebnf_proximo(&p->lexer);
}

static int aceitar(ParserEbnf *p, TipoTokenEbnf tipo)
{
    if (p->atual.tipo != tipo) return 0;
    avancar_parser(p);
    return 1;
}

static const char *nome_tipo(TipoTokenEbnf tipo)
{
    if (tipo == TK_ID) return "identificador";
    if (tipo == TK_REGEX) return "expressao regular";
    if (tipo == TK_LITERAL) return "literal";
    if (tipo == TK_TOKENS) return "TOKENS";
    if (tipo == TK_RULES) return "RULES";
    if (tipo == TK_IGUAL) return "=";
    if (tipo == TK_PONTO_VIRGULA) return ";";
    if (tipo == TK_FECHA_PAR) return ")";
    if (tipo == TK_FECHA_COL) return "]";
    if (tipo == TK_FECHA_CHAVE) return "}";
    return "token";
}

static int esperar(ParserEbnf *p, TipoTokenEbnf tipo)
{
    if (aceitar(p, tipo)) return 1;
    diagnostico_erro(p->g, p->atual.linha, p->atual.coluna,
                     "esperado %s", nome_tipo(tipo));
    return 0;
}

static Terminal *adicionar_terminal(Gramatica *g)
{
    Terminal *t;
    if (g->quantidade_terminais == g->capacidade_terminais) {
        g->capacidade_terminais = g->capacidade_terminais == 0
            ? 8 : g->capacidade_terminais * 2;
        g->terminais = (Terminal *)xrealloc(g->terminais,
            (size_t)g->capacidade_terminais * sizeof(Terminal));
    }
    t = &g->terminais[g->quantidade_terminais++];
    memset(t, 0, sizeof(Terminal));
    t->indice = -1;
    return t;
}

static int buscar_literal(const Gramatica *g, const char *literal)
{
    int i;
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (g->terminais[i].literal_fixo &&
            strcmp(g->terminais[i].literal, literal) == 0) return i;
    }
    return -1;
}

static int adicionar_literal(Gramatica *g, const char *literal,
                             int linha, int coluna)
{
    int existente;
    Terminal *t;
    char nome[64];
    existente = buscar_literal(g, literal);
    if (existente >= 0) return existente;
    t = adicionar_terminal(g);
    sprintf(nome, "LIT_%d", g->quantidade_terminais);
    t->nome = xstrdup(nome);
    t->literal = xstrdup(literal);
    t->literal_fixo = 1;
    t->linha = linha;
    t->coluna = coluna;
    return g->quantidade_terminais - 1;
}

static void adicionar_regra(Gramatica *g, char *nome, No *expressao,
                            int linha, int coluna)
{
    Regra *r;
    if (gramatica_buscar_regra(g, nome) >= 0) {
        diagnostico_erro(g, linha, coluna,
                         "nao terminal %s definido mais de uma vez", nome);
        free(nome);
        no_liberar(expressao);
        return;
    }
    if (g->quantidade_regras == g->capacidade_regras) {
        g->capacidade_regras = g->capacidade_regras == 0
            ? 8 : g->capacidade_regras * 2;
        g->regras = (Regra *)xrealloc(g->regras,
            (size_t)g->capacidade_regras * sizeof(Regra));
    }
    r = &g->regras[g->quantidade_regras++];
    memset(r, 0, sizeof(Regra));
    r->nome = nome;
    r->expressao = expressao;
    r->linha = linha;
    r->coluna = coluna;
}

static int inicia_atomo(TipoTokenEbnf tipo)
{
    return tipo == TK_ID || tipo == TK_LITERAL || tipo == TK_ABRE_PAR ||
           tipo == TK_ABRE_COL || tipo == TK_ABRE_CHAVE;
}

static No *ler_alternativa(ParserEbnf *p);

static No *ler_atomo(ParserEbnf *p)
{
    No *no;
    No *filho;
    int linha;
    int coluna;
    int indice;
    linha = p->atual.linha;
    coluna = p->atual.coluna;
    if (p->atual.tipo == TK_ID) {
        no = novo_no(NO_SIMBOLO, linha, coluna);
        no->texto = xstrdup(p->atual.texto);
        avancar_parser(p);
        return no;
    }
    if (p->atual.tipo == TK_LITERAL) {
        no = novo_no(NO_SIMBOLO, linha, coluna);
        no->texto = xstrdup(p->atual.texto);
        no->classe_simbolo = 1;
        indice = adicionar_literal(p->g, p->atual.texto, linha, coluna);
        no->referencia = indice;
        avancar_parser(p);
        return no;
    }
    if (aceitar(p, TK_ABRE_PAR)) {
        filho = ler_alternativa(p);
        esperar(p, TK_FECHA_PAR);
        return filho;
    }
    if (aceitar(p, TK_ABRE_COL)) {
        no = novo_no(NO_OPCIONAL, linha, coluna);
        no_adicionar(no, ler_alternativa(p));
        esperar(p, TK_FECHA_COL);
        return no;
    }
    if (aceitar(p, TK_ABRE_CHAVE)) {
        no = novo_no(NO_REPETICAO, linha, coluna);
        no_adicionar(no, ler_alternativa(p));
        esperar(p, TK_FECHA_CHAVE);
        return no;
    }
    diagnostico_erro(p->g, linha, coluna, "esperada expressao EBNF");
    if (p->atual.tipo != TK_EOF) avancar_parser(p);
    return novo_no(NO_VAZIO, linha, coluna);
}

static No *ler_sequencia(ParserEbnf *p)
{
    No *seq;
    No *unico;
    int linha;
    int coluna;
    linha = p->atual.linha;
    coluna = p->atual.coluna;
    seq = novo_no(NO_SEQUENCIA, linha, coluna);
    while (inicia_atomo(p->atual.tipo)) {
        no_adicionar(seq, ler_atomo(p));
    }
    if (seq->quantidade == 0) {
        no_liberar(seq);
        return novo_no(NO_VAZIO, linha, coluna);
    }
    if (seq->quantidade == 1) {
        unico = seq->filhos[0];
        free(seq->filhos);
        free(seq);
        return unico;
    }
    return seq;
}

static No *ler_alternativa(ParserEbnf *p)
{
    No *alt;
    No *unico;
    int linha;
    int coluna;
    linha = p->atual.linha;
    coluna = p->atual.coluna;
    alt = novo_no(NO_ALTERNATIVA, linha, coluna);
    no_adicionar(alt, ler_sequencia(p));
    while (aceitar(p, TK_BARRA)) {
        no_adicionar(alt, ler_sequencia(p));
    }
    if (alt->quantidade == 1) {
        unico = alt->filhos[0];
        free(alt->filhos);
        free(alt);
        return unico;
    }
    return alt;
}

static void ler_secao_tokens(ParserEbnf *p)
{
    char *nome;
    char *padrao;
    int linha;
    int coluna;
    Terminal *t;
    while (p->atual.tipo != TK_RULES && p->atual.tipo != TK_EOF) {
        if (p->atual.tipo != TK_ID) {
            diagnostico_erro(p->g, p->atual.linha, p->atual.coluna,
                             "esperado nome de token ou RULES");
            avancar_parser(p);
            continue;
        }
        nome = xstrdup(p->atual.texto);
        linha = p->atual.linha;
        coluna = p->atual.coluna;
        avancar_parser(p);
        if (!esperar(p, TK_IGUAL) || p->atual.tipo != TK_REGEX) {
            diagnostico_erro(p->g, p->atual.linha, p->atual.coluna,
                             "esperada expressao regular para %s", nome);
            free(nome);
            while (p->atual.tipo != TK_PONTO_VIRGULA &&
                   p->atual.tipo != TK_EOF && p->atual.tipo != TK_RULES) {
                avancar_parser(p);
            }
            aceitar(p, TK_PONTO_VIRGULA);
            continue;
        }
        padrao = xstrdup(p->atual.texto);
        avancar_parser(p);
        esperar(p, TK_PONTO_VIRGULA);
        if (gramatica_buscar_terminal(p->g, nome) >= 0) {
            diagnostico_erro(p->g, linha, coluna,
                             "terminal %s declarado mais de uma vez", nome);
            free(nome);
            free(padrao);
            continue;
        }
        t = adicionar_terminal(p->g);
        t->nome = nome;
        t->padrao = padrao;
        t->linha = linha;
        t->coluna = coluna;
        t->ignorado = strcmp(nome, "SKIP") == 0;
    }
}

static void ler_secao_regras(ParserEbnf *p)
{
    char *nome;
    int linha;
    int coluna;
    No *expressao;
    while (p->atual.tipo != TK_EOF) {
        if (p->atual.tipo != TK_ID) {
            diagnostico_erro(p->g, p->atual.linha, p->atual.coluna,
                             "esperado nome de nao terminal");
            avancar_parser(p);
            continue;
        }
        nome = xstrdup(p->atual.texto);
        linha = p->atual.linha;
        coluna = p->atual.coluna;
        if (!islower((unsigned char)nome[0])) {
            diagnostico_erro(p->g, linha, coluna,
                             "nao terminal %s deve comecar com minuscula", nome);
        }
        avancar_parser(p);
        if (!esperar(p, TK_IGUAL)) {
            free(nome);
            while (p->atual.tipo != TK_PONTO_VIRGULA &&
                   p->atual.tipo != TK_EOF) avancar_parser(p);
            aceitar(p, TK_PONTO_VIRGULA);
            continue;
        }
        expressao = ler_alternativa(p);
        esperar(p, TK_PONTO_VIRGULA);
        adicionar_regra(p->g, nome, expressao, linha, coluna);
    }
}

int gramatica_ler(Gramatica *g, const char *entrada, size_t tamanho)
{
    ParserEbnf p;
    memset(&p, 0, sizeof(p));
    p.g = g;
    lexer_ebnf_inicializar(&p.lexer, entrada, tamanho, g->arquivo);
    p.atual = lexer_ebnf_proximo(&p.lexer);
    if (!esperar(&p, TK_TOKENS)) {
        token_ebnf_liberar(&p.atual);
        g->erros += p.lexer.erros;
        return 0;
    }
    ler_secao_tokens(&p);
    if (!esperar(&p, TK_RULES)) {
        token_ebnf_liberar(&p.atual);
        g->erros += p.lexer.erros;
        return 0;
    }
    ler_secao_regras(&p);
    token_ebnf_liberar(&p.atual);
    g->erros += p.lexer.erros;
    if (g->quantidade_regras == 0) {
        diagnostico_erro(g, 1, 1, "a gramatica nao possui regras");
    }
    return g->erros == 0;
}

static void imprimir_literal(FILE *saida, const char *texto)
{
    const unsigned char *p;
    fputc('"', saida);
    p = (const unsigned char *)texto;
    while (*p != '\0') {
        if (*p == '\n') fputs("\\n", saida);
        else if (*p == '\r') fputs("\\r", saida);
        else if (*p == '\t') fputs("\\t", saida);
        else if (*p == '\\' || *p == '"') {
            fputc('\\', saida);
            fputc(*p, saida);
        } else fputc(*p, saida);
        p++;
    }
    fputc('"', saida);
}

static void imprimir_no(const No *no, FILE *saida)
{
    int i;
    if (no->tipo == NO_VAZIO) return;
    if (no->tipo == NO_SIMBOLO) {
        if (no->classe_simbolo == 1 && no->texto != NULL &&
            !isupper((unsigned char)no->texto[0])) imprimir_literal(saida, no->texto);
        else fputs(no->texto, saida);
        return;
    }
    if (no->tipo == NO_OPCIONAL) fputs("[ ", saida);
    if (no->tipo == NO_REPETICAO) fputs("{ ", saida);
    if (no->tipo == NO_ALTERNATIVA) fputs("( ", saida);
    for (i = 0; i < no->quantidade; i++) {
        if (i > 0) {
            if (no->tipo == NO_ALTERNATIVA) fputs(" | ", saida);
            else fputc(' ', saida);
        }
        imprimir_no(no->filhos[i], saida);
    }
    if (no->tipo == NO_OPCIONAL) fputs(" ]", saida);
    if (no->tipo == NO_REPETICAO) fputs(" }", saida);
    if (no->tipo == NO_ALTERNATIVA) fputs(" )", saida);
}

void gramatica_imprimir(const Gramatica *g, FILE *saida)
{
    int i;
    fputs("TOKENS\n", saida);
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (!g->terminais[i].literal_fixo) {
            fprintf(saida, "  %s = /%s/ ;\n", g->terminais[i].nome,
                    g->terminais[i].padrao);
        }
    }
    fputs("RULES\n", saida);
    for (i = 0; i < g->quantidade_regras; i++) {
        fprintf(saida, "  %s = ", g->regras[i].nome);
        imprimir_no(g->regras[i].expressao, saida);
        fputs(" ;\n", saida);
    }
}
