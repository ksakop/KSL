#!/usr/bin/env python3
"""End-to-end KSL language tests; requires only Python's standard library."""
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
examples = {
    "arrays.ksl": "10\n[10, 25, 30]\n",
    "control.ksl": "13\n",
    "functions.ksl": "120\n",
    "hello.ksl": "Hello from KSL\n",
    "values.ksl": "word let num bnum dec\nforty-two\n",
    "whileif.ksl": "0\n1\n2\n",
}
cases = [
    ("precedence", "print(2 + 3 * 4)\nprint((2 + 3) * 4)\nprint(not false and true or false)\n", "14\n20\ntrue\n"),
    ("comparisons", "x = 7\nif <x > 5> [ print(1) ]\nif <x < 8> [ print(2) ]\nif <x >= 7> [ print(3) ]\nif <x <= 7> [ print(4) ]\n", "1\n2\n3\n4\n"),
    ("nested", "x = 2\nif <x > 1> [\n if <x < 3> [ print(9) ]\n else [ print(0) ]\n]\n", "9\n"),
    ("whileif", "i = 0\nwhileif <i < 3>\n<i++> [print(i)]\n", "0\n1\n2\n"),
    ("steps", "i = 6\nwhileif <i > 0>\n<i -= 2> [print(i)]\ni--\nprint(i)\ni++\nprint(i)\n", "6\n4\n2\n-1\n0\n"),
    ("continue", "i = 0\nwhileif <i < 5>\n<i++> [\n if <i == 1> [continue]\n if <i == 4> [break]\n print(i)\n]\n", "0\n2\n3\n"),
    ("while_loop", "i = 0\nwhile <i < 2> [print(i)\ni++]\nloop [print(i)\nbreak]\n", "0\n1\n2\n"),
    ("arrays", "a = [10, 20, 30]\nprint(a[0], a[2], len(a))\na[1] *= 2\nprint(a)\n", "10 30 3\n[10, 40, 30]\n"),
    ("dynamic", "x = 5\nx = \"hey\"\nprint(x + \"!\")\nx = null\nprint(x == null)\n", "hey!\ntrue\n"),
    ("functions", "fn add(a, b) [return a + b]\nprint(add(2, 3))\nfn fib(n) [if <n <= 1> [return n]\nreturn fib(n - 1) + fib(n - 2)]\nprint(fib(7))\n", "5\n13\n"),
    ("logical_short_circuit", "print(false and missing)\nprint(true or missing)\n", "false\ntrue\n"),
    ("string_escapes", "print(\"a\\nb\")\nprint(len(\"ok\"))\n", "a\nb\n2\n"),
    ("assignment_order", "a = b = 3\nprint(a, b)\na += 2 * 4\nprint(a)\n", "3 3\n11\n"),
    ("nested_arrays", "a = [[1, 2], [3]]\nprint(a[0][1])\nb = a\nb[1][0] = 9\nprint(a[1][0])\n", "2\n9\n"),
    ("cycle_display", "a = [null]\na[0] = a\nprint(a)\n", "[[<cycle>]]\n"),
    ("return_from_loop", "fn f() [loop [return 9]]\nprint(f())\n", "9\n"),
    ("nested_continue", "i = 0\nwhileif <i < 3> <i++> [\n j = 0\n while <j < 2> [j++\n  continue]\n print(i)\n]\n", "0\n1\n2\n"),
    ("value_names", "print(type(4), type(2147483648), type(3.5), type(\"KSL\"), type('K'))\nprint(type(true), type(null), type([]))\n", "num bnum dec word let\nbool null array\n"),
    ("integer_promotion", "x = 2147483647\nx++\nprint(x, type(x))\nprint(9007199254740993 + 1)\nprint(type(6 / 2))\n", "2147483648 bnum\n9007199254740994\ndec\n"),
    ("character", "c = 'a'\nprint(c + 'b', type(c), 'a' == 'a', 'a' != 'b')\nprint(type('\\n'))\n", "ab let true true\nlet\n"),
    ("integer_min", "x = -9223372036854775808\nprint(x, type(x))\n", "-9223372036854775808 bnum\n"),
    ("mixed_comparison", "a = 9007199254740993\nb = 9007199254740992.0\nprint(a == b, a > b, b < a)\nprint(3 == 3.0, 3 < 3.5)\n", "false true true\ntrue true\n"),
    ("multiline_collections", "a = [\n 10,\n 20,\n]\nfn add(\n x,\n y,\n) [return x + y]\nprint(\n a[\n 0\n ],\n add(\n 2,\n 3,\n ),\n)\n", "10 5\n"),
]
errors = [
    ("negative_index", "a = [1, 2]\nprint(a[-1])\n", ["ArrayIndexOutOfBounds", "attempted index: -1", "valid range: 0..1", "negative_index.ksl:2:9"]),
    ("high_index", "a = [1, 2]\nprint(a[2])\n", ["ArrayIndexOutOfBounds", "attempted index: 2", "valid range: 0..1"]),
    ("empty_index", "print([][0])\n", ["ArrayIndexOutOfBounds", "empty array"]),
    ("fractional_index", "print([1][0.5])\n", ["TypeError", "integer"]),
    ("undefined", "print(missing)\n", ["UndefinedVariable", "missing", "undefined.ksl:1"]),
    ("division", "print(1 / 0)\n", ["DivisionByZero", "division.ksl:1"]),
    ("type", "print(\"hi\" - 2)\n", ["TypeError", "expected a number, got word"]),
    ("index_type", "print([1][\"zero\"])\n", ["TypeError", "integer number, got word"]),
    ("variable_suggestion", "score = 5\nprint(socre)\n", ["UndefinedVariable", "did you mean 'score'?"]),
    ("parameter_suggestion", "fn greet(person) [return perosn]\nprint(greet(1))\n", ["UndefinedVariable", "did you mean 'person'?"]),
    ("syntax", "if <true> [\nprint(1)\n", ["SyntaxError", "expected ']'", "syntax.ksl:3"]),
    ("invalid_char", "print(1);\n", ["SyntaxError", "unexpected character ';'"]),
    ("break_outside", "break\n", ["ControlFlowError", "outside a loop"]),
    ("return_outside", "return 1\n", ["ControlFlowError", "outside a function"]),
    ("arity", "fn f(x) [return x]\nf()\n", ["ArityError", "expects 1 arguments"]),
    ("huge_number", "print(" + "9" * 400 + ")\n", ["NumberOutOfRange", "numeric literal"]),
    ("decimal_overflow", "x = " + "9" * 200 + ".0\nprint(x * x)\n", ["NumberOutOfRange", "non-finite result"]),
    ("decimal_literal_range", "print(" + "9" * 400 + ".0)\n", ["NumberOutOfRange", "numeric literal"]),
    ("malformed_step", "whileif <true> <i++ [print(1)]\n", ["SyntaxError"]),
    ("index_assign_bounds", "a = [1]\na[-1] = 2\n", ["ArrayIndexOutOfBounds", "attempted index: -1"]),
    ("invalid_assignment", "print(99)\n(1 + 2) = 3\n", ["SyntaxError", "assignment target"]),
    ("invalid_increment", "(1 + 2)++\n", ["SyntaxError", "require a variable"]),
    ("duplicate_parameter", "fn f(x, x) [return x]\n", ["SyntaxError", "duplicate parameter 'x'"]),
    ("integer_overflow", "print(9223372036854775807 + 1)\n", ["NumberOutOfRange", "bnum range"]),
    ("multiply_overflow", "print(3037000500 * 3037000500)\n", ["NumberOutOfRange", "bnum range"]),
    ("character_size", "print('ab')\n", ["SyntaxError", "let literal must contain exactly one byte"]),
    ("recursive_depth", "fn forever() [return forever()]\nforever()\n", ["CallDepthExceeded", "depth exceeded 256", "recursive_depth.ksl:1"]),
    ("array_unclosed", "a = [1,\n", ["SyntaxError", "expected ']' after array elements"]),
    ("call_unclosed", "print(1,\n", ["SyntaxError", "expected ')' after arguments"]),
    ("nul_byte", "print(1)\n\x00print(2)\n", ["SyntaxError", "unexpected character", "nul_byte.ksl:2:1"]),
    ("compound_separator", "if <true> [print(1)] print(2)\n", ["SyntaxError", "expected a newline after statement"]),
    ("function_separator", "fn f() [return 1] print(f())\n", ["SyntaxError", "expected a newline after statement"]),
]

