# Roadmap

These are ideas, not shipped features.

1. Split the current single translation unit into lexer, parser, AST, runtime, and diagnostic modules without changing behavior.
2. Add block-local bindings and clearer shadowing rules; refine REPL recovery for more multiline expressions.
3. Add richer collection operations and explicit numeric conversion, with tests for Unicode and large numbers.
4. Add a formal grammar and a stable language conformance suite.
5. Improve KSL Studio with syntax highlighting and interactive input; explore bytecode execution and, later, a native KSL compiler. The current `kslc.py` packages source with the interpreter into an executable; native translation is still planned. Compiled code will still need runtime support for dynamic values.

No imports or package manager are planned for the first version.
