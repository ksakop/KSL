# KSL v0.1 language reference

## Values and variables

`x = 5` creates or updates a variable. Values have types; variables and function parameters have no declared type. `x = "hello"` can follow `x = 5`. KSL names its value types:

| KSL | Example | Runtime representation |
| --- | --- | --- |
| `num` | `5` | 32-bit signed integer, like C++ `int` |
| `bnum` | `2147483648` | 64-bit signed integer, like C++ `long long` |
| `dec` | `3.5` | IEEE 754 double for fractional numbers |
| `word` | `"KSL"` | String |
| `let` | `'K'` | One-byte character, like C++ `char` |
| `bool` | `true` | Boolean |
| `null` | `null` | No value |
| `array` | `[1, 2]` | Mutable array |
| `function` | `fn f() [return 1]` | Callable value |

`type(value)` returns the type name as a `word`. Integer literals start as `num` when they fit and otherwise become `bnum`; arithmetic promotes a result to `bnum` when necessary. Exceeding the signed 64-bit range is an error. Division returns `dec`, even when the quotient is whole. Numeric literals support decimal digits and a decimal point; scientific notation is not yet supported. A missing variable is an error.

`dec` arithmetic uses IEEE 754 double precision. If an operation overflows to a non-finite result, KSL reports `NumberOutOfRange` instead of silently printing infinity. Precision can still be lost when mixing `bnum` and `dec` in arithmetic.

`word` literals use double quotes and support `\n`, `\t`, `\"`, `\\`. `let` literals use single quotes, hold exactly one byte, and support escapes such as `'\n'` and `'\''`. Unicode letters occupying multiple UTF-8 bytes must currently be written as `word` values. `+` adds numbers or concatenates text; if either side is `word` or `let`, the result is `word`. Arithmetic and ordered comparisons require numbers. Integer comparisons remain exact; mixed arithmetic with `dec` uses floating-point conversion. `==` and `!=` compare numeric values across `num`, `bnum`, and `dec`; arrays and functions compare by identity. `and` and `or` short-circuit and return an operand. `not` returns a boolean. Falsey values are `false`, `null`, zero, empty `word`, empty array, and the zero byte `let`.

## Arrays

```ksl
a = [10, 20, 30]
print(a[0])       // 10
a[1] += 5
print(a)          // [10, 25, 30]
```

Indices are zero-based integer numbers (`num` or `bnum`; an integral `dec` is also accepted). Negative, fractional, and out-of-range indices produce errors; empty arrays have no valid index. Arrays are mutable reference values, so assigning one array to another variable shares it. `len(a)` returns its length.

Arrays, function parameters, call arguments, parenthesized expressions, and array indices may span lines. Arrays and comma-separated parameter/argument lists allow a trailing comma.

## Control flow

```ksl
if <score >= 100> [
    print("nice")
] else [
    print("keep going")
]

while <running> [
    update()
]

loop [
    break
]

i = 0
whileif <i < 3>
<i++> [
    print(i)
]
```

`whileif` evaluates condition, executes body, executes step, and repeats. `break` leaves a loop without executing its step. `continue` runs the step of `whileif` before checking the condition again. The step can be any expression, including `i++`, `i--`, `i += 2`, `i -= 2`, `i *= 2`, or a function call. Conditions may use `<`, `>`, `<=`, `>=` inside their surrounding angle delimiters. The body and step can be on the same line or separate lines.

## Functions and built-ins

```ksl
fn add(a, b) [
    return a + b
]
print(add(2, 3))
```

Parameters are dynamically typed. No `return` means `null`. Functions can call themselves. `return`, `break`, and `continue` outside the relevant context are errors. Built-in names `print`, `input`, `len`, and `type` are recognized at call sites: `print(a, b)` writes arguments separated by spaces and a newline, `input(prompt)` reads a line as a `word`, `len(value)` accepts an array or `word`, and `type(value)` reports its KSL type. Assigning to a built-in name does not replace its call behavior in v0.1.

Function calls are limited to 256 nested calls. A deeper recursive call produces `CallDepthExceeded` with a source location instead of exhausting the interpreter's C++ stack.

## Operators, highest precedence first

1. Calls `f(...)`, indexing `a[...]`, postfix `++` and `--`.
2. Unary `-`, `+`, `not`, `!`.
3. `*`, `/`, `%`.
4. `+`, `-`.
5. `<`, `>`, `<=`, `>=`.
6. `==`, `!=`.
7. `and`.
8. `or`.
9. Assignment `=`, `+=`, `-=`, `*=`, `/=` (right associative).

`++` and `--` require a variable, modify it, and return its previous value. `%` uses integer remainder for two integers and floating-point remainder otherwise. Division and remainder by zero are errors. Newlines separate statements; semicolons are invalid. `//` starts a comment through the end of the line. Square brackets are used both for array literals in expressions and blocks after control/function headers; context distinguishes them.
