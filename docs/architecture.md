# Interpreter architecture

`src/main.cpp` contains the v0.1 implementation. It is organized into four stages:

1. `Lexer` turns source characters into tokens with line and column positions. It recognizes keywords, numeric and string literals, comments, and punctuation.
2. `Parser` turns tokens into owned expression and statement trees. A precedence-climbing expression parser determines grouping; recursive statement parsing handles nested blocks. The tree is an AST: a structural recipe for the program, not machine code.
3. `Interpreter` walks the AST. An environment maps names to runtime values and links to its parent environment. Functions retain their defining environment and a pointer to their AST declaration. The AST lives for the full file execution.
4. `report` renders exceptions with their saved source locations. The CLI loads a file, parses it completely, then executes it; syntax errors never run a partially parsed file.

`Value` is a `std::variant` of null, 32-bit integer (`num`), 64-bit integer (`bnum`), double (`dec`), string (`word`), byte character (`let`), bool, shared array, and shared function. The variant's active alternative provides dynamic typing. Integer operations are checked for 64-bit overflow, while mixed decimal arithmetic uses double. Arrays use `std::shared_ptr` to preserve reference semantics. Indexing validates type, integer value, sign, and bounds before accessing a vector. Control flow uses internal return/break/continue signals caught by the appropriate function or loop.

`ksl --check file.ksl` runs the lexer and parser without executing the AST. The `kslc.py` packaging command checks source this way, copies the already-built interpreter, and appends the program, its name, and a fixed trailer with lengths. At startup, the executable reads that trailer and runs the appended source through the same parser and interpreter. `studio.py` provides a graphical editor and calls this packaging routine. Packaging produces a standalone executable for the build platform, but does not translate KSL expressions into machine instructions.

Future compiler work can reuse tokenization and parsing, then lower the AST to an intermediate representation and generate code with a runtime for strings, arrays, dynamic values, errors, and memory management. Native compilation is not part of v0.1.
