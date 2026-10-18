# KSL

**Kodexically Slow Language** — a suspiciously usable programming language.

KSL started as a joke. Unfortunately, it runs. Version 0.1 is a small, self-contained, dynamically typed interpreter written in C++17. Its square blocks and angle conditions are deliberate; its arrays start at zero.

Values have KSL names: `word` (string), `let` (character), `num` (32-bit integer), `bnum` (64-bit integer), and `dec` (decimal). Variables and function parameters need no type declarations.

```ksl
fn greet(name) [
    print("Hello, " + name)
]

i = 0
whileif <i < 3>
<i++> [
    greet("KSL " + i)
]
```

The loop prints `KSL 0`, `KSL 1`, `KSL 2`. The step runs **after** the body, including on `continue`.

## Build and run

Requires a C++17 compiler and Python 3 for tests. CMake 3.16+ is recommended:

```sh
cmake -S . -B build
cmake --build build
./build/ksl examples/whileif.ksl
ctest --test-dir build --output-on-failure
```

On Windows, run `build/Debug/ksl.exe examples/whileif.ksl` with a multiconfiguration generator. If CMake is unavailable, `g++ -std=c++17 -O2 src/main.cpp -o ksl` builds the same executable; run `python3 tests/run.py ./ksl` for tests.

Run `ksl program.ksl` for a file. Run `ksl` without arguments for a small REPL; variables persist across entered lines. Incomplete blocks, functions, and calls continue at the `...` prompt until closed. In the REPL, put `else [` on the same line as the preceding `]`.

To turn your own `.ksl` file into a runnable executable, run `python3 kslc.py program.ksl -o program` after building KSL. On Windows, use `py kslc.py program.ksl -o program.exe`. This packages the interpreter with your program; see the [executable guide](docs/compiler.md) for what that means and the commercial licensing condition.

## Status

Implemented: `num` and `bnum` integers, `dec` decimals, `word` strings, `let` characters, booleans, null, mutable arrays, expressions with precedence, `if`/`else`, `while`, `loop`, `whileif`, `break`, `continue`, functions, recursion, `return`, `print`, `input`, `len`, `type`, and source-located errors. No type declarations, semicolons, curly blocks, imports, or negative array indices.

Limitations: no module system, native compiler, closures with lexical block scopes, object types, or array slicing. `input` returns a `word`. Decimal arithmetic uses IEEE 754 doubles; mixed integer/decimal arithmetic can lose precision. Full details: [language reference](docs/language.md), [syntax notes](docs/syntax.md), [architecture](docs/architecture.md), [roadmap](docs/roadmap.md).

See [examples](examples/) and [contributing guide](CONTRIBUTING.md). KSL is [source available under custom terms](LICENSE): using and sharing KSL without charge is permitted; selling KSL or its forks requires a separate written profit-sharing agreement. Programs independently written in KSL may be sold without that agreement. These terms are not an OSI-approved open source license.

CI builds and runs the same end-to-end tests on Linux and Windows for each push and pull request.
