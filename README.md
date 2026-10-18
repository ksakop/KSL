# KSL

> **Kodexically Slow Language** — a programming language that started as a joke and made the unfortunate decision to work.

KSL is a small, self-contained programming language/interpreter written in C++17. It has functions, recursion, arrays, loops, a REPL, source-located errors, executable packaging, and syntax choices of questionable origin.

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

Output:

```text
Hello, KSL 0
Hello, KSL 1
Hello, KSL 2
```

Yes, `whileif` is real. No, curly braces were not invited.

## Why does this exist?

Because apparently C++ was not enough of a warning.

KSL started as a joke. Then it acquired an interpreter, tests, a REPL, executable packaging, documentation, and its own Windows editor. At this point stopping would be irresponsible.

## KSL vocabulary

| KSL | Meaning |
| --- | --- |
| `word` | string |
| `let` | character |
| `num` | 32-bit integer |
| `bnum` | 64-bit integer |
| `dec` | decimal |

Variables and function parameters need no type declarations. Arrays start at zero, because some traditions deserve protection.

## Build and run

Requires a C++17 compiler and Python 3 for tests. CMake 3.16+ is recommended:

```sh
cmake -S . -B build
cmake --build build
./build/ksl examples/whileif.ksl
ctest --test-dir build --output-on-failure
```

On Windows, run `build/Debug/ksl.exe examples/whileif.ksl` with a multiconfiguration generator.

No CMake? The emergency exit still works:

```sh
g++ -std=c++17 -O2 src/main.cpp -o ksl
python3 tests/run.py ./ksl
```

Run `ksl program.ksl` for a file. Run `ksl` without arguments for the REPL; variables persist across entered lines. Incomplete blocks, functions, and calls continue at the `...` prompt until closed. In the REPL, put `else [` on the same line as the preceding `]`.

## Build an executable

After building KSL, turn a `.ksl` file into a runnable executable with:

```sh
python3 kslc.py program.ksl -o program
```

On Windows:

```text
py kslc.py program.ksl -o program.exe
```

Packaging does not need a C++ compiler once `ksl` is built.

For an editor with **Save As**, **Run**, and **Build EXE**, download [KSL Studio for Windows](https://github.com/ksakop/KSL/actions/workflows/studio.yml) from its latest successful run. See the [executable guide](docs/compiler.md) for details and the commercial licensing condition.

If you only have the GitHub website, create your `.ksl` file there and use **Actions → Build a KSL program for Windows → Run workflow**. Download the resulting ZIP artifact; the [executable guide](docs/compiler.md#using-only-the-github-website) shows each step.

## Status

Implemented: `num` and `bnum` integers, `dec` decimals, `word` strings, `let` characters, booleans, null, mutable arrays, expressions with precedence, `if`/`else`, `while`, `loop`, `whileif`, `break`, `continue`, functions, recursion, `return`, `print`, `input`, `len`, `type`, and source-located errors.

No type declarations, semicolons, curly blocks, imports, or negative array indices.

Limitations: no module system, native compiler, closures with lexical block scopes, object types, or array slicing. `input` returns a `word`. Decimal arithmetic uses IEEE 754 doubles; mixed integer/decimal arithmetic can lose precision.

## Documentation

- [Language reference](docs/language.md)
- [Syntax notes](docs/syntax.md)
- [Executable guide](docs/compiler.md)
- [Architecture](docs/architecture.md)
- [Roadmap](docs/roadmap.md)
- [Examples](examples/)
- [Contributing guide](CONTRIBUTING.md)

## License

KSL is [source available under custom terms](LICENSE): using and sharing KSL without charge is permitted; selling KSL or its forks requires a separate written profit-sharing agreement. Programs independently written in KSL may be sold without that agreement. These terms are not an OSI-approved open source license.

CI builds and runs the same end-to-end tests on Linux and Windows for each push and pull request.

---

*If KSL breaks your computer, the computer has demonstrated good judgment.*
