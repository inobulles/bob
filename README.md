# bob

Say hello to Bob the builder! 👷

Bob will help you build all sorts of different projects, as he's the buildsystem for the AQUA ecosystem!

[https://www.youtube.com/watch?v=0ldh_Cw6W0c](https://www.youtube.com/watch?v=0ldh_Cw6W0c)

## Installation

If you don't already have Bob on your system, you can bootstrap him as such:

```console
sh build.sh
```

Then, you can install him to your system with:

```console
sh-bin/bob install
```

## Usage

### Building

Bob always loves to give a helping hand! 🤝

To build a project with Bob, navigate to its root directory and run:

```console
bob build
```

### Running

Bob's favourite pastime is running! 🏃

To run a project with Bob, run:

```console
bob run
```

### Testing

Bob is a bona fide science guy in a labcoat! 🧪

To test a project with Bob, run:

```console
bob test
```

### Installing

Bob always helps his grandma install [Linux Mint](https://linuxmint.com/) on her computer! 👵

To install a project with Bob, run:

```console
bob install
```

### Creating a new project

Bob is eternal... 💀

To create and run a new project from a skeleton, run:

```console
bob skeleton c project
cd project
bob run
```

### Creating an LSP configuration

Bob isn't one of those idiots who says "hurr durr language survurs r fur soydevs, _real_ programmurs don't need cuz never make mistakes" (based on true events).
Bob is smart; he knows good tooling makes him a better, more productive developer! 🛠

To generate an LSP configuration for your project, run:

```console
bob lsp
```

### Packaging

Bob used to work at an Amazon distribution centre! 📦

To create a ZPK package, run:

```console
bob package zpk
```

## Projects which use Bob

Here's a list of projects which use Bob:

- [Bob the Builder](https://github.com/inobulles/bob)
- [IAR](https://github.com/inobulles/iar)
- [Umber](https://github.com/inobulles/umber)
- [aquaBSD Aquariums](https://github.com/inobulles/aquarium)
- [`aqua-unix`](https://github.com/inobulles/aqua-unix)
- [`aqua-kos`](https://github.com/inobulles/aqua-kos)
- [`aqua-devices`](https://github.com/inobulles/aqua-devices)
- [LLN Gamejam 2023](https://github.com/obiwac/lln-gamejam-2023)
- [iface](https://github.com/inobulles/iface)
- [`libmkfs_msdos`](https://github.com/inobulles/libmkfs_msdos)
- [`libcopyfile`](https://github.com/inobulles/libcopyfile)

I use this list to not forget to update them when I add a new feature to Bob!

## Features

- [x] Written in C and with a very basic project structure, so can easily be bootstrapped.
- [x] Automatically installs (certain) dependencies.
- [x] Uses [Wren](https://wren.io/) as a dynamic configuration language because regular configuration files sucks.
- [x] Centralized system for building, testing, and installing.
- [ ] Logging class (like with `Log.error`, `Log.warn`) to provide feedback from within build configurations.
- [ ] Works with AQUA, i.e. AQUA projects may be built and run just as easily as any other project using Bob.
- [x] Easy method for building out a project skeleton from a template (serves the same purpose as [`aqua-manager`](https://github.com/inobulles/aqua-manager) did).
- [x] Packaging (AQUA ZPK & FreeBSD PKG formats).
- [ ] Understands other popular buildsystems (simple makefile, cmake, autoconf, qmake, setup.py, &c) to eliminate tmw you have to google stuff when you just want something to compile 🤪
- [ ] Watch source files to automatically rebuild them.
- [ ] Can build in sandboxes (local CI in background truly continuously by watching source files instead of just on commits? integration with other CI providers, e.g. with Cirrus CI by Automatically generating `.cirrus.yml`?).
- [ ] Proper documentation not only on how to use bob, but also on how to write a `build.wren` configuration.
- [ ] Warn against two commands outputting to the same file.
- [ ] Integration with `clangd`? Like we could automatically generate a `.clangd` file with the options passed to the C compiler & linker.
- [ ] If child execution fails from e.g. a segfault, say so (this is for `run`).

## Personal notes

Disregard these, these are personal notes for development.

- wren is okay with `wrenGetListCount` on a `*Sequence`, which happens because `IS_LIST` on a sequence value returns true (it shouldn't right? because that breaks things when doing `AS_LIST` on it to get a `ValueBuffer` afterwards...)
- the same problem as above seems to happen with foreign type slots
