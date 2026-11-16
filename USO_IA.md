## Uso 1

Ferramenta: ChatGPT/Codex (GPT-5)

Trecho ou arquivo afetado: regra de tratamento de EOF dentro de comentario de bloco em microc.flex.

Finalidade do uso: esclarecer como usar start conditions do Flex para detectar o fim do arquivo enquanto o scanner esta dentro de um comentario iniciado por /*.

O que fiz com a resposta: adaptei a explicacao ao formato exigido pelo enunciado, retornando UNDEF com a mensagem "EOF em comentario".

## Uso 2

Ferramenta: ChatGPT/Codex (GPT-5)

Trecho ou arquivo afetado: README.md.

Finalidade do uso: adaptar o README ao modelo do repositorio do trabalho, evitando escrever manualmente toda a estrutura do documento.

O que fiz com a resposta: revisei o texto gerado, preenchi os dados da dupla e mantive as informacoes coerentes com a entrega final.

## Uso 3

Ferramenta: ChatGPT/Codex.

Trechos ou arquivos afetados: modulos em `src/`, gramaticas, testes, Makefile e
README.md do TP2, na branch `tp2-gerador-ebnf`.

Finalidade do uso: interpretar os requisitos do enunciado atualizado, revisar
a compatibilidade ANSI C89/C90, elaborar testes de integracao e diagnosticar
erros de compilacao e de analise das entradas.

Resultado: foram executadas verificacoes de geracao e compilacao dos parsers,
com avisos tratados como erros, e testes com entradas aceitas e rejeitadas.

## Uso 4

Ferramenta: ChatGPT/Codex.

Arquivos afetados: modulos do gerador, adaptador Flex, leitor de listagens do
TP1, testes de regressao e documentacao.

Finalidade do uso: revalidar o trabalho em relacao ao enunciado e investigar
casos nao cobertos pela suite inicial, incluindo erros lexicos, nomes longos,
colisoes de identificadores, regras inalcançaveis e a opcao `--tokens`.

Resultado: a revisao identificou falhas reproduziveis. A ferramenta aplicou
correcoes e acrescentou testes de regressao. A suite original e 76 verificacoes
de regressao passaram, incluindo compilacao C89 estrita e integracao com a
saida real do scanner original do TP1.
