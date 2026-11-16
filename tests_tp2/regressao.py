"""Regressoes do gerador e da integracao TP1. Python 3, sem dependencias."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "tests_tp2" / ".build"
FLAGS = ["-Wall", "-Wextra", "-Werror", "-std=c89", "-pedantic"]
CC = os.environ.get("CC", "gcc")
GEN = ROOT / ("gerador.exe" if os.name == "nt" else "gerador")
checks = 0


def run(args, expected=0, contains=None):
    global checks
    result = subprocess.run(
        [str(a) for a in args], cwd=ROOT, capture_output=True,
        encoding="utf-8", errors="replace", timeout=30,
    )
    assert result.returncode == expected, (
        f"{args}: esperado {expected}, recebido {result.returncode}\n"
        f"{result.stdout}\n{result.stderr}"
    )
    if contains:
        assert contains in result.stdout + result.stderr, result.stdout + result.stderr
    checks += 1
    return result


def main():
    BUILD.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="regressao-", dir=BUILD))

    def fixture(name, content):
        path = work / name
        path.write_text(content, encoding="utf-8")
        return path

    def generate(name, grammar, expected=0, contains=None):
        path = fixture(name + ".ebnf", grammar)
        prefix = work / name
        run([GEN, "-o", prefix, path], expected, contains)
        if expected:
            assert not prefix.with_suffix(".c").exists()
            assert not prefix.with_suffix(".h").exists()
        return prefix

    def compile_parser(prefix, *extra):
        exe = prefix.with_suffix(".exe")
        run([CC, *FLAGS, *extra, prefix.with_suffix(".c"), "-o", exe])
        return exe

    for name, grammar, sample in [
        ("inalcancavel", 'TOKENS RULES inicio = "a"; outra = "b" | "c";', "a"),
        ("colisao", 'TOKENS LIT_2 = /x/; RULES inicio = LIT_2 "a";', "xa"),
        ("reservados", 'TOKENS END_OF_FILE = /x/; UNDEF = /y/; RULES inicio = END_OF_FILE UNDEF;', "xy"),
        ("comentario", 'TOKENS RULES inicio = "*/";', "*/"),
        ("longo", "TOKENS " + "A" * 200 + " = /x/; RULES inicio = " + "A" * 200 + ";", "x"),
    ]:
        prefix = generate(name, grammar)
        exe = compile_parser(prefix)
        run([exe, fixture(name + ".txt", sample)], contains="entrada aceita")
        source = prefix.with_suffix(".c").read_text(encoding="utf-8")
        assert "/* inicio = " in source
        if name == "inalcancavel":
            assert "parse_outra" not in source
        if name == "colisao":
            assert '/* inicio = LIT_2 "a" ; */' in source

    for name, pattern in [("asterisco", "*"), ("mais", "+a"), ("opcional", "?")]:
        generate(name, f"TOKENS NUM = /{pattern}/; RULES inicio = NUM;",
                 1, "operador de repeticao sem operando")
    generate("minuscula", "TOKENS Numero = /[0-9]+/; RULES inicio = Numero;",
             1, "maiusculas")
    prefix = generate("escapado", r'TOKENS STAR = /\*/; RULES inicio = STAR;')
    run([compile_parser(prefix), fixture("estrela.txt", "*")])

    prefix = work / "parser_microc"
    run([GEN, "-o", prefix, ROOT / "grammars/microc.ebnf"])
    generic = compile_parser(prefix)
    flex = os.environ.get("FLEX") or shutil.which("flex")
    if not flex and Path("C:/msys64/usr/bin/flex.exe").exists():
        flex = "C:/msys64/usr/bin/flex.exe"
    assert flex, "Flex necessario para testar a integracao TP1; configure FLEX."
    scanner = work / "scanner.c"
    run([flex, "-o", scanner, ROOT / "src/microc_scanner.flex"])
    external = work / "microc_tp1.exe"
    run([CC, *FLAGS, "-DSCANNER_EXTERNO", prefix.with_suffix(".c"),
         scanner, "-o", external])

    for exe in [generic, external]:
        for path in sorted((ROOT / "tests_tp2/microc").glob("aceito_*")):
            run([exe, path], contains="entrada aceita")
        for path in sorted((ROOT / "tests_tp2/microc").glob("rejeitado_*")):
            run([exe, path], 1, "ERRO SINTATICO")
        run([exe, fixture("erro_final.mc", "int main() {} @")], 1, "ERRO LEXICO")
        dump = run([exe, "--dump-tokens", ROOT / "tests_tp2/microc/aceito_2.mc"])
        run([exe, "--tokens", fixture("dump.tokens", dump.stdout)], contains="entrada aceita")
        escaped = fixture("escape.mc", r'int main() { print("a\"b"); return 0; }')
        dump = run([exe, "--dump-tokens", escaped])
        run([exe, "--tokens", fixture("escape.tokens", dump.stdout)], contains="entrada aceita")

    for content, message in [
        (r'int main() { print("\q"); }', "Sequencia de escape invalida"),
        ("int main() {} /*", "EOF em comentario"),
    ]:
        run([external, fixture("erro.mc", content)], 1, message)

    # Formato real emitido pelo TP1, sem EOF explicito nem coluna.
    records = [
        ("INT", "int"), ("MAIN", "main"), ("LPAREN", "("),
        ("RPAREN", ")"), ("LBRACE", "{"), ("RBRACE", "}"),
    ]
    listing = "".join(
        f"Token: tipo = {kind:<13} lexema = '{lexeme}'  linha = 1\n"
        for kind, lexeme in records
    )
    for exe in [generic, external]:
        run([exe, "--tokens", fixture("tp1.tokens", listing)], contains="entrada aceita")
        eof = "Token: tipo = END_OF_FILE lexema = '' linha = 2\n"
        run([exe, "--tokens", fixture("eof.tokens", listing + eof)])
        run([exe, "--tokens", fixture("apos_eof.tokens", listing + eof + listing)],
            1, "dados apos END_OF_FILE")
        run([exe, "--tokens", fixture("sintaxe.tokens", listing.splitlines()[0] + "\n")],
            1, "ERRO SINTATICO")
        run([exe, "--tokens", fixture("erro.tokens", listing +
            "Token: tipo = UNDEF lexema = 'escape invalido' linha = 2\n")],
            1, "escape invalido")
        for malformed in [
            "lixo", "Token: tipo = INEXISTENTE lexema = '' linha = 1",
            "Token: tipo = INT lexema = 'int' linha = 0",
            "Token: tipo = INT lexema = 'int' linha = 999999999999999999999999",
            "Token: tipo = INT lexema = 'int linha = 1",
        ]:
            run([exe, "--tokens", fixture("registro.tokens", malformed)], 1, "ERRO LEXICO")

    # Lexema longo e escapes da listagem, inclusive NUL, sem limite fixo de linha.
    lexeme = "a" * 10000 + r"\n\t\\\'\0\x41"
    listing = listing.rsplit("Token:", 1)[0]
    listing += (
        "Token: tipo = PRINT lexema = 'print' linha = 2\n"
        "Token: tipo = LPAREN lexema = '(' linha = 2\n"
        f"Token: tipo = STRINGCONST lexema = '{lexeme}' linha = 2\n"
        "Token: tipo = RPAREN lexema = ')' linha = 2\n"
        "Token: tipo = SEMICOLON lexema = ';' linha = 2\n"
        "Token: tipo = RBRACE lexema = '}' linha = 3\n"
    )
    run([generic, "--tokens", fixture("longa.tokens", listing)])

    # Integracao com a saida verdadeira do programa entregue no TP1.
    original_c = work / "tp1.c"
    original_exe = work / "tp1.exe"
    run([flex, "-o", original_c, ROOT / "microc.flex"])
    run([CC, "-Wall", "-Wextra", "-Werror", "-std=c11", "-pedantic",
         original_c, "-o", original_exe])
    original = run([original_exe, ROOT / "tests_tp2/microc/aceito_2.mc"])
    original_list = fixture("original_tp1.tokens", original.stdout)
    run([generic, "--tokens", original_list], contains="entrada aceita")
    run([external, "--tokens", original_list], contains="entrada aceita")
    print(f"Regressoes: {checks} verificacoes passaram. Artefatos: {work}")


if __name__ == "__main__":
    main()
