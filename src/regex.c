#include "gerador.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFA_EPSILON 0
#define NFA_DIVISAO 1
#define NFA_CLASSE 2
#define NFA_QUALQUER 3
#define NFA_FINAL 4

typedef struct Fragmento {
    int inicio;
    int fim;
} Fragmento;

typedef struct ParserRegex {
    const char *padrao;
    size_t posicao;
    Nfa *nfa;
    char *erro;
    size_t tamanho_erro;
    int falhou;
} ParserRegex;

static void definir_erro(ParserRegex *p, const char *mensagem)
{
    if (p->falhou) return;
    p->falhou = 1;
    if (p->tamanho_erro > 0) {
        strncpy(p->erro, mensagem, p->tamanho_erro - 1);
        p->erro[p->tamanho_erro - 1] = '\0';
    }
}

static int novo_estado(ParserRegex *p, int tipo)
{
    EstadoNfa *estado;
    int indice;
    if (p->nfa->quantidade == p->nfa->capacidade) {
        p->nfa->capacidade = p->nfa->capacidade == 0
            ? 16 : p->nfa->capacidade * 2;
        p->nfa->estados = (EstadoNfa *)xrealloc(p->nfa->estados,
            (size_t)p->nfa->capacidade * sizeof(EstadoNfa));
    }
    indice = p->nfa->quantidade++;
    estado = &p->nfa->estados[indice];
    memset(estado, 0, sizeof(EstadoNfa));
    estado->tipo = tipo;
    estado->saida1 = -1;
    estado->saida2 = -1;
    return indice;
}

static Fragmento fragmento_vazio(ParserRegex *p)
{
    Fragmento f;
    f.inicio = novo_estado(p, NFA_EPSILON);
    f.fim = novo_estado(p, NFA_EPSILON);
    p->nfa->estados[f.inicio].saida1 = f.fim;
    return f;
}

static Fragmento fragmento_classe(ParserRegex *p,
                                   const unsigned char *classe, int tipo)
{
    Fragmento f;
    f.inicio = novo_estado(p, tipo);
    f.fim = novo_estado(p, NFA_EPSILON);
    p->nfa->estados[f.inicio].saida1 = f.fim;
    if (classe != NULL) {
        memcpy(p->nfa->estados[f.inicio].classe, classe, 32);
    }
    return f;
}

static Fragmento concatenar(ParserRegex *p, Fragmento a, Fragmento b)
{
    p->nfa->estados[a.fim].saida1 = b.inicio;
    a.fim = b.fim;
    return a;
}

static Fragmento alternar(ParserRegex *p, Fragmento a, Fragmento b)
{
    Fragmento f;
    f.inicio = novo_estado(p, NFA_DIVISAO);
    f.fim = novo_estado(p, NFA_EPSILON);
    p->nfa->estados[f.inicio].saida1 = a.inicio;
    p->nfa->estados[f.inicio].saida2 = b.inicio;
    p->nfa->estados[a.fim].saida1 = f.fim;
    p->nfa->estados[b.fim].saida1 = f.fim;
    return f;
}

static int escape_regex(ParserRegex *p)
{
    int c;
    c = (unsigned char)p->padrao[p->posicao];
    if (c == '\0') {
        definir_erro(p, "escape incompleto");
        return 0;
    }
    p->posicao++;
    if (c == 'n') return '\n';
    if (c == 'r') return '\r';
    if (c == 't') return '\t';
    if (c == '0') return '\0';
    return c;
}

static void classe_adicionar(unsigned char *classe, int c)
{
    classe[((unsigned int)c) / 8] = (unsigned char)
        (classe[((unsigned int)c) / 8] | (1u << (((unsigned int)c) % 8)));
}

static Fragmento ler_classe(ParserRegex *p)
{
    unsigned char classe[32];
    int negar;
    int primeiro;
    int c;
    int fim;
    int i;
    memset(classe, 0, sizeof(classe));
    negar = 0;
    primeiro = 1;
    p->posicao++;
    if (p->padrao[p->posicao] == '^') {
        negar = 1;
        p->posicao++;
    }
    while (p->padrao[p->posicao] != '\0' &&
           (p->padrao[p->posicao] != ']' || primeiro)) {
        primeiro = 0;
        if (p->padrao[p->posicao] == '\\') {
            p->posicao++;
            c = escape_regex(p);
        } else {
            c = (unsigned char)p->padrao[p->posicao++];
        }
        if (p->padrao[p->posicao] == '-' &&
            p->padrao[p->posicao + 1] != ']' &&
            p->padrao[p->posicao + 1] != '\0') {
            p->posicao++;
            if (p->padrao[p->posicao] == '\\') {
                p->posicao++;
                fim = escape_regex(p);
            } else {
                fim = (unsigned char)p->padrao[p->posicao++];
            }
            if (fim < c) {
                definir_erro(p, "intervalo invertido em classe");
                return fragmento_vazio(p);
            }
            for (i = c; i <= fim; i++) classe_adicionar(classe, i);
        } else {
            classe_adicionar(classe, c);
        }
    }
    if (p->padrao[p->posicao] != ']') {
        definir_erro(p, "classe de caracteres nao terminada");
        return fragmento_vazio(p);
    }
    p->posicao++;
    if (negar) {
        for (i = 0; i < 32; i++) classe[i] = (unsigned char)~classe[i];
    }
    return fragmento_classe(p, classe, NFA_CLASSE);
}

