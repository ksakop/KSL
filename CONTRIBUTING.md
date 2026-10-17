# Contributing

KSL v0.1 is intentionally small. Please open an issue describing a language change before implementing new syntax; include an example program, expected result, and likely error cases.

Build with CMake, run `ctest --test-dir build --output-on-failure`, and add a case to `tests/run.py` for behavior changes. The tests execute real `.ksl` files against the CLI. Keep diagnostics actionable and update `docs/language.md` when behavior changes. C++17 and Python 3 (for tests) are the only development dependencies.

The AST and runtime currently live in `src/main.cpp`. Follow the existing lexer → parser → AST → interpreter path; avoid adding native compilation or modules as incidental work. Bugs and focused refactors are welcome.
