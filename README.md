# denvm

NVM bytecode decompiler. Reads NVM binary files produced by the NovariaOS Virtual Machine (NVM) and outputs human-readable disassembly with cross-reference labels, inline annotations, and optional hex display.

## Building

Requires a C11 compiler and [Meson](https://mesonbuild.com/) ≥ 0.55.

```sh
meson setup builddir
ninja -C builddir
```

The `denvm` binary is placed in `builddir/`.

## Usage

```
denvm [options] <file.nvm>
```

**Options:**

| Flag | Description |
|------|-------------|
| `-h`, `--help` | Show help |
| `-V`, `--version` | Show version |
| `-x`, `--hex` | Show raw bytes alongside mnemonics |
| `-d`, `--dump` | Hex dump of the entire file |
| `-n`, `--no-color` | Disable ANSI color output |
| `--no-comments` | Disable inline comments |
| `--no-offsets` | Hide byte offsets |
| `-o <file>` | Write output to file instead of stdout |

## NVM binary format

NVM files start with the 4-byte magic `NVM0` (`4E 56 4D 30`). Bytecode begins at offset `0x04`. All multi-byte operands are big-endian.

| Group | Opcodes |
|-------|---------|
| Stack | `halt` `nop` `push` `pop` `dup` `swap` |
| Arithmetic | `add` `sub` `mul` `div` `mod` |
| Comparison | `cmp` `eq` `neq` `gt` `lt` |
| Control flow | `jmp` `jz` `jnz` `call` `ret` |
| Frame | `enter` `leave` `load_arg` `store_arg` |
| Memory | `load` `store` `load_rel` `store_rel` `load_abs` `store_abs` |
| System | `syscall` `break` |

Unknown opcodes are displayed as `.db 0xXX`.

## Running tests

```sh
meson test -C builddir
```

Tests cover instruction decoding, cross-reference analysis, and binary loading. Individual test binaries are in `builddir/tests/`.

## License

Apache License 2.0.