static Fragmento ler_alternancia(ParserRegex *p);

static Fragmento ler_atomo_regex(ParserRegex *p)
{
    unsigned char classe[32];
    Fragmento f;
    int c;
    c = (unsigned char)p->padrao[p->posicao];
    if (c == '*' || c == '+' || c == '?') {
        definir_erro(p, "operador de repeticao sem operando");
        p->posicao++;
        return fragmento_vazio(p);
    }
    if (c == '(') {
        p->posicao++;
        f = ler_alternancia(p);
        if (p->padrao[p->posicao] != ')') {
            definir_erro(p, "parentese nao terminado");
        } else {
            p->posicao++;
        }
        return f;
    }
    if (c == '[') return ler_classe(p);
    if (c == '.') {
        p->posicao++;
        return fragmento_classe(p, NULL, NFA_QUALQUER);
    }
    if (c == '\\') {
        p->posicao++;
        c = escape_regex(p);
    } else {
        if (c == '\0' || c == ')' || c == '|') {
            definir_erro(p, "atomo ausente na expressao regular");
            return fragmento_vazio(p);
        }
        p->posicao++;
    }
    memset(classe, 0, sizeof(classe));
    classe_adicionar(classe, c);
    return fragmento_classe(p, classe, NFA_CLASSE);
}

static Fragmento aplicar_repeticao(ParserRegex *p, Fragmento filho, int op)
{
    Fragmento f;
    int divisao;
    if (op == '*') {
        f.inicio = novo_estado(p, NFA_DIVISAO);
        f.fim = novo_estado(p, NFA_EPSILON);
        p->nfa->estados[f.inicio].saida1 = filho.inicio;
        p->nfa->estados[f.inicio].saida2 = f.fim;
        p->nfa->estados[filho.fim].saida1 = f.inicio;
        return f;
    }
    if (op == '+') {
        f.inicio = filho.inicio;
        f.fim = novo_estado(p, NFA_EPSILON);
        divisao = novo_estado(p, NFA_DIVISAO);
        p->nfa->estados[divisao].saida1 = filho.inicio;
        p->nfa->estados[divisao].saida2 = f.fim;
        p->nfa->estados[filho.fim].saida1 = divisao;
        return f;
    }
    f.inicio = novo_estado(p, NFA_DIVISAO);
    f.fim = novo_estado(p, NFA_EPSILON);
    p->nfa->estados[f.inicio].saida1 = filho.inicio;
    p->nfa->estados[f.inicio].saida2 = f.fim;
    p->nfa->estados[filho.fim].saida1 = f.fim;
    return f;
}

static Fragmento ler_repeticao(ParserRegex *p)
{
    Fragmento f;
    int op;
    f = ler_atomo_regex(p);
    while (p->padrao[p->posicao] == '*' ||
           p->padrao[p->posicao] == '+' ||
           p->padrao[p->posicao] == '?') {
        op = (unsigned char)p->padrao[p->posicao++];
        f = aplicar_repeticao(p, f, op);
    }
    return f;
}

static int inicia_atomo_regex(int c)
{
    return c != '\0' && c != ')' && c != '|';
}

static Fragmento ler_concatenacao(ParserRegex *p)
{
    Fragmento f;
    Fragmento proximo;
    if (!inicia_atomo_regex((unsigned char)p->padrao[p->posicao])) {
        return fragmento_vazio(p);
    }
    f = ler_repeticao(p);
    while (inicia_atomo_regex((unsigned char)p->padrao[p->posicao])) {
        proximo = ler_repeticao(p);
        f = concatenar(p, f, proximo);
    }
    return f;
}

static Fragmento ler_alternancia(ParserRegex *p)
{
    Fragmento f;
    Fragmento direita;
    f = ler_concatenacao(p);
    while (p->padrao[p->posicao] == '|') {
        p->posicao++;
        direita = ler_concatenacao(p);
        f = alternar(p, f, direita);
    }
    return f;
}

