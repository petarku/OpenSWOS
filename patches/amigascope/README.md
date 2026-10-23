# AmigaScope patch not (yet) upstream

`0001-gdb_proxy-env-overrides-native-Windows.patch` is the5ar's AmigaScope commit `8df9712a`
(2026-10-23), which never reached github.com/benbaker76/AmigaScope. The flow-testing framework
(`scripts/swos_test.py`, `scripts/testing_support/`) was written against it: it lets the AmigaScope
daemon run under Cygwin against a native-Windows FS-UAE + m68k gdb.

- `AMIGASCOPE_GDB` overrides the hard-coded m68k gdb path.
- `AMIGASCOPE_ELF_NOARG=1` starts gdb without the ELF (AmigaDOS relocates hunks: attach first,
  then `add-symbol-file` at the bases `qOffsets` reports).
- A reader-thread race fix in `GdbDaemon`.

The submodule pointer stays on upstream's commit, so a fresh clone works. To use the framework
on Windows, apply it in the submodule:

    git -C scripts/amiga/amigascope am ../../../patches/amigascope/0001-gdb_proxy-env-overrides-native-Windows.patch

Better: send it to AmigaScope as a pull request, then drop this folder.
