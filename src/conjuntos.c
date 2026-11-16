#include "gerador.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void inicializar_no(No *no, int tamanho)
{
    int i;
    conjunto_inicializar(&no->first, tamanho);
    conjunto_inicializar(&no->follow, tamanho);
    for (i = 0; i < no->quantidade; i++) {
        inicializar_no(no->filhos[i], tamanho);
    }
}

static int first_no(Gramatica *g, No *no)
{
    int i;
    int mudou;
    int bytes;
    unsigned char *anterior;
    bytes = (no->first.tamanho + 7) / 8;
    anterior = (unsigned char *)xmalloc((size_t)bytes);
    memcpy(anterior, no->first.dados, (size_t)bytes);
    conjunto_limpar(&no->first);
    if (no->tipo == NO_SIMBOLO) {
        if (no->classe_simbolo == 1) {
            conjunto_adicionar(&no->first, no->referencia);
        } else if (no->classe_simbolo == 2 && no->referencia >= 0) {
            conjunto_unir(&no->first,
                          &g->regras[no->referencia].expressao->first);
        }
    } else if (no->tipo == NO_SEQUENCIA) {
        for (i = 0; i < no->quantidade; i++) {
            first_no(g, no->filhos[i]);
            conjunto_unir(&no->first, &no->filhos[i]->first);
            if (!no->filhos[i]->anulavel) break;
        }
        for (i++; i < no->quantidade; i++) first_no(g, no->filhos[i]);
    } else {
        for (i = 0; i < no->quantidade; i++) {
            first_no(g, no->filhos[i]);
            conjunto_unir(&no->first, &no->filhos[i]->first);
        }
    }
    mudou = memcmp(anterior, no->first.dados, (size_t)bytes) != 0;
    free(anterior);
    return mudou;
}

static int adicionar_first_restante(No *sequencia, int inicio,
                                    Conjunto *destino)
{
    int i;
    int mudou;
    mudou = 0;
    for (i = inicio; i < sequencia->quantidade; i++) {
        mudou |= conjunto_unir(destino, &sequencia->filhos[i]->first);
        if (!sequencia->filhos[i]->anulavel) break;
    }
    return mudou;
}

static int restante_anulavel(No *sequencia, int inicio)
{
    int i;
    for (i = inicio; i < sequencia->quantidade; i++) {
        if (!sequencia->filhos[i]->anulavel) return 0;
    }
    return 1;
}

static int propagar_follow(Gramatica *g, No *no)
{
    int i;
    int mudou;
    No *filho;
    mudou = 0;
    if (no->tipo == NO_SIMBOLO && no->classe_simbolo == 2 &&
        no->referencia >= 0) {
        mudou |= conjunto_unir(
            &g->regras[no->referencia].expressao->follow, &no->follow);
    } else if (no->tipo == NO_SEQUENCIA) {
        for (i = 0; i < no->quantidade; i++) {
            filho = no->filhos[i];
            mudou |= adicionar_first_restante(no, i + 1, &filho->follow);
            if (restante_anulavel(no, i + 1)) {
                mudou |= conjunto_unir(&filho->follow, &no->follow);
            }
        }
    } else if (no->tipo == NO_ALTERNATIVA) {
        for (i = 0; i < no->quantidade; i++) {
            mudou |= conjunto_unir(&no->filhos[i]->follow, &no->follow);
        }
    } else if (no->tipo == NO_OPCIONAL || no->tipo == NO_REPETICAO) {
        filho = no->filhos[0];
        mudou |= conjunto_unir(&filho->follow, &no->follow);
        if (no->tipo == NO_REPETICAO) {
            mudou |= conjunto_unir(&filho->follow, &filho->first);
        }
    }
    for (i = 0; i < no->quantidade; i++) {
        mudou |= propagar_follow(g, no->filhos[i]);
    }
    return mudou;
}

int calcular_conjuntos(Gramatica *g)
{
    int i;
    int mudou;
    int tamanho;
    tamanho = g->quantidade_tokens + 1;
    for (i = 0; i < g->quantidade_regras; i++) {
        inicializar_no(g->regras[i].expressao, tamanho);
    }
    do {
        mudou = 0;
        for (i = 0; i < g->quantidade_regras; i++) {
            mudou |= first_no(g, g->regras[i].expressao);
        }
    } while (mudou);
    if (g->quantidade_regras > 0) {
        conjunto_adicionar(&g->regras[0].expressao->follow,
                           g->quantidade_tokens);
    }
    do {
        mudou = 0;
        for (i = 0; i < g->quantidade_regras; i++) {
            mudou |= propagar_follow(g, g->regras[i].expressao);
        }
    } while (mudou);
    return 1;
}

static void imprimir_conjunto(const Gramatica *g, const Conjunto *c,
                              FILE *saida)
{
    int i;
    int primeiro;
    primeiro = 1;
    fputc('{', saida);
    for (i = 0; i <= g->quantidade_tokens; i++) {
        if (conjunto_contem(c, i)) {
            if (!primeiro) fputs(", ", saida);
            fprintf(saida, "%s", gramatica_nome_token(g, i));
            primeiro = 0;
        }
    }
    fputc('}', saida);
}

void imprimir_conjuntos(const Gramatica *g, FILE *saida)
{
    int i;
    for (i = 0; i < g->quantidade_regras; i++) {
        fprintf(saida, "%-20s anulavel=%s FIRST=", g->regras[i].nome,
                g->regras[i].expressao->anulavel ? "sim" : "nao");
        imprimir_conjunto(g, &g->regras[i].expressao->first, saida);
        fputs(" FOLLOW=", saida);
        imprimir_conjunto(g, &g->regras[i].expressao->follow, saida);
        fputc('\n', saida);
    }
}
