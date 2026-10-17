# Roadmap

These are ideas, not shipped features.

1. Split the current single translation unit into lexer, parser, AST, runtime, and diagnostic modules without changing behavior.
2. Add block-local bindings and clearer shadowing rules, then improve REPL multiline input.
3. Add richer collection operations and explicit numeric conversion, with tests for Unicode and large numbers.
4. Add a formal grammar and a stable language conformance suite.
5. Explore bytecode execution and, later, a separate `kslc` native compiler. A compiled program will still need runtime support for dynamic values.

No imports or package manager are planned for the first version.