static int alcanca_final_vazio(const Nfa *nfa, int estado,
                               unsigned char *visitado)
{
    const EstadoNfa *e;
    if (estado < 0 || visitado[estado]) return 0;
    visitado[estado] = 1;
    e = &nfa->estados[estado];
    if (e->tipo == NFA_FINAL) return 1;
    if (e->tipo == NFA_EPSILON) {
        return alcanca_final_vazio(nfa, e->saida1, visitado);
    }
    if (e->tipo == NFA_DIVISAO) {
        return alcanca_final_vazio(nfa, e->saida1, visitado) ||
               alcanca_final_vazio(nfa, e->saida2, visitado);
    }
    return 0;
}

int regex_compilar(const char *padrao, Nfa *nfa, char *erro,
                   size_t tamanho_erro)
{
    ParserRegex p;
    Fragmento f;
    int final;
    unsigned char *visitado;
    memset(nfa, 0, sizeof(Nfa));
    memset(&p, 0, sizeof(p));
    p.padrao = padrao;
    p.nfa = nfa;
    p.erro = erro;
    p.tamanho_erro = tamanho_erro;
    if (padrao[0] == '\0') {
        definir_erro(&p, "padrao vazio");
        return 0;
    }
    f = ler_alternancia(&p);
    if (!p.falhou && p.padrao[p.posicao] != '\0') {
        definir_erro(&p, "caractere inesperado na expressao regular");
    }
    if (p.falhou) {
        nfa_liberar(nfa);
        return 0;
    }
    final = novo_estado(&p, NFA_FINAL);
    nfa->estados[f.fim].saida1 = final;
    nfa->inicial = f.inicio;
    visitado = (unsigned char *)xmalloc((size_t)nfa->quantidade);
    memset(visitado, 0, (size_t)nfa->quantidade);
    if (alcanca_final_vazio(nfa, nfa->inicial, visitado)) {
        free(visitado);
        if (tamanho_erro > 0) {
            strncpy(erro, "padrao reconhece cadeia vazia", tamanho_erro - 1);
            erro[tamanho_erro - 1] = '\0';
        }
        nfa_liberar(nfa);
        return 0;
    }
    free(visitado);
    return 1;
}

void nfa_liberar(Nfa *nfa)
{
    free(nfa->estados);
    nfa->estados = NULL;
    nfa->quantidade = 0;
    nfa->capacidade = 0;
    nfa->inicial = 0;
}

static void fecho_dfa(const Nfa *nfa, unsigned char *conjunto, int estado)
{
    const EstadoNfa *e;
    if (estado < 0 || conjunto[estado]) return;
    conjunto[estado] = 1;
    e = &nfa->estados[estado];
    if (e->tipo == NFA_EPSILON) {
        fecho_dfa(nfa, conjunto, e->saida1);
    } else if (e->tipo == NFA_DIVISAO) {
        fecho_dfa(nfa, conjunto, e->saida1);
        fecho_dfa(nfa, conjunto, e->saida2);
    }
}

static int subconjunto_final(const Nfa *nfa, const unsigned char *conjunto)
{
    int i;
    for (i = 0; i < nfa->quantidade; i++) {
        if (conjunto[i] && nfa->estados[i].tipo == NFA_FINAL) return 1;
    }
    return 0;
}

static int encontrar_subconjunto(const unsigned char *subconjuntos,
                                 int quantidade, int largura,
                                 const unsigned char *procurado)
{
    int i;
    for (i = 0; i < quantidade; i++) {
        if (memcmp(subconjuntos + i * largura, procurado,
                   (size_t)largura) == 0) return i;
    }
    return -1;
}

static int mesma_assinatura(const Dfa *dfa, int a, int b,
                            const int *grupos)
{
    int c;
    int ta;
    int tb;
    if (dfa->estados[a].final != dfa->estados[b].final) return 0;
    for (c = 0; c < 256; c++) {
        ta = dfa->estados[a].transicoes[c];
        tb = dfa->estados[b].transicoes[c];
        ta = ta < 0 ? -1 : grupos[ta];
        tb = tb < 0 ? -1 : grupos[tb];
        if (ta != tb) return 0;
    }
    return 1;
}

