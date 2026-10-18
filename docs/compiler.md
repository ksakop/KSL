# Building an executable from KSL

## KSL Studio on Windows

Download `KSL-Studio-Windows` from a successful run of
[Build portable KSL Studio](https://github.com/ksakop/KSL/actions/workflows/studio.yml).
Unzip it and open `KSL-Studio.exe`. The editor contains the KSL interpreter;
you do not need Python, CMake, or a C++ compiler on your computer. Use **Save
As** for a `.ksl` file, **Run** (F5) to see its output, and **Build EXE**
(Ctrl+B) to create a standalone program. **Run** executes the current editor
contents even before saving. It has a 30-second timeout and does not currently
support interactive `input()` in its output panel. Programs that need input
can be built and run in a console window instead.

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
`my_program.exe`. Building the interpreter initially needs CMake and a C++17
compiler; packaging more programs only needs Python and that built interpreter.
KSL Studio bundles both the packaging code and the interpreter. Once built,
the executable needs no `.ksl` source file or KSL installation on the target
computer. It still needs the standard C++ runtime for its platform.

`kslc.py` first parses the KSL source with `ksl --check`, so syntax errors are
reported before packaging. It then copies the platform's interpreter and
appends the source, its filename, and a small length-checked trailer. When the
new executable starts, the interpreter reads and runs the appended program.
This is executable packaging, **not** translation of KSL expressions to native
machine instructions. A future compiler can replace this packaging mechanism.

The current license permits selling independently written KSL programs.
Bundling the KSL interpreter in a paid product, including an executable made
with `kslc.py`, requires the separate commercial agreement described in
[`LICENSE`](../LICENSE). Source-only KSL programs do not include the runtime.
