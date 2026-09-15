aluno: 202619040462
aluno: 202119050233
uso_ia: sim

# Trabalho de Compiladores I - Parte 1

# Trabalho Pratico 1: Analise Lexica para Micro C

Repositorio do Trabalho Pratico 1 da disciplina **Compiladores I (FACOM)**:
implementacao de um **analisador lexico (scanner)** para a linguagem
**Micro C**, utilizando a ferramenta **Flex**.

## Sobre o trabalho

O objetivo do trabalho e completar a especificacao lexica de Micro C a partir
do esqueleto fornecido em `microc.flex`, reconhecendo corretamente todos os
tokens da linguagem: palavras reservadas, identificadores, constantes,
operadores e delimitadores. O scanner tambem identifica e reporta erros
lexicos.

Para cada token reconhecido na entrada, o scanner imprime seu tipo, o lexema
correspondente e a linha em que foi encontrado:

```text
Token: tipo = INT           lexema = 'int'  linha = 1
Token: tipo = ID            lexema = 'x'    linha = 1
Token: tipo = ASSIGN        lexema = '='    linha = 1
Token: tipo = INTEGERCONST  lexema = '10'   linha = 1
Token: tipo = SEMICOLON     lexema = ';'    linha = 1
```

Erros lexicos sao impressos na saida de erro padrao (`stderr`) no formato:

```text
ERRO LEXICO (linha N): <mensagem de erro>
```

O enunciado completo do trabalho esta disponivel no repositorio original:
[`amaury-junior/comp1-2026`](https://github.com/amaury-junior/comp1-2026).

## Estrutura do repositorio

```text
entrega_analisador_lexico/
|-- microc.flex       # Especificacao lexica do scanner
|-- tests/            # Programas de teste em Micro C
|-- Makefile          # Regras para compilar, testar e limpar a entrega
|-- leiame.txt        # Identificacao dos alunos por matricula
`-- README.md         # Este arquivo
```

## Itens implementados

- [x] Reconhecimento das palavras reservadas: `main`, `if`, `else`, `for`,
      `return`, `int`, `char`, `print`
- [x] Reconhecimento de identificadores (`ID`)
- [x] Reconhecimento de constantes inteiras (`INTEGERCONST`), incluindo
      constantes negativas quando o sinal de menos faz parte do numero
- [x] Diferenciacao entre constante inteira negativa e operador de subtracao
- [x] Reconhecimento de constantes de caractere (`CHARCONST`)
- [x] Reconhecimento de constantes de string (`STRINGCONST`)
- [x] Conversao de sequencias de escape: `\n`, `\t`, `\\`, `\"`, `\'`, `\0`
- [x] Tabela hash de strings para reutilizar lexemas iguais, com comparacao por
      tamanho e bytes para preservar valores que contenham `\0`
- [x] Reconhecimento de operadores aritmeticos, relacionais e logicos
- [x] Reconhecimento de delimitadores: `;`, `,`, `(`, `)`, `{`, `}`, `[`, `]`
- [x] Tratamento de comentarios de linha (`//`) e de bloco (`/* ... */`)
- [x] Tratamento de erros lexicos, incluindo caractere invalido, comentario
      nao terminado, string nao terminada, caractere invalido e escape invalido
- [x] Ampliacao dos arquivos de teste em `tests/`
- [x] Especificacao lexica completa, com regra final de erro para entradas
      nao reconhecidas
- [x] Remocao de instrucoes de depuracao antes da entrega

## Como compilar

No diretorio do projeto, execute:

```bash
make lexer
```

Ou execute manualmente:

```bash
flex microc.flex
gcc lex.yy.c -o lexer
```

O primeiro comando gera o arquivo `lex.yy.c` a partir das regras definidas em
`microc.flex`. O segundo compila esse codigo gerado, produzindo o executavel
`lexer`.

Em ambientes Windows nos quais o Flex nao esteja no `PATH`, e possivel informar
o caminho do executavel ao Makefile:

```bash
mingw32-make test FLEX=C:/msys64/usr/bin/flex.exe
```

## Como executar

```bash
./lexer tests/test.mc
```

O programa le o arquivo Micro C informado, imprime os tokens reconhecidos e
reporta eventuais erros lexicos.

## Como testar

Para executar todos os testes incluidos:

```bash
make test
```

Arquivos de teste:

- `tests/test.mc`
- `tests/palavras_reservadas.mc`
- `tests/literais.mc`
- `tests/operadores.mc`
- `tests/erros.mc`
- `tests/recuperacao.mc`

## Autores

| Nome | Matricula |
|------|-----------|
| Felipe Sanches Borges Gomes | 202619040462 |
| Marcus Vinicius Bello | 202119050233 |

## Disciplina

- **Curso:** Compiladores I
- **Instituicao:** FACOM
- **Trabalho:** TP1 - Analise Lexica para a linguagem Micro C

A submissao deve seguir as instrucoes especificas informadas pelo professor no
AVA (Moodle), dentro do prazo estabelecido no cronograma da disciplina.
