# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```sh
sh bootstrap.sh                   # Bootstrap: compile Bob with system cc -> .bootstrap/bob
DEBUG=1 SUDO .bootstrap/bob install  # Install bootstrapped Bob
sh tests.sh                       # Full test suite (bootstraps + installs + runs all tests)
sh tests/<name>.sh                # Run single test (requires bob already installed)
bob build                         # Build with installed Bob
bob run build                     # Build Bob with Bob (self-hosting)
```

Tests require `bob` in PATH (installed via `tests.sh` or manually). Individual tests source `tests/common.sh` which sets `BOB_TARGET=bob-testing-target`, `BOB_DEPS_PATH=$(pwd)/.deps`, and defines `SUDO`.

## Architecture

Bob is a universal build orchestrator. It detects which build system a project uses and delegates to it. Projects configure builds via Flamingo (`build.fl`), a custom DSL with a tree-sitter parser + interpreter embedded in `src/flamingo/`.

### Build system detection (`src/bsys.*`)

`bsys_identify()` scans for marker files (`build.fl`, `meson.build`, `CMakeLists.txt`, `Cargo.toml`, etc.) and returns a `bsys_t*`. Each `src/bsys/*/main.c` implements this interface: `identify`, `setup`, `dep_tree`, `build_deps`, `build`, `install`, `run`, `clean`.

Supported: Bob (Flamingo), Meson, CMake, GNU Make, Configure, Cargo, Go, FreeBSD ports.

### Flamingo classes (`src/class/`)

Only used by the Bob/Flamingo bsys. These are runtime classes exposed to `build.fl` scripts: `Cc` (C compiler), `Linker`, `Fs`, `Platform`, `PkgConfig`, `Cargo`, `Go`. Bob's own `build.fl` uses these.

### Dependency tree (`src/deps.h`, `src/dep_tree.c`)

Flamingo `build.fl` files declare `deps` (local paths or git URLs). `get_deps_tree()` resolves them into `dep_node_t` trees, detects circular deps via `path_hashes`, serializes to disk (`deps.tree` + `deps.hash`). Deps cache lives in `~/.cache/bob/deps` or `$BOB_DEPS_PATH`.

### Incremental builds

Two mechanisms work together:
- **Cookies** (`src/cookie.c`): Per-artifact cache markers. Path: `{bsys_out_path}/{path_hash}.cookie.{str_hash}.{ext}`. Thread-safe registry prevents duplicate builds.
- **Frugal** (`src/frugal.c`): Three checks — flag changes (`.flags` files), mtime comparison, link dependency changes (resolves `-lXXX` to actual `.a` paths).

### Output layout

```
.bob/
  {target}/          # e.g. x86_64-Linux
    version          # BUILD_ID = VERSION + compile timestamp; cleared = full rebuild
    bob/             # bsys_out_path for Flamingo builds
      *.o, *.cookie* # object files and cookies
    prefix/          # temp install prefix
      bin/, lib/, include/, share/
```

`BUILD_ID` (defined in `src/common.h`) combines `VERSION` + `__DATE__` + `__TIME__`. On mismatch with `.bob/{target}/version`, the output dir and deps cache are cleared. The deps cache has its own `version` file so it invalidates independently across projects.

### Parallelism

`src/build_step.c`: Flamingo classes schedule work via `add_build_step()` then `run_build_steps()` runs them in parallel (thread pool sized by `-j` / ncpu).

### Logging

`LOG_FATAL`, `LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, `LOG_SUCCESS` macros in `src/logging.h`. Colour controlled by `COLORTERM`, `TERM`, and `CLICOLOR_FORCE` (force if set and `!= "0"`). Init with `logging_init()`.

### Key globals (`src/common.h`)

`abs_out_path`, `targetless_out_path`, `bsys_out_path`, `deps_path`, `install_prefix`, `instr`, `debugging`, `force_dep_tree_rebuild`.
