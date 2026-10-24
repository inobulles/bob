# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is Bob

Bob is a universal build system orchestrator written in C11. It detects the build system of a project (Bob/Flamingo, Meson, CMake, Autotools, GNU Make, Cargo, Go, FreeBSD ports) and manages building, installing, and running it. Bob uses **Flamingo** (a fully-fledged scripting language) for native Bob projects. Flamingo's source is vendored into `src/flamingo/` so Bob can bootstrap with only a C compiler; update it via `scripts/update-flamingo.sh`.

## Commands

### Bootstrap (first build)
```sh
sh bootstrap.sh       # builds Bob using system cc into .bootstrap/
.bootstrap/bob install
```

### Self-hosted build
```sh
bob build
bob install
bob run
bob clean
```

### Tests
```sh
sh tests.sh           # runs all tests in tests/*.sh
```

## Architecture

### Build system plugin pattern (`src/bsys/`)

Each backend exposes a `bsys_t` struct (`src/bsys.h`) with function pointers: `identify`, `setup`, `dep_tree`, `build_deps`, `build`, `install`, `run`, `clean`, `destroy`. Detection happens in order in `src/bsys.c` — Bob/Flamingo first, then Meson, CMake, etc.

### Flamingo language (`src/flamingo/`)

Flamingo is a scripting language vendored into `src/flamingo/` for zero-dependency bootstrapping — don't edit it directly, update via `scripts/update-flamingo.sh`. Built-in Bob classes (Cc, Linker, Fs, Platform, Cargo, Go, PkgConfig) are in `src/class/`.

### Dependency graph (`src/dep_tree.c`, `src/deps.h`)

Builds a DAG of project dependencies, detects cycles, downloads/clones as needed, serializes for caching. Dependencies are stored at `~/.cache/bob/deps` (or `BOB_DEPS_PATH`).

### Frugal incremental builds (`src/frugal.c`)

Tracks compiler flags (`.flags` files) and file mtimes to skip unnecessary recompilation. A `BUILD_ID` (version + compile timestamp) in `src/common.h` invalidates the entire output cache when Bob itself is recompiled.

### Command execution (`src/cmd.c`, `src/pool.c`)

Async command execution via a worker pool.

### Global state (`src/common.h`)

Key globals: `instr` (current instruction), `abs_out_path` (.bob/$TARGET/), `install_prefix`, `deps_path`, `debugging`.

`BOB_BUILD_DEBUGGING=1` sets `debugging=true`: forces `-j1` (no parallelism) and disables output pipe buffering so command output streams in real-time. Use when diagnosing hangs or other unobvious build failures.

## Code conventions

These are enforced by CI (`.github/workflows/lint.yml`) over all of `src/` except `src/flamingo/`:

- **clang-tidy** must pass (run on a compilation database generated with `intercept-build sh bootstrap.sh`).
- **Allocations**: use the checked wrappers from `src/alloc.h`, which assert the result is non-NULL. Never call the raw libc functions directly.
  - `malloc_c`, `calloc_c`, `realloc_c` instead of `malloc`, `calloc`, `realloc`.
  - `strdup_c`, `strndup_c` instead of `strdup`, `strndup`.
  - `asprintf_c`, `vasprintf_c` instead of `asprintf`, `vasprintf` (these return `void`, so there's no return value to check).
- **Paths**: use `realerpath()` (`src/fsutil.h`) instead of `realpath(3)`. It also expands a leading `~` to `$HOME`. The result is heap-allocated and may be NULL.
- **Directories**: use `mkdir_wrapped()` (`src/fsutil.h`) instead of `mkdir(2)`. It treats `EEXIST` as success and fixes ownership of newly-created directories via `set_owner()`.
- **`cmd_set_redirect()`**: never pass raw `true`/`false`; use the `CMD_REDIRECT`/`CMD_NO_REDIRECT` and `CMD_FORCE_REDIRECT`/`CMD_NO_FORCE_REDIRECT` enum values from `src/cmd.h`.

## CLI

```
bob [-p prefix] [-D] [-O] [-C dir] [-o outdir] <instruction>
  -p   install prefix (default /usr/local)
  -D   disable dependency building
  -O   use own prefix (not default .bob/<target>/prefix temporary prefix)
  -C   project directory
  -o   output directory
  -f   force dependency tree rebuild
```

Instructions: `build`, `run`, `sh`, `install`, `clean`, `dep-tree`.

## Flamingo build config (`build.fl`)

```flamingo
let src = Fs.list("src").where(|path| path.endswith(".c"))
let obj = Cc(["-std=c11", "-Wall", "-Isrc"]).compile(src)
let cmd = Linker(["-lm", "-lpthread"]).link(obj)

install = { cmd: "bin/bob", "import/bob.fl": "share/flamingo/import/bob.fl" }
run = ["bob"]
```
