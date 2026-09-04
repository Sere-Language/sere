# Sere for Linux x64

Run `bin/sere` from the extracted portable folder, or install per user with:

```sh
bash install.sh
bash install.sh --prefix "$HOME/tools/sere" --editor
```

The release `.run` installer accepts the same options. No root privileges are
needed. Installation creates `~/.local/bin/sere` and adds that directory to the
Bash/profile PATH when absent. Open a new terminal afterward.

Run `bash uninstall.sh` from the installed folder to remove Sere. The bundled
VSIX is optional and requires an existing VS Code or Cursor installation.
The package targets Linux x64 with the glibc baseline of its build machine;
it is not a statically linked distribution and still requires host system
libraries. LLVM is bundled. Native projects using CMake need CMake separately.
