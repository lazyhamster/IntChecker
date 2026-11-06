# Building IntChecker for Windows ARM64

Use a POSIX shell such as WSL, GNU M4, and LLVM-MinGW with the
`aarch64-w64-mingw32-` tools on `PATH`. The script builds the bundled RHash
sources for ARM64 and generates the plugin resources from the M4 templates.

```sh
sh build-arm64.sh
```

The DLL and its resources are written to `build-arm64/IntChecker`. Copy
that directory into the `Plugins` directory of an ARM64 Far Manager
installation and start a new Far session.

Set `BUILD_DIR` to choose another absolute output directory, `M4` to select
the GNU M4 executable, or `CROSS_PREFIX` to override the compiler prefix.

See `LICENSE.txt` for IntChecker's license. The bundled RHash sources
retain their license notices.
