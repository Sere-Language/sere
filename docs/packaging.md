# Packaging

See [the release guide](../releases/README.md) for Windows packaging, installer
options, and validation. Run `releases/stage.ps1` to build both the portable ZIP
and per-user setup EXE in `releases/pre-<version>/`.

For Linux, build on Linux first:

```bash
./scripts/bootstrap-llvm.sh
export SERE_LLVM_DIR="$HOME/.local/share/sere/toolchains/llvm-22.1.8"
cmake --preset linux-clang-relwithdebinfo
cmake --build --preset linux-clang-relwithdebinfo
ctest --preset linux-clang-relwithdebinfo
./releases/stage.sh
```

Linux staging writes a portable tarball, optional ZIP, and offline `.run`
installer in the same versioned release folder. LLVM tools are bundled, while
host system libraries and glibc remain platform requirements. Install with
`bash Sere-pre-x.x.x-linux-x64-setup.run [--prefix DIR] [--editor]`.
