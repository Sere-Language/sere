# Sere pre-0.1.5

Two installable trees. Do not mix `sere.exe` and the Linux ELF in one `bin/`.

| Platform | Tree | Archive |
| --- | --- | --- |
| Windows x64 | `windows-x64/` | `../Sere-pre-0.1.5-windows-x64.zip` |
| Linux x64 | `linux-x64/` | `../Sere-pre-0.1.5-linux-x64.tar.gz` (and `.zip` if staged on Linux) |

## Windows

```powershell
.\windows-x64\install.ps1
sere --version
```

## Linux (x86_64, Ubuntu 22.04 / glibc 2.35)

This ELF will not run on RISC-V [NanoVM](https://userland.run/docs/).
The Linux archive is self-contained (compiler + LLVM 22.1.8 + stdlib).

```bash
chmod +x linux-x64/install.sh
./linux-x64/install.sh
sere --version
```

Linux stdlib does not include the `windows` module.
