# Building an executable from KSL

## Using only the GitHub website

1. In the repository, select **Add file → Create new file** and name it
   `my_program.ksl`. Write your code, then select **Commit changes**.
2. Open **Actions → Build a KSL program for Windows → Run workflow**.
   Enter `my_program.ksl` as the source path and run it on `main`.
3. Open the new workflow run. When it succeeds, download the
   `ksl-my_program-windows` artifact at the bottom of the run page.
4. Extract the ZIP and run `my_program.exe` on Windows.

The first run after adding this workflow builds `examples/hello.ksl` as a
smoke test. You can keep your own program in a folder too: enter its relative
path, such as `examples/my_program.ksl`. The file name must use ASCII letters,
digits, underscores, or hyphens. GitHub Actions runs the language tests before
building your program. It checks syntax before packaging, but a runtime error
in your program can still occur when you launch the `.exe`.

## Building on your computer

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
