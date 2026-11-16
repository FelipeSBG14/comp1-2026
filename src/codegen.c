#include "gerador.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Terminal *terminal_por_indice(Gramatica *g, int indice)
{
    int i;
    for (i = 0; i < g->quantidade_terminais; i++) {
        if (g->terminais[i].indice == indice) return &g->terminais[i];
    }
    return NULL;
}

static void nome_enum(Gramatica *g, int indice, char *destino)
{
    if (indice == g->quantidade_tokens) {
        strcpy(destino, "T_END_OF_FILE");
        return;
    }
    sprintf(destino, "TOKEN_%d", indice + 1);
}

static void emitir_string_c(FILE *saida, const char *texto, size_t tamanho)
{
    size_t i;
    unsigned char c;
    fputc('"', saida);
    for (i = 0; i < tamanho; i++) {
        c = (unsigned char)texto[i];
        if (c == '\n') fputs("\\n", saida);
        else if (c == '\r') fputs("\\r", saida);
        else if (c == '\t') fputs("\\t", saida);
        else if (c == '\\') fputs("\\\\", saida);
        else if (c == '"') fputs("\\\"", saida);
        else if (c >= 32 && c <= 126) fputc(c, saida);
        else fprintf(saida, "\\%03o", (unsigned int)c);
    }
    fputc('"', saida);
}

