#!/bin/sh
set -eu

CC=${CC:-gcc}
GERADOR=${GERADOR:-./gerador}
BUILD=tests_tp2/.build
CFLAGS="-Wall -Wextra -Werror -std=c89 -pedantic"

mkdir -p "$BUILD"

testar_valida()
{
    nome=$1
    gramatica=$2
    casos=$3
    prefixo="$BUILD/$nome"

    "$GERADOR" -o "$prefixo" "$gramatica"
    $CC $CFLAGS "$prefixo.c" -o "$prefixo"

    for entrada in "$casos"/aceito_*; do
        "$prefixo" "$entrada" >/dev/null
    done
    for entrada in "$casos"/rejeitado_*; do
        if "$prefixo" "$entrada" >/dev/null 2>&1; then
            echo "FALHA: $nome aceitou $entrada" >&2
            exit 1
        fi
    done
    echo "OK: $nome"
}

testar_invalida()
{
    nome=$1
    gramatica=$2
    if "$GERADOR" -o "$BUILD/$nome" "$gramatica" >/dev/null 2>&1; then
        echo "FALHA: gerador aceitou a gramatica invalida $gramatica" >&2
        exit 1
    fi
    echo "OK: $nome rejeitada"
}

testar_valida expressoes grammars/expressoes.ebnf tests_tp2/expressoes
testar_valida listas grammars/listas.ebnf tests_tp2/listas
testar_valida comandos grammars/comandos.ebnf tests_tp2/comandos
testar_valida microc grammars/microc.ebnf tests_tp2/microc
testar_invalida conflito_ll1 grammars/conflito_ll1.ebnf
testar_invalida erros_semanticos grammars/erros_semanticos.ebnf

python tests_tp2/regressao.py

echo "Todos os testes passaram."
