# Syntax notes

The recognizable KSL form is `if <condition> [ body ]` and `whileif <condition> <step> [ body ]`. The parser reads a normal expression after the opening `<`, then consumes the outer closing `>`. An inner `>` is parsed as comparison only when it has a right operand. `if <x > 5> [` is therefore unambiguous in this grammar. An outer `>` immediately before `[` always closes the header.

Whitespace and newlines may occur between a header and its block. A statement ends at a newline or the enclosing `]`. Several statements on one line without a block boundary are rejected. Comments use `//`.

Examples of deliberate errors: `a[-1]` gives an array bounds error, `a[1.5]` gives a type error, and `if <true> { ... }` gives a syntax error. A diagnostic includes category, file, line, column, source excerpt, and caret. An undefined name may suggest a similarly spelled visible variable.

Known grammar boundary: the REPL parses one entered line at a time, so write multiline definitions and blocks in `.ksl` files. There is no escape sequence for arbitrary Unicode code points, no exponent notation, and no block comment yet.