static const char *nome_base(const char *caminho)
{
    const char *p;
    const char *base;
    base = caminho;
    for (p = caminho; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}

static int gerar_cabecalho(Gramatica *g, const char *caminho)
{
    FILE *f;
    int i;
    char nome[160];
    f = fopen(caminho, "wb");
    if (f == NULL) return 0;
    fputs("#ifndef PARSER_GERADO_H\n#define PARSER_GERADO_H\n\n", f);
    fputs("typedef enum TipoToken {\n"
          "    T_UNDEF = 0,\n", f);
    for (i = 0; i < g->quantidade_tokens; i++) {
        nome_enum(g, i, nome);
        fprintf(f, "    %s = %d,\n", nome, i + 1);
    }
    fprintf(f, "    T_END_OF_FILE = %d\n", g->quantidade_tokens + 1);
    fputs("} TipoToken;\n\n", f);
    for (i = 0; i < g->quantidade_tokens; i++) {
        Terminal *t;
        t = terminal_por_indice(g, i);
        if (!t->literal_fixo && strcmp(t->nome, "UNDEF") != 0 &&
            strcmp(t->nome, "END_OF_FILE") != 0) {
            fprintf(f, "#define T_%s TOKEN_%d\n", t->nome, i + 1);
        }
    }
    fputs("typedef struct Token {\n"
          "    int tipo;\n"
          "    const char *lexema;\n"
          "    int tamanho;\n"
          "    int linha;\n"
          "    int coluna;\n"
          "    const char *erro;\n"
          "} Token;\n\n", f);
    fputs("int analisar_arquivo(const char *caminho, int mostrar_arvore);\n"
          "int listar_tokens(const char *caminho);\n\n"
          "#endif\n", f);
    fclose(f);
    return 1;
}

static void emitir_nfas(Gramatica *g, FILE *f)
{
    int i;
    int j;
    int k;
    Terminal *t;
    int quantidade_regex;
    quantidade_regex = 0;
    for (i = 0; i < g->quantidade_terminais; i++) {
        t = &g->terminais[i];
        if (t->literal_fixo) continue;
        fprintf(f, "static const EstadoDfa dfa_%d[] = {\n", i);
        for (j = 0; j < t->dfa.quantidade; j++) {
            fputs("    {{\n        ", f);
            for (k = 0; k < 256; k++) {
                if (k > 0) fputs(",", f);
                if (k > 0 && k % 16 == 0) fputs("\n        ", f);
                fprintf(f, "%d", t->dfa.estados[j].transicoes[k]);
            }
            fprintf(f, "\n    }, %d},\n", t->dfa.estados[j].final);
        }
        fputs("};\n", f);
        quantidade_regex++;
    }
    if (quantidade_regex == 0) {
        fputs("static const EstadoDfa dfa_vazio[] = {{{0},0}};\n", f);
    }
    fputs("\nstatic const DefRegex definicoes_regex[] = {\n", f);
    if (quantidade_regex == 0) {
        fputs("    {dfa_vazio, 1, 0, T_UNDEF, 0}\n", f);
    } else {
        for (i = 0; i < g->quantidade_terminais; i++) {
            t = &g->terminais[i];
            if (t->literal_fixo) continue;
            fprintf(f, "    {dfa_%d, %d, %d, ", i, t->dfa.quantidade,
                    t->dfa.inicial);
            if (t->ignorado) fputs("T_UNDEF, 1},\n", f);
            else {
                char nome[160];
                nome_enum(g, t->indice, nome);
                fprintf(f, "%s, 0},\n", nome);
            }
        }
    }
    fputs("};\n", f);
    fprintf(f, "#define QUANTIDADE_REGEX %d\n\n", quantidade_regex);
}

static void emitir_literais(Gramatica *g, FILE *f)
{
    int i;
    int quantidade;
    char nome[160];
    Terminal *t;
    quantidade = 0;
    fputs("static const DefLiteral definicoes_literais[] = {\n", f);
    for (i = 0; i < g->quantidade_terminais; i++) {
        t = &g->terminais[i];
        if (!t->literal_fixo) continue;
        nome_enum(g, t->indice, nome);
        fputs("    {", f);
        emitir_string_c(f, t->literal, strlen(t->literal));
        fprintf(f, ", %lu, %s},\n", (unsigned long)strlen(t->literal), nome);
        quantidade++;
    }
    if (quantidade == 0) fputs("    {\"\", 0, T_UNDEF}\n", f);
    fputs("};\n", f);
    fprintf(f, "#define QUANTIDADE_LITERAIS %d\n\n", quantidade);
}

static void emitir_nomes_tokens(Gramatica *g, FILE *f)
{
    int i;
    const char *nome;
    fputs("static const char *nomes_tokens[] = {\n"
          "    \"UNDEF\",\n", f);
    for (i = 0; i <= g->quantidade_tokens; i++) {
        nome = gramatica_nome_token(g, i);
        fputs("    ", f);
        emitir_string_c(f, nome, strlen(nome));
        fputs(",\n", f);
    }
    fputs("};\n\n", f);
}

static void atribuir_conjuntos(No *no, int *proximo)
{
    int i;
    if (no->tipo == NO_ALTERNATIVA) no->conjunto_esperado = (*proximo)++;
    for (i = 0; i < no->quantidade; i++) {
        atribuir_conjuntos(no->filhos[i], proximo);
    }
}

static void emitir_itens_conjunto(Gramatica *g, FILE *f, const Conjunto *a,
                                  const Conjunto *b)
{
    int i;
    char nome[160];
    for (i = 0; i <= g->quantidade_tokens; i++) {
        if (conjunto_contem(a, i) || (b != NULL && conjunto_contem(b, i))) {
            nome_enum(g, i, nome);
            fprintf(f, "%s, ", nome);
        }
    }
    fputs("-1", f);
}

static void emitir_conjuntos_no(Gramatica *g, FILE *f, No *no)
{
    int i;
    if (no->tipo == NO_ALTERNATIVA) {
        fprintf(f, "static const int esperado_%d[] = {",
                no->conjunto_esperado);
        emitir_itens_conjunto(g, f, &no->first,
                              no->anulavel ? &no->follow : NULL);
        fputs("};\n", f);
    }
    for (i = 0; i < no->quantidade; i++) {
        emitir_conjuntos_no(g, f, no->filhos[i]);
    }
}

static void emitir_condicao(Gramatica *g, FILE *f, const Conjunto *primeiro,
                            const Conjunto *segundo)
{
    int i;
    int escreveu;
    char nome[160];
    escreveu = 0;
    for (i = 0; i <= g->quantidade_tokens; i++) {
        if (conjunto_contem(primeiro, i) ||
            (segundo != NULL && conjunto_contem(segundo, i))) {
            if (escreveu) fputs(" || ", f);
            nome_enum(g, i, nome);
            fprintf(f, "tok.tipo == %s", nome);
            escreveu = 1;
        }
    }
    if (!escreveu) fputc('0', f);
}

static void recuar(FILE *f, int nivel)
{
    int i;
    for (i = 0; i < nivel; i++) fputs("    ", f);
}

static void emitir_expressao(Gramatica *g, FILE *f, No *no, int nivel)
{
    int i;
    char nome[160];
    if (no->tipo == NO_VAZIO) return;
    if (no->tipo == NO_SIMBOLO) {
        recuar(f, nivel);
        if (no->classe_simbolo == 1) {
            nome_enum(g, no->referencia, nome);
            fprintf(f, "casar(%s);\n", nome);
        } else {
            fprintf(f, "parse_%s();\n", g->regras[no->referencia].nome);
        }
        return;
    }
    if (no->tipo == NO_SEQUENCIA) {
        for (i = 0; i < no->quantidade; i++) {
            emitir_expressao(g, f, no->filhos[i], nivel);
        }
        return;
    }
    if (no->tipo == NO_OPCIONAL) {
        recuar(f, nivel);
        fputs("if (", f);
        emitir_condicao(g, f, &no->filhos[0]->first, NULL);
        fputs(") {\n", f);
        emitir_expressao(g, f, no->filhos[0], nivel + 1);
        recuar(f, nivel);
        fputs("}\n", f);
        return;
    }
    if (no->tipo == NO_REPETICAO) {
        recuar(f, nivel);
        fputs("while (", f);
        emitir_condicao(g, f, &no->filhos[0]->first, NULL);
        fputs(") {\n", f);
        emitir_expressao(g, f, no->filhos[0], nivel + 1);
        recuar(f, nivel);
        fputs("}\n", f);
        return;
    }
    if (no->tipo == NO_ALTERNATIVA) {
        for (i = 0; i < no->quantidade; i++) {
            recuar(f, nivel);
            if (i == 0) fputs("if (", f);
            else fputs("else if (", f);
            emitir_condicao(g, f, &no->filhos[i]->first,
                no->filhos[i]->anulavel ? &no->follow : NULL);
            fputs(") {\n", f);
            emitir_expressao(g, f, no->filhos[i], nivel + 1);
            recuar(f, nivel);
            fputs("}\n", f);
        }
        recuar(f, nivel);
        fprintf(f, "else erro_sintatico(esperado_%d);\n",
                no->conjunto_esperado);
    }
}

static void emitir_runtime_inicio(FILE *f, const char *cabecalho)
{
    fputs("/* Arquivo gerado automaticamente pelo gerador EBNF. */\n", f);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n"
          "#include <setjmp.h>\n", f);
    fputs("#include <ctype.h>\n#include <limits.h>\n#include <errno.h>\n", f);
    fprintf(f, "#include \"%s\"\n\n", cabecalho);
    fputs("#ifndef SCANNER_EXTERNO\n"
          "typedef struct EstadoDfa { int transicoes[256]; int final; "
          "} EstadoDfa;\n"
          "typedef struct DefRegex { const EstadoDfa *estados; int quantidade; "
          "int inicial; int token; int ignorado; } DefRegex;\n"
          "typedef struct DefLiteral { const char *texto; unsigned long tamanho; "
          "int token; } DefLiteral;\n"
          "#endif\n\n", f);
}

