Sere project: test-environment
====================
src/       Sere sources (entry: src/main.sere)
libs/      Sere modules and native C++ extensions
venv/      local stdlib, headers, compiler copy, and nested-shell rc
scripts/   activate this project in your current terminal
bin/       built programs plus sere-path (puts the compiler on PATH)

Stay in this terminal (recommended):
  . ./scripts/activate           bash
  . .\scripts\activate.ps1      PowerShell
  call scripts\activate.bat     cmd
  deactivate                     restore PATH and this prompt

Put the compiler on PATH (this machine):
  .\bin\sere-path.ps1
  .\bin\sere-path.ps1 -Persistent

Then:
  sere build
  sere run
  sere run -- arg1 arg2
  sere clean

Set native = true in sere.toml to auto-build libs/native on sere build.
Drop a packed .slib (or .sere) into libs/ and `import` it.
Any .sere file dropped in venv/stdlib is importable.
prelude.sere is imported automatically.
