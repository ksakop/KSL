#!/usr/bin/env python3
"""End-to-end KSL language tests; requires only Python's standard library."""
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
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
]
errors = [
    ("negative_index", "a = [1, 2]\nprint(a[-1])\n", ["ArrayIndexOutOfBounds", "attempted index: -1", "valid range: 0..1", "negative_index.ksl:2:9"]),
    ("high_index", "a = [1, 2]\nprint(a[2])\n", ["ArrayIndexOutOfBounds", "attempted index: 2", "valid range: 0..1"]),
    ("empty_index", "print([][0])\n", ["ArrayIndexOutOfBounds", "empty array"]),
    ("fractional_index", "print([1][0.5])\n", ["TypeError", "integer"]),
    ("undefined", "print(missing)\n", ["UndefinedVariable", "missing", "undefined.ksl:1"]),
    ("division", "print(1 / 0)\n", ["DivisionByZero", "division.ksl:1"]),
    ("type", "print(\"hi\" - 2)\n", ["TypeError", "number"]),
    ("syntax", "if <true> [\nprint(1)\n", ["SyntaxError", "expected ']'", "syntax.ksl:3"]),
    ("invalid_char", "print(1);\n", ["SyntaxError", "unexpected character ';'"]),
    ("break_outside", "break\n", ["ControlFlowError", "outside a loop"]),
    ("return_outside", "return 1\n", ["ControlFlowError", "outside a function"]),
    ("arity", "fn f(x) [return x]\nf()\n", ["ArityError", "expects 1 arguments"]),
    ("huge_number", "print(" + "9" * 400 + ")\n", ["NumberOutOfRange", "numeric literal"]),
    ("malformed_step", "whileif <true> <i++ [print(1)]\n", ["SyntaxError"]),
    ("index_assign_bounds", "a = [1]\na[-1] = 2\n", ["ArrayIndexOutOfBounds", "attempted index: -1"]),
    ("invalid_assignment", "print(99)\n(1 + 2) = 3\n", ["SyntaxError", "assignment target"]),
    ("invalid_increment", "(1 + 2)++\n", ["SyntaxError", "require a variable"]),
    ("duplicate_parameter", "fn f(x, x) [return x]\n", ["SyntaxError", "duplicate parameter 'x'"]),
]

with tempfile.TemporaryDirectory() as tmp:
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
print(f"{len(cases) + len(errors) + 2} language tests passed")