static void emitir_runtime_scanner(FILE *f)
{
    fputs(
"static char *fonte;\n"
"static unsigned long tamanho_fonte;\n"
"static unsigned long posicao_fonte;\n"
"static int linha_fonte = 1;\n"
"static int coluna_fonte = 1;\n"
"static char *lexema_atual;\n\n", f);
    fputs(
"static int prefixo_nfa(const DefRegex *def, const unsigned char *texto,\n"
"                       unsigned long tamanho)\n"
"{\n"
"    unsigned long pos;\n"
"    int estado;\n"
"    int melhor;\n"
"    estado = def->inicial;\n"
"    melhor = 0;\n", f);
    fputs(
"    for (pos = 0; pos < tamanho; pos++) {\n"
"        estado = def->estados[estado].transicoes[texto[pos]];\n"
"        if (estado < 0) break;\n"
"        if (def->estados[estado].final) melhor = (int)pos + 1;\n", f);
    fputs(
"    }\n"
"    return melhor;\n"
"}\n\n", f);
    fputs(
"static void consumir(unsigned long quantidade)\n"
"{\n"
"    unsigned long i;\n"
"    for (i = 0; i < quantidade; i++) {\n"
"        if (fonte[posicao_fonte] == '\\n') { linha_fonte++; coluna_fonte = 1; }\n"
"        else coluna_fonte++;\n"
"        posicao_fonte++;\n"
"    }\n"
"}\n\n", f);
    fputs(
"static void guardar_lexema(unsigned long inicio, unsigned long tamanho)\n"
"{\n"
"    free(lexema_atual);\n"
"    lexema_atual = (char *)malloc((size_t)tamanho + 1);\n"
"    if (lexema_atual == NULL) {\n"
"        fprintf(stderr, \"erro fatal: memoria insuficiente\\n\"); exit(2);\n"
"    }\n"
"    memcpy(lexema_atual, fonte + inicio, (size_t)tamanho);\n"
"    lexema_atual[tamanho] = '\\0';\n"
"}\n\n", f);
    fputs(
"static Token proximo_token(void)\n"
"{\n"
"    Token token;\n"
"    unsigned long inicio;\n"
"    unsigned long restante;\n"
"    unsigned long melhor;\n"
"    int tipo;\n"
"    int n;\n"
"    int i;\n"
"    do {\n"
"        melhor = 0;\n"
"        restante = tamanho_fonte - posicao_fonte;\n", f);
    fputs(
"        for (i = 0; i < QUANTIDADE_REGEX; i++) {\n"
"            if (!definicoes_regex[i].ignorado) continue;\n"
"            n = prefixo_nfa(&definicoes_regex[i],\n"
"                (const unsigned char *)fonte + posicao_fonte, restante);\n"
"            if ((unsigned long)n > melhor) melhor = (unsigned long)n;\n"
"        }\n"
"        if (melhor > 0) consumir(melhor);\n"
"    } while (melhor > 0);\n", f);
    fputs(
"    token.linha = linha_fonte; token.coluna = coluna_fonte;\n"
"    token.erro = NULL;\n"
"    if (posicao_fonte >= tamanho_fonte) {\n"
"        guardar_lexema(posicao_fonte, 0);\n"
"        token.tipo = T_END_OF_FILE; token.lexema = lexema_atual;\n"
"        token.tamanho = 0; return token;\n"
"    }\n"
"    inicio = posicao_fonte; restante = tamanho_fonte - inicio;\n"
"    melhor = 0; tipo = T_UNDEF;\n", f);
    fputs(
"    for (i = 0; i < QUANTIDADE_LITERAIS; i++) {\n"
"        if (definicoes_literais[i].tamanho > melhor &&\n"
"            definicoes_literais[i].tamanho <= restante &&\n"
"            memcmp(fonte + inicio, definicoes_literais[i].texto,\n"
"                   (size_t)definicoes_literais[i].tamanho) == 0) {\n"
"            melhor = definicoes_literais[i].tamanho;\n"
"            tipo = definicoes_literais[i].token;\n"
"        }\n", f);
    fputs(
"    }\n"
"    for (i = 0; i < QUANTIDADE_REGEX; i++) {\n"
"        if (definicoes_regex[i].ignorado) continue;\n"
"        n = prefixo_nfa(&definicoes_regex[i],\n"
"            (const unsigned char *)fonte + inicio, restante);\n"
"        if ((unsigned long)n > melhor) { melhor = (unsigned long)n;\n"
"            tipo = definicoes_regex[i].token; }\n"
"    }\n"
"    if (melhor == 0) { melhor = 1; tipo = T_UNDEF;\n", f);
    fputs(
"        token.erro = \"caractere nao reconhecido\"; }\n"
"    guardar_lexema(inicio, melhor); consumir(melhor);\n"
"    token.tipo = tipo; token.lexema = lexema_atual;\n"
"    token.tamanho = (int)melhor; return token;\n"
"}\n\n", f);
    fputs(
"static int carregar_fonte(const char *caminho)\n"
"{\n"
"    FILE *arquivo; long fim; size_t lidos;\n"
"    arquivo = fopen(caminho, \"rb\");\n"
"    if (arquivo == NULL) return 0;\n"
"    if (fseek(arquivo, 0L, SEEK_END) != 0 || (fim = ftell(arquivo)) < 0 ||\n"
"        fseek(arquivo, 0L, SEEK_SET) != 0) { fclose(arquivo); return 0; }\n", f);
    fputs(
"    fonte = (char *)malloc((size_t)fim + 1);\n"
"    if (fonte == NULL) { fclose(arquivo); return 0; }\n"
"    lidos = fread(fonte, 1, (size_t)fim, arquivo); fclose(arquivo);\n"
"    if (lidos != (size_t)fim) { free(fonte); fonte = NULL; return 0; }\n"
"    fonte[lidos] = '\\0'; tamanho_fonte = (unsigned long)lidos;\n"
"    posicao_fonte = 0; linha_fonte = 1; coluna_fonte = 1; return 1;\n"
"}\n\n", f);
    fputs(
"static void liberar_fonte(void)\n"
"{ free(fonte); fonte = NULL; free(lexema_atual); lexema_atual = NULL; }\n\n", f);
}

