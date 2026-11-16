#include "gerador.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void atribuir_indices(Gramatica *g)
{
    int i;
    int indice;
    indice = 0;
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (g->terminais[i].literal_fixo) {
            g->terminais[i].indice = indice++;
        }
    }
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (!g->terminais[i].literal_fixo && !g->terminais[i].ignorado) {
            g->terminais[i].indice = indice++;
        }
    }
    g->quantidade_tokens = indice;
}

static void resolver_no(Gramatica *g, No *no)
{
    int i;
    int indice;
    Terminal *terminal;
    if (no->tipo == NO_SIMBOLO) {
        if (no->classe_simbolo == 1 && no->referencia >= 0) {
            terminal = &g->terminais[no->referencia];
            no->referencia = terminal->indice;
            return;
        }
        if (isupper((unsigned char)no->texto[0])) {
            indice = gramatica_buscar_terminal(g, no->texto);
            if (indice < 0 || g->terminais[indice].ignorado ||
                g->terminais[indice].literal_fixo) {
                diagnostico_erro(g, no->linha, no->coluna,
                                 "terminal %s nao declarado em TOKENS",
                                 no->texto);
                no->referencia = -1;
            } else {
                no->referencia = g->terminais[indice].indice;
            }
            no->classe_simbolo = 1;
        } else {
            indice = gramatica_buscar_regra(g, no->texto);
            if (indice < 0) {
                diagnostico_erro(g, no->linha, no->coluna,
                                 "nao terminal %s usado mas nao definido",
                                 no->texto);
                no->referencia = -1;
            } else {
                no->referencia = indice;
            }
            no->classe_simbolo = 2;
        }
        return;
    }
    for (i = 0; i < no->quantidade; i++) resolver_no(g, no->filhos[i]);
}

static int calcular_anulavel_no(Gramatica *g, No *no)
{
    int i;
    int valor;
    int anterior;
    anterior = no->anulavel;
    valor = 0;
    if (no->tipo == NO_VAZIO || no->tipo == NO_OPCIONAL ||
        no->tipo == NO_REPETICAO) {
        valor = 1;
    } else if (no->tipo == NO_SIMBOLO) {
        if (no->classe_simbolo == 2 && no->referencia >= 0) {
            valor = g->regras[no->referencia].expressao->anulavel;
        }
    } else if (no->tipo == NO_SEQUENCIA) {
        valor = 1;
        for (i = 0; i < no->quantidade; i++) {
            calcular_anulavel_no(g, no->filhos[i]);
            if (!no->filhos[i]->anulavel) valor = 0;
        }
    } else if (no->tipo == NO_ALTERNATIVA) {
        for (i = 0; i < no->quantidade; i++) {
            calcular_anulavel_no(g, no->filhos[i]);
            if (no->filhos[i]->anulavel) valor = 1;
        }
    }
    if (no->tipo == NO_OPCIONAL || no->tipo == NO_REPETICAO) {
        calcular_anulavel_no(g, no->filhos[0]);
    }
    no->anulavel = valor;
    return anterior != valor;
}

static void calcular_anulaveis(Gramatica *g)
{
    int mudou;
    int i;
    do {
        mudou = 0;
        for (i = 0; i < g->quantidade_regras; i++) {
            if (calcular_anulavel_no(g, g->regras[i].expressao)) mudou = 1;
        }
    } while (mudou);
}

static int produtividade_no(Gramatica *g, No *no)
{
    int i;
    int valor;
    valor = 0;
    if (no->tipo == NO_VAZIO || no->tipo == NO_OPCIONAL ||
        no->tipo == NO_REPETICAO) {
        valor = 1;
    } else if (no->tipo == NO_SIMBOLO) {
        if (no->classe_simbolo == 1 && no->referencia >= 0) valor = 1;
        else if (no->classe_simbolo == 2 && no->referencia >= 0)
            valor = g->regras[no->referencia].produtivo;
    } else if (no->tipo == NO_SEQUENCIA) {
        valor = 1;
        for (i = 0; i < no->quantidade; i++) {
            if (!produtividade_no(g, no->filhos[i])) valor = 0;
        }
    } else if (no->tipo == NO_ALTERNATIVA) {
        for (i = 0; i < no->quantidade; i++) {
            if (produtividade_no(g, no->filhos[i])) valor = 1;
        }
    }
    no->produtivo = valor;
    return valor;
}

static void verificar_produtividade(Gramatica *g)
{
    int mudou;
    int i;
    int valor;
    do {
        mudou = 0;
        for (i = 0; i < g->quantidade_regras; i++) {
            valor = produtividade_no(g, g->regras[i].expressao);
            if (valor && !g->regras[i].produtivo) {
                g->regras[i].produtivo = 1;
                mudou = 1;
            }
        }
    } while (mudou);
    for (i = 0; i < g->quantidade_regras; i++) {
        if (!g->regras[i].produtivo) {
            diagnostico_erro(g, g->regras[i].linha, g->regras[i].coluna,
                             "nao terminal %s e improdutivo",
                             g->regras[i].nome);
        }
    }
}

static void marcar_referencias(Gramatica *g, No *no);

