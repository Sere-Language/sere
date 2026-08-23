# Nested `sere shell` only. Does not load ~/.bashrc.
# PATH and SERE_* are already set by the parent `sere` process.
PS1="(sere:${SERE_PROJECT_NAME}) \w \$ "
deactivate() { echo "Leaving nested Sere shell."; exit; }