static void emitir_runtime_parser(FILE *f)
{
    fputs(
"static Token tok;\n"
"static jmp_buf ponto_erro;\n"
"static int exibir_arvore;\n"
"static int nivel_arvore;\n\n", f);
    fputs(
"static void indentacao(void)\n"
"{ int i; for (i = 0; i < nivel_arvore; i++) fputs(\"  \", stdout); }\n"
"static void entrar_regra(const char *nome)\n"
"{ if (exibir_arvore) { indentacao(); printf(\"%s\\n\", nome); nivel_arvore++; } }\n"
"static void sair_regra(void)\n"
"{ if (exibir_arvore) nivel_arvore--; }\n", f);
    fputs(
"static void imprimir_lexema(const char *s, int n)\n"
"{ int i; unsigned char c; for (i = 0; i < n; i++) { c = (unsigned char)s[i];\n"
"  if (c == '\\n') fputs(\"\\\\n\", stdout); else if (c == '\\t') fputs(\"\\\\t\", stdout);\n"
"  else if (c == '\\\\' || c == '\\'') { putchar('\\\\'); putchar(c); }\n"
"  else if (c >= 32 && c < 127) putchar(c); else printf(\"\\\\x%02X\", c); } }\n"
"static void erro_lexico(void)\n", f);
    fputs(
"{ fprintf(stderr, \"ERRO LEXICO (%d:%d): %s: '\", tok.linha, tok.coluna,\n"
"          tok.erro != NULL ? tok.erro : \"token indefinido\");\n"
"  fwrite(tok.lexema, 1, (size_t)tok.tamanho, stderr); fputs(\"'\\n\", stderr);\n"
"  longjmp(ponto_erro, 1); }\n"
"static void avancar(void)\n"
"{ tok = arquivo_tokens ? proximo_token_lista() : proximo_token();\n"
"  if (tok.tipo == T_UNDEF) erro_lexico(); }\n", f);
    fputs(
"static void erro_sintatico(const int *esperados)\n"
"{ int i; fprintf(stderr, \"ERRO SINTATICO (%d:%d): encontrado %s, esperado {\",\n"
"      tok.linha, tok.coluna, nomes_tokens[tok.tipo]);\n"
"  for (i = 0; esperados[i] >= 0; i++) { if (i) fputs(\", \", stderr);\n"
"      fputs(nomes_tokens[esperados[i]], stderr); }\n"
"  fputs(\"}\\n\", stderr); longjmp(ponto_erro, 1); }\n", f);
    fputs(
"static void erro_token(int esperado)\n"
"{ int lista[2]; lista[0] = esperado; lista[1] = -1; erro_sintatico(lista); }\n"
"static void casar(int esperado)\n"
"{ if (tok.tipo != esperado) erro_token(esperado);\n"
"  if (exibir_arvore && esperado != T_END_OF_FILE) { indentacao();\n"
"      printf(\"%s '\", nomes_tokens[esperado]);\n"
"      imprimir_lexema(tok.lexema, tok.tamanho); fputs(\"'\\n\", stdout); }\n"
"  avancar(); }\n\n", f);
}

