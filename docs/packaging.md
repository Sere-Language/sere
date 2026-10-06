# Packaging

See [the release guide](../releases/README.md) for Windows packaging, installer
options, and validation. Run `releases/stage.ps1` to build both the portable ZIP
and per-user setup EXE in `releases/pre-<version>/`.

For Linux, build on Linux first (`scripts/bootstrap-llvm.sh` only runs on Linux
x86_64 and the `linux-clang-relwithdebinfo` preset is Linux-only):

```bash
./scripts/bootstrap-llvm.sh
export SERE_LLVM_DIR="$HOME/.local/share/sere/toolchains/llvm-22.1.8"
cmake --preset linux-clang-relwithdebinfo
cmake --build --preset linux-clang-relwithdebinfo
ctest --preset linux-clang-relwithdebinfo
./releases/stage.sh
```

`releases/stage-linux.ps1` performs those steps for you inside whichever Linux
environment is available (WSL2, Docker, or the host) and publishes the archives
into `releases/<version>/`; `releases/linux-build.sh` is the script it runs inside
that environment. Use `-BuildDir`/`-LlvmRoot` to package a Linux build produced
elsewhere instead of building here.

On a Linux host, `releases/stage-linux.sh` is the bash equivalent of those
commands: it installs missing build dependencies, bootstraps LLVM, configures and
builds `linux-clang-relwithdebinfo` in place, then runs `releases/stage.sh` and
leaves the archives in `releases/<version>/`. Accepts `--name`, `--preset`,
`--jobs`, `--test`, `--skip-build`, and `--skip-deps`.

Linux staging writes a portable tarball, optional ZIP, and offline `.run`
installer in the same versioned release folder. LLVM tools are bundled, while
host system libraries and glibc remain platform requirements. Install with
`bash Sere-pre-x.x.x-linux-x64-setup.run [--prefix DIR] [--editor]`.

To package a Linux build from the release builder instead of the Linux host, pass
it to `releases/stage-linux.ps1 -BuildDir <linux-build> -LlvmRoot <linux-llvm>`;
`releases/stage.ps1 -LinuxOnly` does the same and is exposed as the
`release: linux tar` task.
