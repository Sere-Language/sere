# Release packaging

On Windows, run from a normal PowerShell terminal:

```powershell
.\releases\stage.ps1
```

The builder configures a Release build, builds and runs CTest, packages the VSIX,
stages the required tools and libraries, runs a portable compilation smoke test,
and creates `releases/pre-<CMake project version>/` containing:

- `windows-x64/`: extracted portable distribution
- `Sere-pre-x.x.x-windows-x64-portable.zip`
- `Sere-pre-x.x.x-windows-x64-setup.exe`: offline, per-user Inno Setup installer
- `SHA256SUMS.txt`

Use `-BuildDir <path>` and `-LlvmRoot <path>` for explicit build inputs,
`-SkipBuild` to package an existing matching compiler, `-WithoutEditor` to omit
the VSIX, or `-PortableOnly` to omit the setup EXE. Inno Setup is only needed on
the release builder; specify `-Iscc <path>` if it is not discovered. The existing
`sere --build-installer [-o setup.exe]` command invokes this same pipeline.
You can also run `cmake --build <build-dir> --target sere_release`.

The release builder requires LLVM development files, MSVC x64 development tools,
Windows SDK, CMake, Ninja, and Inno Setup 6. Recipients do not install these tools:
the payload includes the selected compiler binaries and x64 headers/libraries.
Only projects explicitly using another native build system, such as CMake, need
that build system installed separately. Qt support depends on the release build;
when Qt is enabled, its runtime must be present in the compiler output directory.

To test a payload again:

```powershell
.\packaging\test-portable.ps1 -Package .\releases\pre-0.1.5\windows-x64
```

This clears developer environment variables and PATH tools, asserts that both the
portable compiler and venv select bundled clang, then builds and runs a project.
A pristine Windows VM remains useful for validating OS-level compatibility.

See [installation instructions](../packaging/README.md) for command-line setup.