static void comentar_no(Gramatica *g, FILE *f, No *no)
{
    int i;
    Terminal *t;
    const unsigned char *p;
    if (no->tipo == NO_SIMBOLO) {
        t = no->classe_simbolo == 1
            ? terminal_por_indice(g, no->referencia) : NULL;
        if (t != NULL && t->literal_fixo) {
            fputc('"', f);
            for (p = (const unsigned char *)t->literal; *p; p++) {
                if (*p == '/' || *p == '"' || *p == '\\') {
                    fputc('\\', f);
                    fputc(*p, f);
                } else if (*p == '\n') fputs("\\n", f);
                else if (*p == '\r') fputs("\\r", f);
                else if (*p == '\t') fputs("\\t", f);
                else fputc(*p, f);
            }
            fputc('"', f);
        } else fputs(no->texto, f);
        return;
    }
    if (no->tipo == NO_OPCIONAL) fputs("[ ", f);
    if (no->tipo == NO_REPETICAO) fputs("{ ", f);
    if (no->tipo == NO_ALTERNATIVA) fputs("( ", f);
    for (i = 0; i < no->quantidade; i++) {
        if (i) fputs(no->tipo == NO_ALTERNATIVA ? " | " : " ", f);
        comentar_no(g, f, no->filhos[i]);
    }
    if (no->tipo == NO_OPCIONAL) fputs(" ]", f);
    if (no->tipo == NO_REPETICAO) fputs(" }", f);
    if (no->tipo == NO_ALTERNATIVA) fputs(" )", f);
}

