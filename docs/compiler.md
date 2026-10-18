# Building an executable from KSL

Write a self-contained `.ksl` file, for example `my_program.ksl`:

```ksl
fn square(x) [
    return x * x
]

print(square(7))
```

Build KSL once, then package your program:

```sh
cmake -S . -B build
cmake --build build
python3 kslc.py my_program.ksl -o my_program
./my_program
```

On Windows, use `py kslc.py my_program.ksl -o my_program.exe` and then
`my_program.exe`. You need Python 3, CMake, and a C++17 compiler to build the
executable. Once built, the executable needs no `.ksl` source file or KSL
installation on the target computer. It still needs the standard C++ runtime
for its platform.

`kslc.py` first parses the KSL source with `ksl --check`, so syntax errors are
reported before the C++ build starts. It then embeds the source and the KSL
interpreter in a platform-specific executable. The KSL program is parsed and
executed by that embedded interpreter when the executable starts. This is a
practical executable packaging step, **not** translation of KSL expressions
to native machine instructions. A future compiler can replace the embedded
interpreter with generated code while preserving the language front end.

The current license permits selling independently written KSL programs.
Bundling the KSL interpreter in a paid product, including an executable made
with `kslc.py`, requires the separate commercial agreement described in
[`LICENSE`](../LICENSE). Source-only KSL programs do not include the runtime.
