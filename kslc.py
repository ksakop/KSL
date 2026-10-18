#!/usr/bin/env python3
"""Package a checked KSL source file and its interpreter into an executable."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent


def find_ksl(explicit):
    if explicit:
        return Path(explicit).resolve()
    candidates = (
        ROOT / "build" / "ksl",
        ROOT / "build" / "Release" / "ksl.exe",
        ROOT / "build" / "Debug" / "ksl.exe",
        ROOT / "ksl",
    )
    return next((path for path in candidates if path.is_file()), None)


def byte_array(data):
    data += b"\0"
    return ",\n    ".join(", ".join(f"0x{byte:02x}" for byte in data[i:i + 16])
                           for i in range(0, len(data), 16))


def build_with_cmake(project, output):
    if not shutil.which("cmake"):
        raise RuntimeError("no C++ compiler or CMake found")
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\n"
        "project(ksl_packaged_program LANGUAGES CXX)\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "add_executable(ksl_program program.cpp)\n"
        "install(TARGETS ksl_program RUNTIME DESTINATION bin)\n",
        encoding="utf-8",
    )
    build = project / "build"
    install = project / "install"
    subprocess.run(["cmake", "-S", str(project), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", str(build), "--config", "Release"], check=True)
    subprocess.run(["cmake", "--install", str(build), "--config", "Release", "--prefix", str(install)], check=True)
    executable = install / "bin" / ("ksl_program.exe" if os.name == "nt" else "ksl_program")
    shutil.copy2(executable, output)


def main():
    parser = argparse.ArgumentParser(description="Build a runnable executable from a .ksl file")
    parser.add_argument("source", type=Path, help="KSL source file")
    parser.add_argument("-o", "--output", type=Path, help="output executable path")
    parser.add_argument("--ksl", type=Path, help="path to the built ksl interpreter")
    args = parser.parse_args()

    source = args.source.resolve()
    if not source.is_file() or source.suffix.lower() != ".ksl":
        parser.error("source must be an existing .ksl file")
    output = (args.output or source.with_suffix(".exe" if os.name == "nt" else "")).resolve()
    if os.name == "nt" and output.suffix.lower() != ".exe":
        output = output.with_suffix(".exe")
    if source == output:
        parser.error("output must differ from the source file")
    ksl = find_ksl(args.ksl)
    if not ksl or not ksl.is_file():
        parser.error("build ksl first with 'cmake -S . -B build' and 'cmake --build build'")

    try:
        checked = subprocess.run([str(ksl), "--check", str(source)], check=False)
        if checked.returncode:
            return checked.returncode
        with tempfile.TemporaryDirectory(prefix="kslc-") as directory:
            project = Path(directory)
            shutil.copy2(ROOT / "src" / "main.cpp", project / "main.cpp")
            program = (
                "#define KSL_EMBEDDED_PROGRAM\n"
                "static const unsigned char kslEmbeddedSource[] = {\n    "
                + byte_array(source.read_bytes()) + "\n};\n"
                "static const unsigned char kslEmbeddedName[] = {\n    "
                + byte_array(source.name.encode("utf-8")) + "\n};\n"
                '#include "main.cpp"\n'
            )
            (project / "program.cpp").write_text(program, encoding="utf-8")
            compiler = os.environ.get("CXX") or next(
                (tool for name in ("c++", "g++", "clang++") if (tool := shutil.which(name))), None
            )
            built = project / ("program.exe" if os.name == "nt" else "program")
            if compiler and (os.name != "nt" or os.environ.get("CXX")):
                subprocess.run([compiler, "-std=c++17", "-O2", str(project / "program.cpp"), "-o", str(built)], check=True)
            else:
                build_with_cmake(project, built)
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(built, output)
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"kslc: {error}", file=sys.stderr)
        return 1
    print(f"Built {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