static void emitir_procedimentos(Gramatica *g, FILE *f)
{
    int i;
    for (i = 0; i < g->quantidade_regras; i++) {
        if (!g->regras[i].alcancavel) continue;
        fprintf(f, "static void parse_%s(void);\n", g->regras[i].nome);
    }
    fputc('\n', f);
    for (i = 0; i < g->quantidade_regras; i++) {
        if (!g->regras[i].alcancavel) continue;
        fprintf(f, "/* %s = ", g->regras[i].nome);
        comentar_no(g, f, g->regras[i].expressao);
        fputs(" ; */\n", f);
        fprintf(f, "static void parse_%s(void)\n{\n", g->regras[i].nome);
        fprintf(f, "    entrar_regra(\"%s\");\n", g->regras[i].nome);
        emitir_expressao(g, f, g->regras[i].expressao, 1);
        fputs("    sair_regra();\n}\n\n", f);
    }
}

static void emitir_api(Gramatica *g, FILE *f)
{
    fputs(
"int analisar_arquivo(const char *caminho, int mostrar_arvore)\n"
"{\n"
"    int falhou;\n"
"    if (modo_tokens) arquivo_tokens = fopen(caminho, \"rb\");\n"
"    if ((modo_tokens && arquivo_tokens == NULL) ||\n"
"        (!modo_tokens && !carregar_fonte(caminho))) {\n"
"        fprintf(stderr, \"erro: nao foi possivel ler %s\\n\", caminho); return 2;\n"
"    }\n"
"    exibir_arvore = mostrar_arvore; nivel_arvore = 0;\n", f);
    fprintf(f,
"    if (setjmp(ponto_erro) == 0) {\n"
"        avancar(); parse_%s(); casar(T_END_OF_FILE); falhou = 0;\n"
"    } else falhou = 1;\n"
"    if (modo_tokens) fechar_lista(); else liberar_fonte();\n"
"    if (falhou) return 1;\n"
"    if (!mostrar_arvore) puts(\"entrada aceita\");\n"
"    return 0;\n"
"}\n\n", g->regras[0].nome);
    fputs(
"int listar_tokens(const char *caminho)\n"
"{\n"
"    Token t;\n"
"    if (!carregar_fonte(caminho)) {\n"
"        fprintf(stderr, \"erro: nao foi possivel ler %s\\n\", caminho); return 2;\n"
"    }\n"
"    do {\n"
"        t = proximo_token();\n", f);
    fputs(
"        printf(\"Token: tipo = %-16s lexema = '\", nomes_tokens[t.tipo]);\n"
"        imprimir_lexema(t.lexema, t.tamanho);\n"
"        printf(\"' linha = %d coluna = %d\\n\", t.linha, t.coluna);\n"
"        if (t.tipo == T_UNDEF) {\n"
"            fprintf(stderr, \"ERRO LEXICO (%d:%d): %s\\n\", t.linha, t.coluna,\n"
"                    t.erro); liberar_fonte(); return 1;\n", f);
    fputs(
"        }\n"
"    } while (t.tipo != T_END_OF_FILE);\n"
"    liberar_fonte(); return 0;\n"
"}\n\n", f);
    fputs(
"int main(int argc, char **argv)\n"
"{\n"
"    if (argc == 3 && strcmp(argv[1], \"--tokens\") == 0) {\n"
"        modo_tokens = 1; return analisar_arquivo(argv[2], 0); }\n"
"    if (argc == 3 && strcmp(argv[1], \"--dump-tokens\") == 0)\n"
"        return listar_tokens(argv[2]);\n", f);
    fputs(
"    if (argc == 3 && strcmp(argv[1], \"--tree\") == 0)\n"
"        return analisar_arquivo(argv[2], 1);\n"
"    if (argc == 2) return analisar_arquivo(argv[1], 0);\n"
"    fprintf(stderr, \"Uso: %s [--tokens|--dump-tokens|--tree] arquivo\\n\", argv[0]);\n"
"    return 2;\n"
"}\n", f);
}

