# KSL v0.1 language reference

## Values and variables

`x = 5` creates or updates a variable. Values have types; variables have no declared type. `x = "hello"` can follow `x = 5`. Values are numbers (IEEE 754 double), strings, booleans (`true`, `false`), `null`, arrays, and functions. Numeric literals support decimal digits and a decimal point; scientific notation is not yet supported. A missing variable is an error.

Strings use double quotes and support `\n`, `\t`, `\"`, `\\`. `+` adds numbers or concatenates strings; if either side is a string, the other value is converted to display text. Arithmetic and ordered comparisons require numbers. `==` and `!=` compare values of the same type; arrays and functions compare by identity. `and` and `or` short-circuit and return an operand. `not` returns a boolean. Falsey values are `false`, `null`, zero, empty string, and empty array.

## Arrays

```ksl
a = [10, 20, 30]
print(a[0])       // 10
a[1] += 5
print(a)          // [10, 25, 30]
```

Indices are zero-based integer numbers. Negative, fractional, and out-of-range indices produce errors; empty arrays have no valid index. Arrays are mutable reference values, so assigning one array to another variable shares it. `len(a)` returns its length.

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

Parameters are dynamically typed. No `return` means `null`. Functions can call themselves. `return`, `break`, and `continue` outside the relevant context are errors. Built-in names `print`, `input`, and `len` are recognized at call sites: `print(a, b)` writes arguments separated by spaces and a newline, `input(prompt)` reads a line as a string, and `len(value)` accepts an array or string. Assigning to a built-in name does not replace its call behavior in v0.1.

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

`++` and `--` require a variable, modify it, and return its previous value. `%` uses floating-point remainder. Division and remainder by zero are errors. Newlines separate statements; semicolons are invalid. `//` starts a comment through the end of the line. Square brackets are used both for array literals in expressions and blocks after control/function headers; context distinguishes them.
