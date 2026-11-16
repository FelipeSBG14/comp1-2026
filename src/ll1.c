#include "gerador.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *texto_intersecao(const Gramatica *g, const Conjunto *a,
                              const Conjunto *b)
{
    int i;
    size_t tamanho;
    char *texto;
    int primeiro;
    tamanho = 3;
    for (i = 0; i <= g->quantidade_tokens; i++) {
        if (conjunto_contem(a, i) && conjunto_contem(b, i)) {
            tamanho += strlen(gramatica_nome_token(g, i)) + 2;
        }
    }
    texto = (char *)xmalloc(tamanho);
    strcpy(texto, "{");
    primeiro = 1;
    for (i = 0; i <= g->quantidade_tokens; i++) {
        if (conjunto_contem(a, i) && conjunto_contem(b, i)) {
            if (!primeiro) strcat(texto, ", ");
            strcat(texto, gramatica_nome_token(g, i));
            primeiro = 0;
        }
    }
    strcat(texto, "}");
    return texto;
}

static void verificar_no(Gramatica *g, No *no, const char *regra)
{
    int i;
    int j;
    int anulaveis;
    char *itens;
    No *corpo;
    if (no->tipo == NO_ALTERNATIVA) {
        anulaveis = 0;
        for (i = 0; i < no->quantidade; i++) {
            if (no->filhos[i]->anulavel) anulaveis++;
            for (j = i + 1; j < no->quantidade; j++) {
                if (conjunto_intersecta(&no->filhos[i]->first,
                                        &no->filhos[j]->first)) {
                    itens = texto_intersecao(g, &no->filhos[i]->first,
                                             &no->filhos[j]->first);
                    diagnostico_erro(g, no->linha, no->coluna,
                        "conflito LL(1) em %s: alternativas %d e %d "
                        "comecam com %s", regra, i + 1, j + 1, itens);
                    free(itens);
                }
                if (no->filhos[i]->anulavel &&
                    conjunto_intersecta(&no->follow,
                                        &no->filhos[j]->first)) {
                    itens = texto_intersecao(g, &no->follow,
                                             &no->filhos[j]->first);
                    diagnostico_erro(g, no->linha, no->coluna,
                        "conflito LL(1) em %s: alternativa anulavel %d "
                        "conflita com FOLLOW em %s", regra, i + 1, itens);
                    free(itens);
                }
                if (no->filhos[j]->anulavel &&
                    conjunto_intersecta(&no->follow,
                                        &no->filhos[i]->first)) {
                    itens = texto_intersecao(g, &no->follow,
                                             &no->filhos[i]->first);
                    diagnostico_erro(g, no->linha, no->coluna,
                        "conflito LL(1) em %s: alternativa anulavel %d "
                        "conflita com FOLLOW em %s", regra, j + 1, itens);
                    free(itens);
                }
            }
        }
        if (anulaveis > 1) {
            diagnostico_erro(g, no->linha, no->coluna,
                             "conflito LL(1) em %s: %d alternativas anulaveis",
                             regra, anulaveis);
        }
    }
    if (no->tipo == NO_OPCIONAL || no->tipo == NO_REPETICAO) {
        corpo = no->filhos[0];
        if (corpo->anulavel) {
            diagnostico_erro(g, no->linha, no->coluna,
                "%s em %s possui conteudo anulavel",
                no->tipo == NO_OPCIONAL ? "opcional" : "repeticao", regra);
        } else if (conjunto_intersecta(&corpo->first, &no->follow)) {
            itens = texto_intersecao(g, &corpo->first, &no->follow);
            diagnostico_aviso(g, no->linha, no->coluna,
                "conflito FIRST/FOLLOW em %s de %s com %s; "
                "resolucao gulosa", no->tipo == NO_OPCIONAL
                ? "opcional" : "repeticao", regra, itens);
            free(itens);
        }
    }
    for (i = 0; i < no->quantidade; i++) {
        verificar_no(g, no->filhos[i], regra);
    }
}

int verificar_ll1(Gramatica *g)
{
    int i;
    int erros_antes;
    erros_antes = g->erros;
    for (i = 0; i < g->quantidade_regras; i++) {
        verificar_no(g, g->regras[i].expressao, g->regras[i].nome);
    }
    if (g->erros != erros_antes) {
        fprintf(stderr, "%s: a gramatica nao e LL(1); "
                        "nenhum codigo foi gerado\n", g->arquivo);
    }
    return g->erros == erros_antes;
}
