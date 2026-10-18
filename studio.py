"""KSL Studio: a small portable Windows editor, runner, and EXE packager."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from kslc import package


def interpreter_path():
    root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    candidates = [root / "ksl.exe", root / "build" / "Release" / "ksl.exe",
                  root / "build" / "Debug" / "ksl.exe", root / "ksl"]
    return next((path for path in candidates if path.is_file()), None)


class Studio:
    def __init__(self, root):
        self.root = root
        self.path = None
        self.dirty = False
        self.running = False
        self.ksl = interpreter_path()
        root.title("KSL Studio — Untitled")
        root.geometry("1050x720")
        root.minsize(640, 400)
        root.configure(bg="#111317")

        toolbar = tk.Frame(root, bg="#20242a", padx=8, pady=7)
        toolbar.pack(fill="x")
        for label, action in (("New", self.new), ("Open", self.open),
                              ("Save", self.save), ("Save As", self.save_as),
                              ("Run  F5", self.run), ("Build EXE  Ctrl+B", self.build)):
            tk.Button(toolbar, text=label, command=action, bg="#333a43", fg="#f3f5f7",
                      activebackground="#6e8794", activeforeground="white", relief="flat",
                      padx=13, pady=5).pack(side="left", padx=3)
        self.status = tk.StringVar(value="Ready")
        tk.Label(toolbar, textvariable=self.status, bg="#20242a", fg="#aab8c4").pack(side="right", padx=8)

        splitter = tk.PanedWindow(root, orient="vertical", sashwidth=6, bg="#111317")
        splitter.pack(fill="both", expand=True, padx=9, pady=9)
        editor_frame = tk.Frame(splitter, bg="#111317")
        self.editor = tk.Text(editor_frame, undo=True, wrap="none", font=("Consolas", 12),
                              bg="#171b20", fg="#e3e7eb", insertbackground="white",
                              selectbackground="#405c71", padx=14, pady=12, relief="flat",
                              tabs=(32,))
        editor_scroll = ttk.Scrollbar(editor_frame, command=self.editor.yview)
        self.editor.configure(yscrollcommand=editor_scroll.set)
        editor_scroll.pack(side="right", fill="y")
        self.editor.pack(side="left", fill="both", expand=True)
        splitter.add(editor_frame, minsize=150, stretch="always")

        output_frame = tk.Frame(splitter, bg="#111317")
        tk.Label(output_frame, text="OUTPUT", bg="#111317", fg="#9aabb8",
                 anchor="w", padx=8, pady=5).pack(fill="x")
        self.output = tk.Text(output_frame, state="disabled", height=9, wrap="word",
                              font=("Consolas", 10), bg="#0d1014", fg="#d5e1e8",
                              padx=12, pady=8, relief="flat")
        self.output.pack(fill="both", expand=True)
        splitter.add(output_frame, minsize=80)
        self.editor.bind("<<Modified>>", self.modified)
        for shortcut, action in (("<Control-s>", self.save), ("<Control-Shift-S>", self.save_as),
                                 ("<Control-o>", self.open), ("<Control-n>", self.new),
                                 ("<Control-b>", self.build), ("<F5>", self.run)):
            root.bind(shortcut, lambda _event, fn=action: (fn(), "break")[-1])
        root.protocol("WM_DELETE_WINDOW", self.close)
        if not self.ksl:
            self.status.set("KSL interpreter missing")
            messagebox.showerror("KSL Studio", "Bundled ksl.exe was not found.")
        self.editor.focus_set()

    def title(self):
        self.root.title("KSL Studio — " + (str(self.path) if self.path else "Untitled") +
                        (" *" if self.dirty else ""))

    def modified(self, _event=None):
        if self.editor.edit_modified():
            self.dirty = True
            self.title()
            self.editor.edit_modified(False)

    def set_text(self, text):
        self.editor.delete("1.0", "end")
        self.editor.insert("1.0", text)
        self.editor.edit_modified(False)
        self.dirty = False
        self.title()

    def confirm_discard(self):
        if not self.dirty:
            return True
        result = messagebox.askyesnocancel("Unsaved changes", "Save changes before continuing?")
        if result is None:
            return False
        return self.save() if result else True

    def new(self):
        if self.confirm_discard():
            self.path = None
            self.set_text("")

    def open(self):
        if not self.confirm_discard():
            return
        name = filedialog.askopenfilename(filetypes=[("KSL programs", "*.ksl"), ("All files", "*.*")])
        if name:
            try:
                content = Path(name).read_text(encoding="utf-8")
            except (OSError, UnicodeError) as error:
                messagebox.showerror("Open failed", str(error))
                return
            self.path = Path(name)
            self.set_text(content)

    def save_as(self):
        name = filedialog.asksaveasfilename(defaultextension=".ksl", initialfile=self.path.name if self.path else "program.ksl",
                                            filetypes=[("KSL programs", "*.ksl")])
        if not name:
            return False
        self.path = Path(name)
        return self.save()

    def save(self):
        if not self.path:
            return self.save_as()
        try:
            self.path.write_text(self.editor.get("1.0", "end-1c"), encoding="utf-8")
        except OSError as error:
            messagebox.showerror("Save failed", str(error))
            return False
        self.dirty = False
        self.title()
        self.status.set("Saved " + self.path.name)
        return True

    def show_output(self, text):
        self.output.configure(state="normal")
        self.output.delete("1.0", "end")
        self.output.insert("end", text)
        self.output.configure(state="disabled")

    def run(self):
        if self.running or not self.ksl:
            return
        try:
            with tempfile.NamedTemporaryFile(mode="w", suffix=".ksl", encoding="utf-8",
                                             delete=False) as source:
                source.write(self.editor.get("1.0", "end-1c"))
                temporary = Path(source.name)
        except OSError as error:
            messagebox.showerror("Run failed", str(error))
            return
        self.running = True
        self.status.set("Running...")
        self.show_output("")
        def worker():
            try:
                result = subprocess.run([str(self.ksl), str(temporary)], stdin=subprocess.DEVNULL,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        timeout=30,
                                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
                if result.returncode:
                    output += f"\nExited with code {result.returncode}.\n"
            except (OSError, subprocess.TimeoutExpired) as error:
                output = str(error)
            finally:
                temporary.unlink(missing_ok=True)
            self.root.after(0, lambda: self.finished(output))
        threading.Thread(target=worker, daemon=True).start()

    def finished(self, output):
        self.running = False
        self.show_output(output)
        self.status.set("Finished")

    def build(self):
        if not self.ksl or not self.save():
            return
        target = filedialog.asksaveasfilename(defaultextension=".exe",
                                              initialfile=self.path.stem + ".exe",
                                              filetypes=[("Windows executable", "*.exe")])
        if not target:
            return
        try:
            result = package(self.path, target, self.ksl)
        except (ValueError, OSError) as error:
            messagebox.showerror("Build failed", str(error))
            return
        if result:
            self.status.set("Build failed — see error output")
            return
        self.status.set("Built " + Path(target).name)
        messagebox.showinfo("KSL Studio", "Built " + target)

    def close(self):
        if self.confirm_discard():
            self.root.destroy()


def main():
    if sys.argv[1:] == ["--self-test"]:
        def mark(step):
            if path := os.environ.get("KSL_STUDIO_TEST_LOG"):
                with open(path, "a", encoding="utf-8") as log:
                    log.write(step + "\n")
        mark("Studio started")
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "check.ksl"
            output = Path(directory) / ("check.exe" if os.name == "nt" else "check")
            source.write_text("print(6 * 7)\n", encoding="utf-8")
            ksl = interpreter_path()
            mark("Bundled interpreter found" if ksl else "Bundled interpreter missing")
            if ksl is None or package(source, output, ksl):
                raise RuntimeError("bundled KSL failed to package a program")
            mark("Program packaged")
            source.unlink()
            result = subprocess.run([str(output)], capture_output=True, timeout=10,
                                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            if result.returncode or result.stdout != b"42\n":
                raise RuntimeError("packaged program failed to run")
            mark("Program ran successfully")
        return
    root = tk.Tk()
    Studio(root)
    root.mainloop()


if __name__ == "__main__":
    main()
