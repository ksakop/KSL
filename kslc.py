#!/usr/bin/env python3
"""Package a checked KSL file with the already-built interpreter."""

import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
MAGIC = b"KSLPKG01"


def find_ksl(explicit=None):
    if explicit:
        return Path(explicit).resolve()
    candidates = (
        ROOT / "build" / "ksl",
        ROOT / "build" / "Release" / "ksl.exe",
        ROOT / "build" / "Debug" / "ksl.exe",
        ROOT / "ksl",
    )
    return next((path for path in candidates if path.is_file()), None)


def package(source, output, ksl):
    """Create a runnable copy of ksl with source and filename appended."""
    source, output, ksl = Path(source).resolve(), Path(output).resolve(), Path(ksl).resolve()
    if not source.is_file() or source.suffix.lower() != ".ksl":
        raise ValueError("source must be an existing .ksl file")
    if not ksl.is_file():
        raise ValueError("build KSL first or pass --ksl with a built interpreter")
    if output in (source, ksl):
        raise ValueError("output must differ from the source and KSL interpreter")
    checked = subprocess.run([str(ksl), "--check", str(source)], check=False,
                             capture_output=True, text=True, encoding="utf-8", errors="replace",
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if checked.returncode:
        raise ValueError(checked.stderr.strip() or "KSL syntax check failed")
    data, name = source.read_bytes(), source.name.encode("utf-8")
    if len(data) > 64 * 1024 * 1024 or len(name) > 4096:
        raise ValueError("program source or filename is too large to package")
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".kslc-", delete=False) as target:
            temp = Path(target.name)
            with ksl.open("rb") as original:
                shutil.copyfileobj(original, target)
            target.write(data)
            target.write(name)
            target.write(struct.pack("<QQ", len(data), len(name)))
            target.write(MAGIC)
        shutil.copymode(ksl, temp)
        temp.replace(output)
    finally:
        if temp is not None:
            temp.unlink(missing_ok=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description="Package a .ksl program as an executable")
    parser.add_argument("source", type=Path, help="KSL source file")
    parser.add_argument("-o", "--output", type=Path, help="output executable")
    parser.add_argument("--ksl", type=Path, help="path to the built KSL interpreter")
    args = parser.parse_args()
    source = args.source.resolve()
    output = (args.output or source.with_suffix(".exe" if os.name == "nt" else "")).resolve()
    if os.name == "nt" and output.suffix.lower() != ".exe":
        output = output.with_suffix(".exe")
    try:
        result = package(source, output, find_ksl(args.ksl) or Path("missing-ksl"))
    except (OSError, ValueError) as error:
        print(f"kslc: {error}", file=sys.stderr)
        return 1
    if result == 0:
        print(f"Built {output}")
    return result


if __name__ == "__main__":
    sys.exit(main())
