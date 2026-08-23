import pathlib
import subprocess
import os

sere = pathlib.Path(
    r"C:\Users\jackw\OneDrive\Desktop\git-projects\sere\build\windows-clang-cl-relwithdebinfo\bin\staging\sere.exe"
)
tmpdir = pathlib.Path(os.environ["TEMP"])
src = pathlib.Path(
    r"C:\Users\jackw\OneDrive\Desktop\git-projects\sere\stdlib\gl.sere"
).read_text(encoding="utf-8").splitlines(True)

def run(name: str, text: str) -> None:
    path = tmpdir / name
    path.write_text(text, encoding="utf-8")
    result = subprocess.run([str(sere), "--analyze", str(path)], capture_output=True, text=True)
    print(f"{name} exit={result.returncode} stdout={result.stdout[:160]!r} stderr={result.stderr[:160]!r}")

run("hello.sere", "def main() -> void:\n    print(1)\n")
for n in (5, 20, 22, 24, 30, 50, 100):
    run(f"gln_{n}.sere", "".join(src[:n]))