static int minimizar_dfa(Dfa *dfa)
{
    int *grupos;
    int *novos;
    int *representantes;
    int i;
    int j;
    int c;
    int quantidade_grupos;
    int mudou;
    int destino;
    Dfa minimo;
    grupos = (int *)xmalloc((size_t)dfa->quantidade * sizeof(int));
    novos = (int *)xmalloc((size_t)dfa->quantidade * sizeof(int));
    for (i = 0; i < dfa->quantidade; i++) {
        grupos[i] = dfa->estados[i].final ? 1 : 0;
    }
    do {
        quantidade_grupos = 0;
        for (i = 0; i < dfa->quantidade; i++) {
            novos[i] = -1;
            for (j = 0; j < i; j++) {
                if (mesma_assinatura(dfa, i, j, grupos)) {
                    novos[i] = novos[j];
                    break;
                }
            }
            if (novos[i] < 0) novos[i] = quantidade_grupos++;
        }
        mudou = 0;
        for (i = 0; i < dfa->quantidade; i++) {
            if (novos[i] != grupos[i]) mudou = 1;
            grupos[i] = novos[i];
        }
    } while (mudou);
    representantes = (int *)xmalloc((size_t)quantidade_grupos * sizeof(int));
    for (i = 0; i < quantidade_grupos; i++) representantes[i] = -1;
    for (i = 0; i < dfa->quantidade; i++) {
        if (representantes[grupos[i]] < 0) representantes[grupos[i]] = i;
    }
    minimo.quantidade = quantidade_grupos;
    minimo.inicial = grupos[dfa->inicial];
    minimo.estados = (EstadoDfa *)xmalloc(
        (size_t)quantidade_grupos * sizeof(EstadoDfa));
    for (i = 0; i < quantidade_grupos; i++) {
        j = representantes[i];
        minimo.estados[i].final = dfa->estados[j].final;
        for (c = 0; c < 256; c++) {
            destino = dfa->estados[j].transicoes[c];
            minimo.estados[i].transicoes[c] =
                destino < 0 ? -1 : grupos[destino];
        }
    }
    free(representantes);
    free(novos);
    free(grupos);
    free(dfa->estados);
    *dfa = minimo;
    return 1;
}

int dfa_construir_minimo(const Nfa *nfa, Dfa *dfa)
{
    int capacidade;
    int largura;
    unsigned char *subconjuntos;
    unsigned char *destino;
    unsigned char *atual;
    int indice;
    int encontrado;
    int i;
    int c;
    const EstadoNfa *e;
    memset(dfa, 0, sizeof(Dfa));
    largura = nfa->quantidade;
    capacidade = 8;
    subconjuntos = (unsigned char *)xmalloc(
        (size_t)capacidade * (size_t)largura);
    dfa->estados = (EstadoDfa *)xmalloc(
        (size_t)capacidade * sizeof(EstadoDfa));
    memset(subconjuntos, 0, (size_t)capacidade * (size_t)largura);
    fecho_dfa(nfa, subconjuntos, nfa->inicial);
    dfa->quantidade = 1;
    dfa->inicial = 0;
    destino = (unsigned char *)xmalloc((size_t)largura);
    for (indice = 0; indice < dfa->quantidade; indice++) {
        atual = subconjuntos + indice * largura;
        dfa->estados[indice].final = subconjunto_final(nfa, atual);
        for (c = 0; c < 256; c++) {
            memset(destino, 0, (size_t)largura);
            for (i = 0; i < nfa->quantidade; i++) {
                if (!atual[i]) continue;
                e = &nfa->estados[i];
                if ((e->tipo == NFA_CLASSE &&
                     (e->classe[c / 8] & (1u << (c % 8)))) ||
                    e->tipo == NFA_QUALQUER) {
                    fecho_dfa(nfa, destino, e->saida1);
                }
            }
            encontrado = encontrar_subconjunto(subconjuntos,
                dfa->quantidade, largura, destino);
            if (encontrado < 0) {
                int vazio;
                vazio = 1;
                for (i = 0; i < largura; i++) {
                    if (destino[i]) { vazio = 0; break; }
                }
                if (vazio) {
                    dfa->estados[indice].transicoes[c] = -1;
                    continue;
                }
                if (dfa->quantidade == capacidade) {
                    capacidade *= 2;
                    subconjuntos = (unsigned char *)xrealloc(subconjuntos,
                        (size_t)capacidade * (size_t)largura);
                    dfa->estados = (EstadoDfa *)xrealloc(dfa->estados,
                        (size_t)capacidade * sizeof(EstadoDfa));
                    atual = subconjuntos + indice * largura;
                }
                encontrado = dfa->quantidade++;
                memcpy(subconjuntos + encontrado * largura, destino,
                       (size_t)largura);
            }
            dfa->estados[indice].transicoes[c] = encontrado;
        }
    }
    free(destino);
    free(subconjuntos);
    return minimizar_dfa(dfa);
}

void dfa_liberar(Dfa *dfa)
{
    free(dfa->estados);
    dfa->estados = NULL;
    dfa->quantidade = 0;
    dfa->inicial = 0;
}
