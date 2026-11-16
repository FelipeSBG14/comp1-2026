#include "gerador.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t tamanho)
{
    void *p;
    if (tamanho == 0) {
        tamanho = 1;
    }
    p = malloc(tamanho);
    if (p == NULL) {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(2);
    }
    return p;
}

void *xrealloc(void *ponteiro, size_t tamanho)
{
    void *p;
    if (tamanho == 0) {
        tamanho = 1;
    }
    p = realloc(ponteiro, tamanho);
    if (p == NULL) {
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        exit(2);
    }
    return p;
}

char *xstrndup(const char *texto, size_t tamanho)
{
    char *copia;
    copia = (char *)xmalloc(tamanho + 1);
    memcpy(copia, texto, tamanho);
    copia[tamanho] = '\0';
    return copia;
}

char *xstrdup(const char *texto)
{
    return xstrndup(texto, strlen(texto));
}

char *ler_arquivo(const char *caminho, size_t *tamanho)
{
    FILE *arquivo;
    long fim;
    size_t lidos;
    char *conteudo;
    arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        return NULL;
    }
    if (fseek(arquivo, 0L, SEEK_END) != 0) {
        fclose(arquivo);
        return NULL;
    }
    fim = ftell(arquivo);
    if (fim < 0 || fseek(arquivo, 0L, SEEK_SET) != 0) {
        fclose(arquivo);
        return NULL;
    }
    conteudo = (char *)xmalloc((size_t)fim + 1);
    lidos = fread(conteudo, 1, (size_t)fim, arquivo);
    fclose(arquivo);
    if (lidos != (size_t)fim) {
        free(conteudo);
        return NULL;
    }
    conteudo[lidos] = '\0';
    *tamanho = lidos;
    return conteudo;
}

static void diagnostico(Gramatica *g, const char *nivel, int linha,
                        int coluna, const char *formato, va_list argumentos)
{
    fprintf(stderr, "%s:%d:%d: %s: ", g->arquivo, linha, coluna, nivel);
    vfprintf(stderr, formato, argumentos);
    fputc('\n', stderr);
}

void diagnostico_erro(Gramatica *g, int linha, int coluna,
                      const char *formato, ...)
{
    va_list argumentos;
    va_start(argumentos, formato);
    diagnostico(g, "erro", linha, coluna, formato, argumentos);
    va_end(argumentos);
    g->erros++;
}

void diagnostico_aviso(Gramatica *g, int linha, int coluna,
                       const char *formato, ...)
{
    va_list argumentos;
    va_start(argumentos, formato);
    diagnostico(g, "aviso", linha, coluna, formato, argumentos);
    va_end(argumentos);
    g->avisos++;
}

void conjunto_inicializar(Conjunto *c, int tamanho)
{
    int bytes;
    c->tamanho = tamanho;
    bytes = (tamanho + 7) / 8;
    c->dados = (unsigned char *)xmalloc((size_t)bytes);
    memset(c->dados, 0, (size_t)bytes);
}

void conjunto_liberar(Conjunto *c)
{
    free(c->dados);
    c->dados = NULL;
    c->tamanho = 0;
}

void conjunto_limpar(Conjunto *c)
{
    memset(c->dados, 0, (size_t)((c->tamanho + 7) / 8));
}

int conjunto_adicionar(Conjunto *c, int elemento)
{
    unsigned char mascara;
    int byte;
    if (elemento < 0 || elemento >= c->tamanho) {
        return 0;
    }
    byte = elemento / 8;
    mascara = (unsigned char)(1u << (elemento % 8));
    if ((c->dados[byte] & mascara) != 0) {
        return 0;
    }
    c->dados[byte] = (unsigned char)(c->dados[byte] | mascara);
    return 1;
}

int conjunto_unir(Conjunto *destino, const Conjunto *origem)
{
    int i;
    int bytes;
    int mudou;
    unsigned char antigo;
    bytes = (destino->tamanho + 7) / 8;
    mudou = 0;
    for (i = 0; i < bytes; i++) {
        antigo = destino->dados[i];
        destino->dados[i] = (unsigned char)(antigo | origem->dados[i]);
        if (destino->dados[i] != antigo) {
            mudou = 1;
        }
    }
    return mudou;
}

int conjunto_intersecta(const Conjunto *a, const Conjunto *b)
{
    int i;
    int bytes;
    bytes = (a->tamanho + 7) / 8;
    for (i = 0; i < bytes; i++) {
        if ((a->dados[i] & b->dados[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

int conjunto_contem(const Conjunto *c, int elemento)
{
    if (elemento < 0 || elemento >= c->tamanho) {
        return 0;
    }
    return (c->dados[elemento / 8] & (1u << (elemento % 8))) != 0;
}

int conjunto_igual(const Conjunto *a, const Conjunto *b)
{
    int bytes;
    if (a->tamanho != b->tamanho) {
        return 0;
    }
    bytes = (a->tamanho + 7) / 8;
    return memcmp(a->dados, b->dados, (size_t)bytes) == 0;
}
