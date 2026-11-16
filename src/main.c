#include "gerador.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void uso(const char *programa)
{
    fprintf(stderr,
        "Uso: %s [--debug] [--sets] [-o prefixo] gramatica.ebnf\n",
        programa);
}

int main(int argc, char **argv)
{
    int i;
    int debug;
    int sets;
    const char *entrada;
    const char *prefixo;
    char *conteudo;
    size_t tamanho;
    Gramatica g;
    debug = 0;
    sets = 0;
    entrada = NULL;
    prefixo = "parser";
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) debug = 1;
        else if (strcmp(argv[i], "--sets") == 0) sets = 1;
        else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) {
                uso(argv[0]);
                return 2;
            }
            prefixo = argv[i];
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            uso(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "opcao desconhecida: %s\n", argv[i]);
            uso(argv[0]);
            return 2;
        } else if (entrada == NULL) entrada = argv[i];
        else {
            uso(argv[0]);
            return 2;
        }
    }
    if (entrada == NULL) {
        uso(argv[0]);
        return 2;
    }
    conteudo = ler_arquivo(entrada, &tamanho);
    if (conteudo == NULL) {
        fprintf(stderr, "erro: nao foi possivel ler %s\n", entrada);
        return 2;
    }
    gramatica_inicializar(&g, entrada);
    if (!gramatica_ler(&g, conteudo, tamanho) ||
        !verificar_gramatica(&g)) {
        gramatica_liberar(&g);
        free(conteudo);
        return 1;
    }
    calcular_conjuntos(&g);
    if (debug) gramatica_imprimir(&g, stdout);
    if (sets) imprimir_conjuntos(&g, stdout);
    if (!verificar_ll1(&g)) {
        gramatica_liberar(&g);
        free(conteudo);
        return 1;
    }
    if (!gerar_codigo(&g, prefixo)) {
        gramatica_liberar(&g);
        free(conteudo);
        return 1;
    }
    printf("gerados %s.c e %s.h\n", prefixo, prefixo);
    gramatica_liberar(&g);
    free(conteudo);
    return 0;
}