static int gerar_implementacao(Gramatica *g, const char *caminho,
                               const char *cabecalho)
{
    FILE *f;
    int i;
    int proximo;
    f = fopen(caminho, "wb");
    if (f == NULL) return 0;
    proximo = 0;
    for (i = 0; i < g->quantidade_regras; i++) {
        atribuir_conjuntos(g->regras[i].expressao, &proximo);
    }
    emitir_runtime_inicio(f, cabecalho);
    fputs("#ifndef SCANNER_EXTERNO\n", f);
    emitir_nfas(g, f);
    emitir_literais(g, f);
    fputs("#endif\n\n", f);
    emitir_nomes_tokens(g, f);
    for (i = 0; i < g->quantidade_regras; i++) {
        if (!g->regras[i].alcancavel) continue;
        emitir_conjuntos_no(g, f, g->regras[i].expressao);
    }
    fputc('\n', f);
    fputs("#ifndef SCANNER_EXTERNO\n", f);
    emitir_runtime_scanner(f);
    fputs("#else\n"
          "Token proximo_token(void);\n"
          "int carregar_fonte(const char *caminho);\n"
          "void liberar_fonte(void);\n"
          "#endif\n\n", f);
    {
        static const char *linhas[] = {
#include "runtime_tokens.inc"
        };
        size_t k;
        for (k = 0; k < sizeof(linhas) / sizeof(linhas[0]); k++)
            fputs(linhas[k], f);
    }
    emitir_runtime_parser(f);
    emitir_procedimentos(g, f);
    emitir_api(g, f);
    fclose(f);
    return 1;
}

int gerar_codigo(Gramatica *g, const char *prefixo)
{
    char *arquivo_c;
    char *arquivo_h;
    const char *base_h;
    size_t tamanho;
    int sucesso;
    tamanho = strlen(prefixo);
    arquivo_c = (char *)xmalloc(tamanho + 3);
    arquivo_h = (char *)xmalloc(tamanho + 3);
    sprintf(arquivo_c, "%s.c", prefixo);
    sprintf(arquivo_h, "%s.h", prefixo);
    base_h = nome_base(arquivo_h);
    sucesso = gerar_cabecalho(g, arquivo_h);
    if (sucesso) sucesso = gerar_implementacao(g, arquivo_c, base_h);
    if (!sucesso) {
        fprintf(stderr, "erro: nao foi possivel gravar %s.c e %s.h\n",
                prefixo, prefixo);
    }
    free(arquivo_c);
    free(arquivo_h);
    return sucesso;
}