with tempfile.TemporaryDirectory() as tmp:
    example_dir = pathlib.Path(__file__).resolve().parent.parent / "examples"
    actual_examples = {path.name for path in example_dir.glob("*.ksl")}
    if actual_examples != set(examples):
        raise AssertionError(f"example expectations differ from repository: {actual_examples ^ set(examples)}")
    for name, expected in examples.items():
        result = subprocess.run([str(binary), str(example_dir / name)], capture_output=True, text=True, timeout=3)
        if result.returncode != 0 or result.stdout != expected:
            raise AssertionError(f"example {name}: status={result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}")
    for name, source, expected in cases:
        path = pathlib.Path(tmp, name + ".ksl")
        path.write_text(source, encoding="utf-8")
        result = subprocess.run([str(binary), str(path)], capture_output=True, text=True, timeout=3)
        if result.returncode != 0 or result.stdout != expected:
            raise AssertionError(f"{name}: status={result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}")
    for name, source, expected_parts in errors:
        path = pathlib.Path(tmp, name + ".ksl")
        path.write_text(source, encoding="utf-8")
        result = subprocess.run([str(binary), str(path)], capture_output=True, text=True, timeout=3)
        if result.returncode == 0 or any(part not in result.stderr for part in expected_parts):
            raise AssertionError(f"{name}: status={result.returncode}, stderr={result.stderr!r}")
        if name == "invalid_assignment" and result.stdout:
            raise AssertionError("syntax error executed a partial program")
    repl = subprocess.run([str(binary)], input="fn f(x) [return x * 2]\nprint(f(4))\n",
                          capture_output=True, text=True, timeout=3)
    if repl.returncode != 0 or "> 8\n" not in repl.stdout:
        raise AssertionError(f"repl: {repl.stdout!r}, {repl.stderr!r}")
    help_result = subprocess.run([str(binary), "--help"], capture_output=True, text=True, timeout=3)
    if help_result.returncode != 0 or "Usage: ksl" not in help_result.stdout:
        raise AssertionError(f"help: {help_result.stdout!r}, {help_result.stderr!r}")
print(f"{len(examples) + len(cases) + len(errors) + 2} language tests passed")