static void marcar_regra(Gramatica *g, int indice)
{
    if (indice < 0 || g->regras[indice].alcancavel) return;
    g->regras[indice].alcancavel = 1;
    marcar_referencias(g, g->regras[indice].expressao);
}

static void marcar_referencias(Gramatica *g, No *no)
{
    int i;
    if (no->tipo == NO_SIMBOLO && no->classe_simbolo == 2) {
        marcar_regra(g, no->referencia);
    }
    for (i = 0; i < no->quantidade; i++) marcar_referencias(g, no->filhos[i]);
}

static void verificar_alcance(Gramatica *g)
{
    int i;
    if (g->quantidade_regras == 0) return;
    marcar_regra(g, 0);
    for (i = 0; i < g->quantidade_regras; i++) {
        if (!g->regras[i].alcancavel) {
            diagnostico_aviso(g, g->regras[i].linha, g->regras[i].coluna,
                              "nao terminal %s e inalcancavel a partir de %s",
                              g->regras[i].nome, g->regras[0].nome);
        }
    }
}

static void coletar_iniciais(Gramatica *g, No *no, unsigned char *linha)
{
    int i;
    if (no->tipo == NO_SIMBOLO) {
        if (no->classe_simbolo == 2 && no->referencia >= 0) {
            linha[no->referencia] = 1;
        }
        return;
    }
    if (no->tipo == NO_SEQUENCIA) {
        for (i = 0; i < no->quantidade; i++) {
            coletar_iniciais(g, no->filhos[i], linha);
            if (!no->filhos[i]->anulavel) break;
        }
        return;
    }
    for (i = 0; i < no->quantidade; i++) {
        coletar_iniciais(g, no->filhos[i], linha);
    }
}

static int alcanca(unsigned char *arestas, int n, int atual, int alvo,
                   unsigned char *visitado)
{
    int j;
    if (visitado[atual]) return 0;
    visitado[atual] = 1;
    for (j = 0; j < n; j++) {
        if (!arestas[atual * n + j]) continue;
        if (j == alvo) return 1;
        if (alcanca(arestas, n, j, alvo, visitado)) return 1;
    }
    return 0;
}

static void verificar_recursao_esquerda(Gramatica *g)
{
    int n;
    int i;
    int j;
    unsigned char *arestas;
    unsigned char *visitado;
    n = g->quantidade_regras;
    if (n == 0) return;
    arestas = (unsigned char *)xmalloc((size_t)n * (size_t)n);
    memset(arestas, 0, (size_t)n * (size_t)n);
    for (i = 0; i < n; i++) {
        coletar_iniciais(g, g->regras[i].expressao, arestas + i * n);
    }
    visitado = (unsigned char *)xmalloc((size_t)n);
    for (i = 0; i < n; i++) {
        if (arestas[i * n + i]) {
            diagnostico_erro(g, g->regras[i].linha, g->regras[i].coluna,
                             "nao terminal %s tem recursao a esquerda direta",
                             g->regras[i].nome);
            continue;
        }
        for (j = 0; j < n; j++) {
            memset(visitado, 0, (size_t)n);
            if (j != i && arestas[i * n + j] &&
                alcanca(arestas, n, j, i, visitado)) {
                diagnostico_erro(g, g->regras[i].linha, g->regras[i].coluna,
                                 "nao terminal %s tem recursao a esquerda indireta",
                                 g->regras[i].nome);
                break;
            }
        }
    }
    free(visitado);
    free(arestas);
}

static void validar_terminais(Gramatica *g)
{
    int i;
    char erro[160];
    for (i = 0; i < g->quantidade_terminais; i++) {
        Terminal *t;
        t = &g->terminais[i];
        if (t->literal_fixo) {
            if (t->literal[0] == '\0') {
                diagnostico_erro(g, t->linha, t->coluna,
                                 "literal vazio nao pode ser token");
            }
            continue;
        }
        {
            const unsigned char *p;
            int valido;
            valido = isupper((unsigned char)t->nome[0]);
            for (p = (const unsigned char *)t->nome; *p; p++) {
                if (!isupper(*p) && !isdigit(*p) && *p != '_') valido = 0;
            }
            if (!valido) {
            diagnostico_erro(g, t->linha, t->coluna,
                             "terminal %s deve usar somente maiusculas, digitos e sublinhado e iniciar com maiuscula", t->nome);
            }
        }
        erro[0] = '\0';
        if (!regex_compilar(t->padrao, &t->nfa, erro, sizeof(erro))) {
            diagnostico_erro(g, t->linha, t->coluna,
                             "expressao regular invalida de %s: %s",
                             t->nome, erro);
        } else if (!dfa_construir_minimo(&t->nfa, &t->dfa)) {
            diagnostico_erro(g, t->linha, t->coluna,
                             "nao foi possivel construir o AFD de %s",
                             t->nome);
        }
    }
}

int verificar_gramatica(Gramatica *g)
{
    int i;
    atribuir_indices(g);
    validar_terminais(g);
    for (i = 0; i < g->quantidade_regras; i++) {
        resolver_no(g, g->regras[i].expressao);
    }
    calcular_anulaveis(g);
    verificar_produtividade(g);
    verificar_alcance(g);
    verificar_recursao_esquerda(g);
    return g->erros == 0;
}
