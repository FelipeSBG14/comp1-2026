# Trabalho de Compiladores I - Parte 2

Gerador, escrito em ANSI C89/C90, de analisadores sintáticos descendentes
recursivos a partir de gramáticas EBNF. O projeto dá continuidade ao scanner de
Micro C do TP1.

## Funcionalidades

- leitura das seções `TOKENS` e `RULES` da notação EBNF;
- árvore interna para alternativas, sequências, opcionais e repetições;
- detecção de símbolos indefinidos ou duplicados;
- avisos para não terminais inalcançáveis;
- detecção de improdutividade e recursão à esquerda direta ou indireta,
  inclusive através de prefixos anuláveis;
- cálculo iterativo de anulável, FIRST e FOLLOW para regras e subexpressões;
- verificação LL(1), com diagnóstico das alternativas conflitantes;
- construção de AFN por Thompson, conversão para AFD pelo algoritmo dos
  subconjuntos e minimização por refinamento de partições;
- geração de `parser.c` e `parser.h` compatíveis com C89;
- scanner gerado com maior prefixo e prioridade de literais;
- mensagens léxicas e sintáticas com linha, coluna e tokens esperados;
- leitura da listagem do TP1 com `--tokens`, listagem do código-fonte com
  `--dump-tokens` e árvore de derivação com `--tree`;
- integração opcional com o scanner Flex de Micro C adaptado do TP1.

## Estrutura

```text
src/
  main.c               fluxo das cinco etapas
  ebnf_lexer.c         scanner da notação EBNF
  ebnf_parser.c        parser EBNF e construção da árvore
  verificacao.c        verificações semânticas
  conjuntos.c          anulável, FIRST e FOLLOW
  ll1.c                verificação LL(1)
  regex.c              Thompson, subconjuntos e minimização
  codegen.c            geração do scanner e parser C
  microc_scanner.flex  scanner do TP1 adaptado para fornecer tokens
grammars/               quatro gramáticas válidas e duas inválidas
tests_tp2/              programas aceitos e rejeitados
```

## Compilação

```sh
make
```

Sem `make`, use:

```sh
gcc -Wall -Wextra -Werror -std=c89 -pedantic \
  src/main.c src/apoio.c src/ebnf_lexer.c src/ebnf_parser.c \
  src/verificacao.c src/conjuntos.c src/ll1.c src/regex.c src/codegen.c \
  -o gerador
```

## Uso do gerador

```sh
./gerador [--debug] [--sets] [-o prefixo] gramatica.ebnf
```

Exemplo:

```sh
./gerador --sets -o parser_expr grammars/expressoes.ebnf
gcc -Wall -Wextra -Werror -std=c89 -pedantic parser_expr.c -o parser_expr
./parser_expr tests_tp2/expressoes/aceito_1.txt
```

O analisador gerado aceita:

```sh
./parser_expr arquivo.txt
./parser_expr --dump-tokens arquivo.txt
./parser_expr --tokens listagem.tokens
./parser_expr --tree arquivo.txt
```

`--tokens` analisa uma listagem no formato do TP1:
`Token: tipo = ID lexema = 'x' linha = 1`. A coluna é opcional (assume 1),
e o EOF pode ser implícito ou um registro `END_OF_FILE` (também aceita `$`).
Registros malformados, tipos desconhecidos e tokens `UNDEF` produzem erro.
Os escapes `\n`, `\t`, `\r`, `\0`, `\\`, `\'`, `\"` e
`\xHH` são decodificados sem descartar bytes NUL do lexema.

Os identificadores internos dos tokens são numéricos, evitando colisões com
nomes da gramática. Terminais nomeados possuem aliases `T_NOME` para integração,
exceto `UNDEF` e `END_OF_FILE`, cujos aliases são reservados ao protocolo.
Regras inalcançáveis recebem aviso e não são emitidas no C gerado.

## Micro C e scanner do TP1

O modo padrão gera um scanner próprio a partir de `grammars/microc.ebnf`:

```sh
make microc-parser
```

Para usar o scanner Flex adaptado do TP1 como fornecedor de tokens:

```sh
make microc-parser-tp1
```

Essa segunda forma compila `parser_microc.c` com `SCANNER_EXTERNO` e liga o
resultado de `src/microc_scanner.flex`. O scanner permanece silencioso,
devolve um token por chamada e propaga `UNDEF` com a mensagem léxica.

## Testes

Requer GCC, Flex, shell POSIX e Python 3 (biblioteca padrão).

```sh
make test
```

A suíte cobre quatro gramáticas válidas (incluindo Micro C), com três entradas
aceitas e três rejeitadas em cada grupo, além de gramáticas com conflito LL(1),
símbolo indefinido, improdutividade e recursão à esquerda.

`tests_tp2/regressao.py` cobre também as falhas encontradas na revalidação:
erros léxicos no scanner externo, nomes longos e colisões, regras inalcançáveis,
regex sem operando, nomes de terminais inválidos, comentários EBNF completos e
leitura de listagens do TP1. Compila os parsers com C89 e avisos como erros.
Os artefatos dos testes ficam em `tests_tp2/.build/`.

## Autores

| Nome | Matrícula |
|---|---|
| Felipe Sanches Borges Gomes | 202619040462 |
| Marcus Vinicius Bello | 202119050233 |
