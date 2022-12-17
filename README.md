# bob

Bob the builder (yes I know, I currently already have a project called this - the old project will be completely merged into the aquarium system, and this one will be the "definitive" bob).
Buildsystem for the AQUA ecosystem.

## Installation

If you don't already have Bob on your system, you can bootstrap it as such:

```console
sh build.sh
```

Then, you can install it to your system with:

```console
sh-bin/bob install
```

## Usage

### Building

To build a project with Bob, navigate to its root directory and run:

```console
bob build
```

### Running

To run a project with Bob, run:

```console
bob run
```

### Testing

To test a project with Bob, run:

```console
bob test
```

### Installing

To install a project with Bob, run:

```console
bob install
```

## Features

- [x] Written in C and with a very basic project structure, so can easily be bootstrapped.
- [x] Automatically installs (certain) dependencies.
- [x] Uses [Wren](https://wren.io/) as a dynamic configuration language because regular configuration files sucks.
- [x] Centralised system for building, testing, and installing.
- [ ] Logging class (like with `Log.error`, `Log.warn`) to provide feedback from within build configurations.
- [ ] Works with AQUA, i.e. AQUA projects may be built and run just as easily as any other project using Bob.
- [ ] Easy method for building out a project skeleton from a template (serves the same purpose as [`aqua-manager`](https://github.com/inobulles/aqua-manager) did).
- [ ] Packaging (AQUA ZPK & FreeBSD PKG formats).
- [ ] Understands other popular buildsystems (simple makefile, cmake, autoconf, qmake, setup.py, &c) to eliminate tmw you have to google stuff when you just want something to compile 🤪
- [ ] Watch source files to automatically rebuild them.
- [ ] Can build in sandboxes (local CI in background truly continuously by watching source files instead of just on commits? integration with other CI providers, e.g. with Cirrus CI by Automatically generating `.cirrus.yml`?).
- [ ] Proper documentation not only on how to use bob, but also on how to write a `build.wren` configuration.
- [ ] Warn against two commands outputting to the same file.
- [ ] Integration with `clangd`? Like we could automatically generate a `.clangd` file with the options passed to the C compiler & linker.

## Personal notes

Disregard these, these are personal notes for development.

- wren is okay with `wrenGetListCount` on a `*Sequence`, which happens because `IS_LIST` on a sequence value returns true (it shouldn't right? because that breaks things when doing `AS_LIST` on it to get a `ValueBuffer` afterwards...)
- the same problem as above seems to happen with foreign type slots
