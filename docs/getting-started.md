# Getting started with Sere

This walkthrough uses PowerShell on Windows and an installed `sere` on `PATH`.
See [release installation](../releases/README.md) for packages, or the
[bootstrap commands](../README.md#bootstrap) to build the compiler yourself.
Repository builds provide `bin/sere.exe`.

## Check your compiler

```powershell
sere --version
sere --print-env
```

`--print-env` reports compiler, standard-library, and toolchain paths as JSON.
Use it when a terminal and an editor appear to use different installations.

## Create and run an application

```powershell
sere init hello
cd hello
. .\scripts\activate.ps1
```

The leading dot activates the project in the current terminal. Replace
`src/main.sere` with:

```sere
def double(value: i32) -> i32:
    return value * 2

def main() -> i32:
    values: list[i32] = [1, 2, 3]
    values.append(4)
    for value in values:
        print(double(value))
    return 0
```

```powershell
sere run
```

The program prints `2`, `4`, `6`, and `8`, each on its own line. `run` builds
before executing. Use `sere build` to build without running; the default Windows
output is `bin/hello.exe`. Returning zero from `main` indicates success.

Use `sere run -- first second` to pass arguments to your program. When finished,
run `deactivate` to restore the terminal's previous environment.

## Move code into a module

Create `src/helpers.sere`:

```sere
def double(value: i32) -> i32:
    return value * 2
```

Remove the `double` definition from `src/main.sere` and put this import at the top:

```sere
from helpers import double
```

Run `sere run` again. Modules beside the importing file can be imported by their
filename without `.sere`. See [projects](projects.md) for distributing libraries.

## Choose types deliberately

Local values can infer their types: `count = 3` and `name = "Sere"` need no
annotation. Explicit annotations are useful for function interfaces and empty
collections, such as `values: list[i32] = []`.

Omitted function parameter types use `Any`. Most omitted return types also use
`Any`; `main` and `__init__` have special defaults. An `Any` value retains its
concrete type, and converting it back checks that type at runtime. See
[gradual typing and inference](gradual-typing.md) before using `Any` at interfaces.

## Inspect a program without running it

From the project root:

```powershell
sere --analyze src/main.sere
sere --emit-llvm src/main.sere -o main.ll
sere --emit-asm src/main.sere -o main.s
```

`--analyze` produces JSON diagnostics without generating code. LLVM and assembly
output help investigate generated code; neither command runs the program.
Compilation errors include names such as `NameError` or `TypeError`. See the
[diagnostic reference](language.md#diagnostics) for their meanings.

## Where to go next

- [Language reference](language.md): syntax, types, classes, errors, and macros.
- [Projects](projects.md): manifest settings, libraries, and troubleshooting.
- [Standard library](stdlib.md): available modules and numeric array examples.
- [Editor support](../editors/vscode/README.md): extension setup and configuration.
- [Compiler handbook](README.md): architecture and contributor documentation.
